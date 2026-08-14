#ifndef WORKFLOW_CORE_ENVIRONMENT_H
#define WORKFLOW_CORE_ENVIRONMENT_H

#include <windows.h>

#include <QHash>
#include <QString>

#include <atomic>

class Emitter;
class OcrProvider;
class Platform;

struct Env {
    HWND hwnd{};
    DWORD pid{};
    Emitter* emitter{};
    std::atomic<bool>* stopFlag{};

    QHash<QString, bool> logFlag{};
    QHash<QString, bool> context{};

    // Optional dependency injection points. Null selects the built-in Win32
    // platform; text matching requires a configured OCR provider.
    Platform* platform{};
    OcrProvider* ocr{};
    QString resourceRoot{};
};

extern thread_local Env env;

#endif // WORKFLOW_CORE_ENVIRONMENT_H
