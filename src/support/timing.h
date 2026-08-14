#ifndef WORKFLOW_SUPPORT_TIMING_H
#define WORKFLOW_SUPPORT_TIMING_H

#include <atomic>

void sleep(const std::atomic<bool>* stopFlag, float seconds);
[[nodiscard]] bool stopped(const std::atomic<bool>* stopFlag);

#endif // WORKFLOW_SUPPORT_TIMING_H
