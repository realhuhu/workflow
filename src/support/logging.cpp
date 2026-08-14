#include "support/logging.h"

#include "core/environment.h"
#include "support/emitter.h"

void logMessage(
    const QString& message,
    const QString& color
) {
    if (env.emitter) emit env.emitter->log(message, color);
}
