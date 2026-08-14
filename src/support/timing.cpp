#include "support/timing.h"

#include <algorithm>
#include <chrono>
#include <thread>

bool stopped(
    const std::atomic<bool>* stopFlag
) {
    return stopFlag && stopFlag->load();
}

void sleep(
    const std::atomic<bool>* stopFlag,
    const float seconds
) {
    if (seconds <= 0 || stopped(stopFlag)) return;

    const auto duration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(seconds));
    const auto deadline = std::chrono::steady_clock::now() + duration;
    constexpr auto slice =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(10));
    while (!stopped(stopFlag)) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return;
        const auto remaining = deadline - now;
        std::this_thread::sleep_for((std::min)(remaining, slice));
    }
}
