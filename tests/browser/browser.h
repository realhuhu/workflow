#ifndef WORKFLOW_TESTS_BROWSER_BROWSER_H
#define WORKFLOW_TESTS_BROWSER_BROWSER_H

#include <QMainWindow>

#include <atomic>
#include <memory>

class Emitter;
class OcrEngine;
class QCloseEvent;
class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;
class QThread;
class QWebEngineView;

void configureDeterministicBrowserProcess();

class FixtureBrowserWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit FixtureBrowserWindow(QWidget* parent = nullptr);
    ~FixtureBrowserWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWebEngineView* browser{};
    QListWidget* caseList{};
    QLabel* caseTitle{};
    QLabel* caseDetails{};
    QLabel* environmentStatus{};
    QLabel* runStatus{};
    QPushButton* runButton{};
    QPushButton* resetButton{};
    QPushButton* stopButton{};
    QPushButton* indexButton{};
    QTextEdit* output{};

    Emitter* emitter{};
    std::unique_ptr<OcrEngine> ocr;
    std::atomic_bool stopFlag{false};
    QThread* worker{};
    QString webRoot;
    QString previousLog;
    int currentCaseId = 0;
    bool pageReady = false;
    bool ocrReady = false;

    void buildInterface();
    void configureBrowser();
    void prepareFixtureImages() const;
    void loadIndex();
    void loadCase(int id);
    void synchronizeCaseFromUrl();
    void updateCaseDetails();
    void verifyPageEnvironment();
    void resetCurrentCase();
    void runCurrentCase();
    void startCaseWorker(int id);
    void finishCaseWorker(int id, bool success, const QString& message);
    void stopCurrentCase();
    void setRunState(const QString& state, const QString& text);
    void updateControls();
    void appendLog(const QString& text, const QString& color = QStringLiteral("black"));
};

#endif // WORKFLOW_TESTS_BROWSER_BROWSER_H
