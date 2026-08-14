#include "workflow.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <atomic>
#include <memory>
#include <stdexcept>

namespace {

    void runMixedWorkflow(
        Env& executionEnv,
        const QString& startTemplate
    ) {
        // Conqueror-compatible task entry: bind the caller-owned context to the
        // current worker thread before constructing a Clicker.
        env = executionEnv;

        // Clicker is abstract. The concrete entry type makes the accepted run
        // config a compile-time property.
        auto imageClicker = std::make_unique<ImageClicker>(
            startTemplate,
            ImageInitConfig{
                .threshold = 0.92f,
                .timeout = 30,
                .wait = 0,
                .mode = Mode::GRAY,
                .region = QRect(0, 0, 900, 700),
            }
        );
        auto next = imageClicker->click(
            ImageRunConfig{
                .finishUntilList =
                    {
                        // AnyText crops before OCR, recognizes the ROI once,
                        // then checks candidates in input order. Its global
                        // Segment result is consumed by Clicker::_createNext,
                        // which creates a concrete TextClicker.
                        new AnyText(
                            {"确认", "继续"},
                            {
                                .mode = Mode::RGB,
                                .threshold = 0.80f,
                                .reverse = false,
                                .match = TextMatch::EXACT,
                                .boxThreshold = 0.50f,
                                .region = QRect(250, 180, 500, 360),
                            }
                        ),
                    },
            }
        );

        auto* textClicker = dynamic_cast<TextClicker*>(next.get());
        if (!textClicker) throw std::runtime_error("下一节点不是TextClicker");
        textClicker
            ->click(
                TextRunConfig{
                    .runUntilList =
                        {
                            // INNER derives the smallest OCR crop from the
                            // previously clicked text box. No full-window OCR.
                            new AnyText(
                                {"确认", "继续"},
                                {
                                    .onPrevious = Previous::INNER,
                                    .mode = Mode::RGB,
                                    .threshold = 0.80f,
                                    .reverse = true,
                                    .match = TextMatch::EXACT,
                                    .boxThreshold = 0.50f,
                                    .cropToPrevious = true,
                                    .cropPadding = QMargins(12, 8, 12, 8),
                                }
                            ),
                        },
                }
            )
            ->end();
    }

} // namespace

int main(
    int argc,
    char* argv[]
) {
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() < 3) {
        qInfo().noquote() << "用法: mixed_workflow_example <HWND十六进制或十进制> <入口模板图片路径>";
        return 0;
    }

    bool parsed = false;
    const quintptr rawHwnd = arguments.at(1).toULongLong(&parsed, 0);
    if (!parsed || rawHwnd == 0) {
        qCritical().noquote() << "HWND格式无效";
        return 2;
    }

    std::atomic_bool stopFlag{false};
    Emitter emitter;
    QObject::connect(&emitter, &Emitter::log, [](const QString& message, const QString&) {
        qInfo().noquote() << message;
    });
    QObject::connect(&emitter, &Emitter::error, [](const QString& message) { qCritical().noquote() << message; });

    OcrEngine ocr(QDir(QCoreApplication::applicationDirPath()).filePath("models"));
    QString error;
    if (!ocr.initialize(&error)) {
        qCritical().noquote() << error;
        return 3;
    }

    Env executionEnv;
    executionEnv.hwnd = reinterpret_cast<HWND>(rawHwnd);
    executionEnv.emitter = &emitter;
    executionEnv.stopFlag = &stopFlag;
    executionEnv.ocr = &ocr;
    executionEnv.resourceRoot = QCoreApplication::applicationDirPath();

    try {
        runMixedWorkflow(executionEnv, arguments.at(2));
    } catch (const std::exception& exception) {
        qCritical().noquote() << QString::fromUtf8(exception.what());
        return 4;
    }
    return 0;
}
