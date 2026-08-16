#include "window.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLocale>
#include <QStyleFactory>
#include <QTimer>

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void attachCommandLineOutput() {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (output && output != INVALID_HANDLE_VALUE) {
            SetConsoleOutputCP(CP_UTF8);
            return;
        }
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;

        FILE* stream = nullptr;
        (void)freopen_s(&stream, "CONOUT$", "w", stdout);
        (void)freopen_s(&stream, "CONOUT$", "w", stderr);
        SetConsoleOutputCP(CP_UTF8);
    }

    bool containsCase(
        const int id
    ) {
        return std::any_of(testCases().begin(), testCases().end(), [id](const TestCase& definition) {
            return definition.id == id;
        });
    }

    std::vector<int> parseCaseIds(
        const QString& expression
    ) {
        std::vector<int> result;
        if (expression.trimmed().compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) return result;

        const QStringList parts = expression.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.isEmpty()) throw std::invalid_argument("--cases 不能为空");
        for (const QString& rawPart : parts) {
            const QString part = rawPart.trimmed();
            const qsizetype separator = part.indexOf(QLatin1Char('-'));
            bool startValid = false;
            bool finishValid = false;
            const int start = (separator < 0 ? part : part.left(separator)).toInt(&startValid);
            const int finish = separator < 0 ? start : part.mid(separator + 1).toInt(&finishValid);
            if (separator < 0) finishValid = startValid;
            if (!startValid || !finishValid || start > finish) {
                throw std::invalid_argument("无效的 --cases 范围: " + part.toStdString());
            }
            for (int id = start; id <= finish; ++id) {
                if (!containsCase(id)) throw std::invalid_argument("不存在测试用例: " + std::to_string(id));
                if (std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
            }
        }
        return result;
    }

    std::optional<TestWorkflowSource> parseSource(
        const QString& value
    ) {
        if (value.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) return std::nullopt;
        if (value.compare(QStringLiteral("cpp"), Qt::CaseInsensitive) == 0) return TestWorkflowSource::CPP;
        if (value.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) return TestWorkflowSource::JSON;
        throw std::invalid_argument("--source 只允许 cpp、json 或 all");
    }
} // namespace

int main(
    int argc,
    char* argv[]
) {
    configureDeterministicBrowserProcess();
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("workflow-test"));
    QApplication::setOrganizationName(QStringLiteral("workflow"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    if (argc > 1) attachCommandLineOutput();
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Workflow Qt Browser 自动化测试程序"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        QStringLiteral("run-all"),
        QStringLiteral("显示测试窗口，自动运行全部 C++/JSON 用例，打印结果后退出。")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("cases"),
        QStringLiteral("只运行指定页面；支持逗号和范围，例如 1,3,8-12。"),
        QStringLiteral("列表")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("source"),
        QStringLiteral("只运行指定流程来源：cpp、json 或 all。"),
        QStringLiteral("来源"),
        QStringLiteral("all")
    ));
    parser.process(application);
    const bool automaticRun = parser.isSet(QStringLiteral("run-all")) || parser.isSet(QStringLiteral("cases")) ||
                              parser.isSet(QStringLiteral("source"));

    std::vector<int> caseIds;
    std::optional<TestWorkflowSource> source;
    try {
        caseIds = parser.isSet(QStringLiteral("cases")) ? parseCaseIds(parser.value(QStringLiteral("cases")))
                                                        : std::vector<int>{};
        source = parseSource(parser.value(QStringLiteral("source")));
    } catch (const std::exception& exception) {
        if (argc > 1) {
            std::cerr << "workflow-test: " << exception.what() << '\n';
            std::cerr.flush();
        } else {
            qCritical("%s", exception.what());
        }
        return 2;
    }

    try {
        TestWindow window;
        QObject::connect(&window, &TestWindow::automaticRunFinished, &application, [&application](const int exitCode) {
            application.exit(exitCode);
        });
        window.show();
        if (automaticRun) {
            QTimer::singleShot(0, &window, [&window, caseIds, source] { window.startAutomaticRun(caseIds, source); });
        }
        return QApplication::exec();
    } catch (const std::exception& exception) {
        if (argc > 1) {
            std::cerr << "workflow-test: " << exception.what() << '\n';
            std::cerr.flush();
        } else {
            qCritical("%s", exception.what());
        }
        return 1;
    }
}
