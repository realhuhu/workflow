#include "window.h"

#include "cases.h"
#include "core/environment.h"
#include "support/emitter.h"
#include "support/ocr.h"

#include <windows.h>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyleFactory>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>

namespace {

    constexpr int PageWidth = 1000;
    constexpr int PageHeight = 800;
    constexpr int PanelWidth = 390;

    struct RunOutcome {
        bool success = false;
        QString message;
    };

    const TestCase* findCase(
        const int id
    ) {
        for (const TestCase& definition : testCases()) {
            if (definition.id == id) return &definition;
        }
        return nullptr;
    }

    QString mappedColor(
        const QString& color
    ) {
        if (color.compare(QStringLiteral("green"), Qt::CaseInsensitive) == 0) return QStringLiteral("#70e1a1");
        if (color.compare(QStringLiteral("red"), Qt::CaseInsensitive) == 0) return QStringLiteral("#ff6b6b");
        if (color.compare(QStringLiteral("blue"), Qt::CaseInsensitive) == 0) return QStringLiteral("#65b7ff");
        if (color.compare(QStringLiteral("orange"), Qt::CaseInsensitive) == 0) return QStringLiteral("#ffb454");
        return QStringLiteral("#bac4c9");
    }

    void paintMarkerA(
        QPainter& painter,
        const bool variant
    ) {
        painter.fillRect(0, 0, 48, 48, QColor("#f2c94c"));
        painter.fillRect(4, 4, 40, 40, QColor("#0a1118"));
        painter.fillRect(20, 8, 8, 32, QColor("#27d7e6"));
        painter.fillRect(8, 20, 32, 8, QColor("#27d7e6"));
        painter.fillRect(8, 8, 6, 6, QColor("#ff5d5d"));
        painter.fillRect(34, 34, 6, 6, QColor("#ff5d5d"));
        if (variant) {
            painter.fillRect(18, 18, 12, 12, QColor("#f05cff"));
            painter.fillRect(34, 8, 6, 6, QColor("#f05cff"));
        } else {
            painter.fillRect(22, 22, 4, 4, QColor("#f7f3e8"));
        }
    }

    void paintMarkerB(
        QPainter& painter
    ) {
        painter.fillRect(0, 0, 48, 48, QColor("#e8f1f2"));
        painter.fillRect(4, 4, 40, 40, QColor("#0d3b66"));
        for (int y = 8; y < 40; y += 8) {
            for (int x = 8; x < 40; x += 8) {
                if (((x + y) / 8) % 2 == 0) painter.fillRect(x, y, 8, 8, QColor("#2ec4b6"));
            }
        }
        painter.fillRect(18, 18, 12, 12, QColor("#ffcf56"));
    }

    void paintMarkerC(
        QPainter& painter
    ) {
        painter.fillRect(0, 0, 48, 48, QColor("#111820"));
        painter.fillRect(4, 4, 40, 40, QColor("#ff6b35"));
        painter.fillRect(8, 8, 8, 32, QColor("#f7f3e8"));
        painter.fillRect(16, 16, 8, 24, QColor("#f7f3e8"));
        painter.fillRect(24, 24, 8, 16, QColor("#f7f3e8"));
        painter.fillRect(32, 32, 8, 8, QColor("#f7f3e8"));
        painter.fillRect(32, 8, 8, 8, QColor("#2b59c3"));
    }

    void saveMarker(
        const QString& path,
        const std::function<void(QPainter&)>& renderer
    ) {
        QImage image(48, 48, QImage::Format_RGB32);
        image.fill(QColor("#000000"));
        image.setDotsPerMeterX(3780);
        image.setDotsPerMeterY(3780);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        renderer(painter);
        painter.end();
        if (!image.save(path, "PNG", 0)) {
            throw std::runtime_error("无法生成测试模板: " + path.toStdString());
        }
    }

} // namespace

void configureDeterministicBrowserProcess() {
    SetProcessDPIAware();
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");

    QByteArray chromiumFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!chromiumFlags.isEmpty()) chromiumFlags.append(' ');
    chromiumFlags.append(
        "--force-device-scale-factor=1 "
        "--force-color-profile=srgb "
        "--disable-background-timer-throttling "
        "--disable-renderer-backgrounding "
        "--disable-backgrounding-occluded-windows "
        "--disable-features=CalculateNativeWinOcclusion,TranslateUI"
    );
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlags);

    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi);
}

TestWindow::TestWindow(
    QWidget* parent
) : QMainWindow(parent), emitter(new Emitter(this)) {
    setWindowTitle(QStringLiteral("Workflow · 自动化校准台"));
    setObjectName(QStringLiteral("fixtureWindow"));
    webRoot = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test-page"));

    if (!QFileInfo::exists(QDir(webRoot).filePath(QStringLiteral("index.html")))) {
        throw std::runtime_error("测试页面未部署到: " + webRoot.toStdString());
    }
    prepareFixtureImages();
    buildInterface();
    configureBrowser();

    connect(emitter, &Emitter::log, this, [this](const QString& text, const QString& color) {
        appendLog(text, color);
    });
    connect(emitter, &Emitter::error, this, [this](const QString& text) { appendLog(text, QStringLiteral("red")); });

    const QString models = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("models"));
    ocr = std::make_unique<OcrEngine>(models);
    QString ocrError;
    ocrReady = ocr->initialize(&ocrError);
    if (ocrReady) {
        appendLog(QStringLiteral("RapidOCR 已就绪：%1").arg(models), QStringLiteral("green"));
    } else {
        appendLog(QStringLiteral("RapidOCR 初始化失败：%1").arg(ocrError), QStringLiteral("red"));
    }

    loadIndex();
    setFixedSize(PageWidth + PanelWidth, PageHeight);
}

TestWindow::~TestWindow() {
    stopFlag.store(true);
    if (worker && worker->isRunning()) worker->wait();
}

void TestWindow::buildInterface() {
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    browser = new QWebEngineView(central);
    browser->setObjectName(QStringLiteral("fixtureBrowser"));
    browser->setFixedSize(PageWidth, PageHeight);
    browser->setContextMenuPolicy(Qt::NoContextMenu);
    browser->setZoomFactor(1.0);
    browser->setAttribute(Qt::WA_NativeWindow);
    rootLayout->addWidget(browser);

    auto* panel = new QWidget(central);
    panel->setObjectName(QStringLiteral("controlPanel"));
    panel->setFixedSize(PanelWidth, PageHeight);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(20, 18, 20, 18);
    panelLayout->setSpacing(10);

    auto* eyebrow = new QLabel(QStringLiteral("WORKFLOW / VISUAL LAB"), panel);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    panelLayout->addWidget(eyebrow);

    auto* heading = new QLabel(QStringLiteral("自动化校准台"), panel);
    heading->setObjectName(QStringLiteral("heading"));
    panelLayout->addWidget(heading);

    auto* environmentRow = new QHBoxLayout();
    environmentStatus = new QLabel(QStringLiteral("等待页面校准"), panel);
    environmentStatus->setObjectName(QStringLiteral("environmentStatus"));
    runStatus = new QLabel(QStringLiteral("IDLE"), panel);
    runStatus->setObjectName(QStringLiteral("runStatus"));
    environmentRow->addWidget(environmentStatus, 1);
    environmentRow->addWidget(runStatus);
    panelLayout->addLayout(environmentRow);

    caseList = new QListWidget(panel);
    caseList->setObjectName(QStringLiteral("caseList"));
    caseList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const TestCase& definition : testCases()) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(definition.id, 2, 10, QLatin1Char('0')).arg(definition.title),
            caseList
        );
        item->setData(Qt::UserRole, definition.id);
        item->setToolTip(definition.capability);
    }
    panelLayout->addWidget(caseList, 4);

    caseTitle = new QLabel(QStringLiteral("选择一个测试用例"), panel);
    caseTitle->setObjectName(QStringLiteral("caseTitle"));
    caseDetails = new QLabel(QStringLiteral("左侧索引包含 20 个独立二级页面。"), panel);
    caseDetails->setObjectName(QStringLiteral("caseDetails"));
    caseDetails->setWordWrap(true);
    panelLayout->addWidget(caseTitle);
    panelLayout->addWidget(caseDetails);

    auto* primaryButtons = new QHBoxLayout();
    runButton = new QPushButton(QStringLiteral("运行当前用例"), panel);
    runButton->setObjectName(QStringLiteral("runButton"));
    resetButton = new QPushButton(QStringLiteral("重置"), panel);
    resetButton->setObjectName(QStringLiteral("secondaryButton"));
    primaryButtons->addWidget(runButton, 1);
    primaryButtons->addWidget(resetButton);
    panelLayout->addLayout(primaryButtons);

    auto* secondaryButtons = new QHBoxLayout();
    stopButton = new QPushButton(QStringLiteral("停止"), panel);
    stopButton->setObjectName(QStringLiteral("dangerButton"));
    indexButton = new QPushButton(QStringLiteral("用例索引"), panel);
    indexButton->setObjectName(QStringLiteral("secondaryButton"));
    secondaryButtons->addWidget(stopButton);
    secondaryButtons->addWidget(indexButton);
    panelLayout->addLayout(secondaryButtons);

    output = new QTextEdit(panel);
    output->setObjectName(QStringLiteral("output"));
    output->setReadOnly(true);
    output->setAcceptRichText(true);
    panelLayout->addWidget(output, 3);

    rootLayout->addWidget(panel);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(R"(
        #fixtureWindow, #central, #controlPanel { background: #111820; }
        #controlPanel { border-left: 1px solid #29333a; }
        QLabel { color: #dbe2e5; font-family: "Microsoft YaHei UI"; }
        #eyebrow { color: #f2c94c; font: 700 11px "Consolas"; letter-spacing: 2px; }
        #heading { color: #f7f3e8; font: 700 26px "Microsoft YaHei UI"; }
        #environmentStatus { color: #8fa2aa; font: 12px "Consolas"; }
        #runStatus { color: #111820; background: #8fa2aa; border-radius: 3px; padding: 3px 8px; font: 700 11px "Consolas"; }
        #runStatus[state="ready"] { background: #27d7e6; }
        #runStatus[state="running"] { background: #f2c94c; }
        #runStatus[state="pass"] { background: #70e1a1; }
        #runStatus[state="fail"] { background: #ff6b6b; }
        #caseList { background: #0b1117; color: #aebcc2; border: 1px solid #29333a; outline: none; font: 12px "Microsoft YaHei UI"; }
        #caseList::item { min-height: 27px; padding: 2px 8px; border-bottom: 1px solid #182127; }
        #caseList::item:selected { color: #111820; background: #f2c94c; }
        #caseList::item:hover:!selected { background: #18232a; color: #f7f3e8; }
        #caseTitle { color: #f7f3e8; font: 700 15px "Microsoft YaHei UI"; }
        #caseDetails { color: #8fa2aa; font: 12px "Microsoft YaHei UI"; }
        QPushButton { min-height: 34px; padding: 0 12px; border: 1px solid #35434b; border-radius: 3px; background: #18232a; color: #dbe2e5; font: 700 12px "Microsoft YaHei UI"; }
        QPushButton:hover:enabled { border-color: #f2c94c; color: #f2c94c; }
        QPushButton:disabled { color: #53626a; border-color: #263138; background: #141c21; }
        #runButton { background: #f2c94c; color: #111820; border-color: #f2c94c; }
        #runButton:hover:enabled { background: #ffe17a; color: #111820; }
        #dangerButton:hover:enabled { color: #ff6b6b; border-color: #ff6b6b; }
        #output { background: #0a0f14; color: #bac4c9; border: 1px solid #29333a; padding: 6px; font: 11px "Consolas"; }
        QScrollBar:vertical { width: 9px; background: #0b1117; }
        QScrollBar::handle:vertical { min-height: 24px; background: #35434b; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));

    connect(caseList, &QListWidget::itemSelectionChanged, this, [this] {
        if (worker) return;
        const QList<QListWidgetItem*> selected = caseList->selectedItems();
        if (selected.isEmpty()) return;
        loadCase(selected.front()->data(Qt::UserRole).toInt());
    });
    connect(runButton, &QPushButton::clicked, this, &TestWindow::runCurrentCase);
    connect(resetButton, &QPushButton::clicked, this, &TestWindow::resetCurrentCase);
    connect(stopButton, &QPushButton::clicked, this, &TestWindow::stopCurrentCase);
    connect(indexButton, &QPushButton::clicked, this, &TestWindow::loadIndex);
    connect(browser, &QWebEngineView::loadFinished, this, [this](const bool success) {
        pageReady = success;
        browser->setZoomFactor(1.0);
        if (success) verifyPageEnvironment();
        else {
            environmentStatus->setText(QStringLiteral("页面加载失败"));
            setRunState(QStringLiteral("fail"), QStringLiteral("LOAD FAIL"));
        }
        updateControls();
    });
    connect(browser, &QWebEngineView::urlChanged, this, [this] { synchronizeCaseFromUrl(); });

    updateControls();
}

void TestWindow::configureBrowser() {
    QWebEngineProfile* profile = browser->page()->profile();
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile->clearHttpCache();

    QWebEngineSettings* settings = browser->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
}

void TestWindow::prepareFixtureImages() const {
    const QString fixtureDirectory = QDir(webRoot).filePath(QStringLiteral("游戏图片/fixtures"));
    if (!QDir().mkpath(fixtureDirectory)) {
        throw std::runtime_error("无法创建浏览器模板目录: " + fixtureDirectory.toStdString());
    }

    saveMarker(QDir(fixtureDirectory).filePath(QStringLiteral("marker-a.png")), [](QPainter& painter) {
        paintMarkerA(painter, false);
    });
    saveMarker(QDir(fixtureDirectory).filePath(QStringLiteral("marker-a-variant.png")), [](QPainter& painter) {
        paintMarkerA(painter, true);
    });
    saveMarker(QDir(fixtureDirectory).filePath(QStringLiteral("marker-b.png")), paintMarkerB);
    saveMarker(QDir(fixtureDirectory).filePath(QStringLiteral("marker-c.png")), paintMarkerC);
}

void TestWindow::loadIndex() {
    if (worker) return;
    currentCaseId = 0;
    pageReady = false;
    {
        const QSignalBlocker blocker(caseList);
        caseList->clearSelection();
        caseList->setCurrentRow(-1);
    }
    caseTitle->setText(QStringLiteral("20 个确定性测试页面"));
    caseDetails->setText(QStringLiteral("从索引或右侧列表选择用例；网页区域固定为 1000 × 800 CSS px。"));
    setRunState(QStringLiteral("ready"), QStringLiteral("INDEX"));
    browser->load(QUrl::fromLocalFile(QDir(webRoot).filePath(QStringLiteral("index.html"))));
    updateControls();
}

void TestWindow::loadCase(
    const int id
) {
    if (worker) return;
    const TestCase* definition = findCase(id);
    if (!definition) return;

    currentCaseId = id;
    pageReady = false;
    updateCaseDetails();
    setRunState(QStringLiteral("ready"), QStringLiteral("LOADING"));
    const QString fileName = QStringLiteral("%1-%2.html").arg(id, 2, 10, QLatin1Char('0')).arg(definition->slug);
    browser->load(QUrl::fromLocalFile(QDir(webRoot).filePath(QStringLiteral("cases/%1").arg(fileName))));
    updateControls();
}

void TestWindow::synchronizeCaseFromUrl() {
    const QString fileName = browser->url().fileName();
    for (const TestCase& definition : testCases()) {
        const QString expected =
            QStringLiteral("%1-%2.html").arg(definition.id, 2, 10, QLatin1Char('0')).arg(definition.slug);
        if (fileName != expected) continue;
        currentCaseId = definition.id;
        const QSignalBlocker blocker(caseList);
        caseList->setCurrentRow(definition.id - 1);
        updateCaseDetails();
        return;
    }
    if (fileName == QStringLiteral("index.html")) currentCaseId = 0;
}

void TestWindow::updateCaseDetails() {
    const TestCase* definition = findCase(currentCaseId);
    if (!definition) return;
    caseTitle->setText(QStringLiteral("%1 · %2").arg(definition->id, 2, 10, QLatin1Char('0')).arg(definition->title));
    caseDetails->setText(
        QStringLiteral("%1\n目标：%2 · 期望：%3").arg(definition->description, definition->target, definition->expected)
    );
}

void TestWindow::verifyPageEnvironment() {
    browser->page()->runJavaScript(
        QStringLiteral("window.workflowFixture ? window.workflowFixture.environment() : null"),
        [this](const QVariant& value) {
            const QVariantMap result = value.toMap();
            const bool valid = result.value(QStringLiteral("ok")).toBool();
            const double dpr = result.value(QStringLiteral("dpr")).toDouble();
            const int width = result.value(QStringLiteral("width")).toInt();
            const int height = result.value(QStringLiteral("height")).toInt();
            if (valid) {
                environmentStatus->setText(
                    QStringLiteral("DPR %1 · ZOOM 100% · %2×%3").arg(dpr, 0, 'f', 0).arg(width).arg(height)
                );
                if (currentCaseId > 0) setRunState(QStringLiteral("ready"), QStringLiteral("READY"));
            } else {
                environmentStatus->setText(
                    QStringLiteral("校准失败 · DPR %1 · %2×%3").arg(dpr, 0, 'f', 2).arg(width).arg(height)
                );
                setRunState(QStringLiteral("fail"), QStringLiteral("DPI FAIL"));
            }
            pageReady = pageReady && valid;
            updateControls();
        }
    );
}

void TestWindow::resetCurrentCase() {
    if (!pageReady || currentCaseId == 0 || worker) return;
    browser->page()->runJavaScript(QStringLiteral("window.workflowFixture.reset()"));
    setRunState(QStringLiteral("ready"), QStringLiteral("READY"));
    appendLog(QStringLiteral("用例 %1 已重置").arg(currentCaseId), QStringLiteral("blue"));
}

void TestWindow::runCurrentCase() {
    if (!pageReady || currentCaseId == 0 || worker) return;
    if (!ocrReady && currentCaseId >= 9) {
        QMessageBox::critical(this, QStringLiteral("OCR 不可用"), QStringLiteral("文字用例需要成功初始化 RapidOCR。"));
        return;
    }

    const int id = currentCaseId;
    stopFlag.store(false);
    setRunState(QStringLiteral("running"), QStringLiteral("RUNNING"));
    appendLog(QStringLiteral("—— 运行用例 %1 ——").arg(id, 2, 10, QLatin1Char('0')), QStringLiteral("orange"));
    updateControls();

    browser->page()->runJavaScript(QStringLiteral("window.workflowFixture.reset()"), [this, id](const QVariant&) {
        QTimer::singleShot(80, this, [this, id] {
            if (currentCaseId == id && !worker) startCaseWorker(id);
        });
    });
}

void TestWindow::startCaseWorker(
    const int id
) {
    Env execution;
    execution.hwnd = reinterpret_cast<HWND>(browser->winId());
    execution.pid = GetCurrentProcessId();
    execution.emitter = emitter;
    execution.stopFlag = &stopFlag;
    execution.ocr = ocr.get();
    execution.resourceRoot = webRoot;

    auto outcome = std::make_shared<RunOutcome>();
    QThread* thread = QThread::create([execution, id, outcome] {
        env = execution;
        try {
            runTestCase(id);
            outcome->success = !execution.stopFlag->load();
            outcome->message = outcome->success ? QStringLiteral("工作流执行完成") : QStringLiteral("用户已停止");
        } catch (const std::exception& exception) {
            outcome->message = QString::fromUtf8(exception.what());
        } catch (...) {
            outcome->message = QStringLiteral("未知异常");
        }
    });
    worker = thread;
    connect(thread, &QThread::finished, this, [this, thread, id, outcome] {
        if (worker == thread) worker = nullptr;
        finishCaseWorker(id, outcome->success, outcome->message);
        thread->deleteLater();
        updateControls();
    });
    thread->start();
    updateControls();
}

void TestWindow::finishCaseWorker(
    const int id,
    const bool success,
    const QString& message
) {
    if (!success) {
        setRunState(QStringLiteral("fail"), stopFlag.load() ? QStringLiteral("STOPPED") : QStringLiteral("FAILED"));
        appendLog(message, stopFlag.load() ? QStringLiteral("orange") : QStringLiteral("red"));
        return;
    }

    browser->page()->runJavaScript(
        QStringLiteral("document.body.dataset.result || 'pending'"),
        [this, id, message](const QVariant& value) {
            const QString result = value.toString();
            if (currentCaseId == id && result == QStringLiteral("pass")) {
                setRunState(QStringLiteral("pass"), QStringLiteral("PASS"));
                appendLog(message + QStringLiteral("，页面结果 PASS"), QStringLiteral("green"));
            } else {
                setRunState(QStringLiteral("fail"), QStringLiteral("ASSERT FAIL"));
                appendLog(QStringLiteral("%1，但页面结果为 %2").arg(message, result), QStringLiteral("red"));
            }
        }
    );
}

void TestWindow::stopCurrentCase() {
    if (!worker) return;
    stopFlag.store(true);
    setRunState(QStringLiteral("running"), QStringLiteral("STOPPING"));
    appendLog(QStringLiteral("已请求停止当前工作流"), QStringLiteral("orange"));
    updateControls();
}

void TestWindow::setRunState(
    const QString& state,
    const QString& text
) {
    runStatus->setProperty("state", state);
    runStatus->setText(text);
    runStatus->style()->unpolish(runStatus);
    runStatus->style()->polish(runStatus);
}

void TestWindow::updateControls() {
    const bool running = worker != nullptr;
    runButton->setEnabled(pageReady && currentCaseId > 0 && !running);
    resetButton->setEnabled(pageReady && currentCaseId > 0 && !running);
    stopButton->setEnabled(running && !stopFlag.load());
    indexButton->setEnabled(!running);
    caseList->setEnabled(!running);
}

void TestWindow::appendLog(
    const QString& text,
    const QString& color
) {
    if (text == previousLog) return;
    previousLog = text;
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    output->append(QStringLiteral(
        "<div style=\"white-space:nowrap;\">"
        "<span style=\"color:#091017;background-color:#70e1a1;font-family:Consolas;\">"
        "&nbsp;%1&nbsp;</span>&nbsp;"
        "<span style=\"color:%2;\">%3</span>"
        "</div>"
    )
            .arg(time, mappedColor(color), text));
}

void TestWindow::closeEvent(
    QCloseEvent* event
) {
    stopFlag.store(true);
    if (worker && worker->isRunning() && !worker->wait(15000)) {
        QMessageBox::warning(
            this,
            QStringLiteral("仍在停止"),
            QStringLiteral("截图调用仍在运行，测试台暂时不能安全关闭。")
        );
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}
