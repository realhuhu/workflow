#include "clickers/base.h"

#include "clickers/image.h"
#include "clickers/text.h"
#include "core/environment.h"
#include "platform/platform.h"
#include "support/logging.h"
#include "support/timing.h"
#include "untils/base.h"
#include "untils/image.h"
#include "untils/text.h"

#include <chrono>
#include <stdexcept>
#include <unordered_set>
#include <utility>

ClickerBase::ClickerBase(
    QString target,
    const MatchKind kind
) : target(std::move(target)), kind(kind) {
}

ClickerBase::ClickerBase(
    QString target,
    const Segment& segment,
    const MatchKind kind
) : target(std::move(target)), kind(kind), targetSegmentList{segment} {
}

ClickerBase::ClickerBase(
    QString target,
    const std::vector<Segment>& segmentList,
    const MatchKind kind
) : target(std::move(target)), kind(kind), targetSegmentList(segmentList) {
}

void ClickerBase::initialize(
    const float wait
) {
    sleep(env.stopFlag, wait);
    if (!stopped(env.stopFlag)) (void)matchTarget();
}

void ClickerBase::end() {
}

bool ClickerBase::founded() const {
    return !targetSegmentList.empty();
}

QString ClickerBase::toString() const {
    return QString("[%1]").arg(target);
}

namespace {
    bool timedOut(
        const std::chrono::steady_clock::time_point& startedAt,
        const float timeout
    ) {
        return timeout >= 0 &&
               std::chrono::duration<float>(std::chrono::steady_clock::now() - startedAt).count() > timeout;
    }
} // namespace

std::unique_ptr<ClickerBase> ClickerBase::locate() {
    return locateResolved(defaultRunConfig());
}

std::unique_ptr<ClickerBase> ClickerBase::locateResolved(
    ResolvedRunConfig config
) {
    validateLocateConfig(config.startUntilList, config.runUntilList, config.finishUntilList);

    const auto executor = [this, &config] {
        if (targetSegmentList.empty()) {
            throw std::runtime_error("未匹配到目标: " + target.toStdString());
        }
        const Segment segment = config.selector(targetSegmentList);
        previousSegment = std::make_unique<Segment>(segment.copy());
        logMessage(toString() + "定位成功: " + segment.toString());
    };

    return _run(
        "locate",
        executor,
        config.startWait,
        config.finishWait,
        config.startUntilList,
        {},
        config.finishUntilList,
        config.homing
    );
}

std::unique_ptr<ClickerBase> ClickerBase::click(
    const float interval,
    const int offsetX,
    const int offsetY,
    const Click position
) {
    return clickResolved(defaultRunConfig(), interval, offsetX, offsetY, position);
}

std::unique_ptr<ClickerBase> ClickerBase::clickResolved(
    ResolvedRunConfig config,
    const float interval,
    const int offsetX,
    const int offsetY,
    const Click position
) {
    const auto executor = [this, &config, interval, offsetX, offsetY, position] {
        if (targetSegmentList.empty()) {
            throw std::runtime_error("未匹配到目标: " + target.toStdString());
        }

        const Segment segment = config.selector(targetSegmentList);
        previousSegment = std::make_unique<Segment>(segment.copy());
        const auto startedAt = std::chrono::steady_clock::now();
        logMessage("开始循环点击: " + segment.toString());

        segment.click(0, offsetX, offsetY, position);
        sleep(env.stopFlag, 0.5f);

        while (!stopped(env.stopFlag)) {
            if (timedOut(startedAt, timeout())) {
                throw std::runtime_error("超时，结束运行: " + toString().toStdString());
            }
            if (config.runUntilList.empty()) break;

            bool allFulfilled = true;
            for (Until* condition : config.runUntilList) {
                if (condition->fulfilled(previousSegment)) continue;
                allFulfilled = false;
                break;
            }
            if (allFulfilled) {
                logMessage("所有CLICK条件满足，结束循环点击");
                env.logFlag.remove(QString("%1-click-not-fulfilled").arg(toString()));
                break;
            }

            segment.click(0, offsetX, offsetY, position);
            if (const QString key = QString("%1-click-not-fulfilled").arg(toString()); !env.logFlag.contains(key)) {
                env.logFlag[key] = true;
                logMessage("CLICK条件不满足，继续循环点击");
            }
            sleep(env.stopFlag, interval);
        }
    };

    return _run(
        "click",
        executor,
        config.startWait,
        config.finishWait,
        config.startUntilList,
        config.runUntilList,
        config.finishUntilList,
        config.homing
    );
}

std::unique_ptr<ClickerBase> ClickerBase::drag(
    const int step,
    const bool reverse
) {
    return dragResolved(defaultRunConfig(), step, reverse);
}

std::unique_ptr<ClickerBase> ClickerBase::dragResolved(
    ResolvedRunConfig config,
    const int step,
    const bool reverse
) {
    const auto executor = [this, &config, step, reverse] {
        if (targetSegmentList.empty()) {
            throw std::runtime_error("未匹配到目标: " + target.toStdString());
        }

        Segment before = config.selector(targetSegmentList);
        previousSegment = std::make_unique<Segment>(before.copy());
        const auto startedAt = std::chrono::steady_clock::now();
        logMessage("开始拖动: " + before.toString());

        const auto checkTimeout = [this, &startedAt] {
            if (timedOut(startedAt, timeout())) {
                throw std::runtime_error("超时，结束运行: " + toString().toStdString());
            }
        };

        const auto rematchAfterDrag = [this, &config, &before] {
            const auto matched = matchTarget();
            if (matched.empty()) {
                logMessage("拖动后目标消失，结束拖动");
                return false;
            }
            const Segment after = config.selector(matched);
            if (before == after) {
                logMessage("已拖动到底，结束拖动");
                return false;
            }
            before = after;
            return true;
        };

        if (reverse) {
            while (!stopped(env.stopFlag)) {
                checkTimeout();
                before.drag(0.1f, 100);
                if (!rematchAfterDrag()) break;
            }
        }

        while (!stopped(env.stopFlag)) {
            checkTimeout();
            if (config.runUntilList.empty()) break;

            bool allFulfilled = true;
            for (Until* condition : config.runUntilList) {
                if (condition->fulfilled(previousSegment)) continue;
                allFulfilled = false;
                break;
            }
            if (allFulfilled) {
                logMessage("所有DRAG条件满足，结束拖动");
                break;
            }

            before.drag(0.1f, reverse ? -step : step);
            if (!rematchAfterDrag()) break;
            logMessage("DRAG条件不满足，继续拖动");
        }
    };

    return _run(
        "drag",
        executor,
        config.startWait,
        config.finishWait,
        config.startUntilList,
        config.runUntilList,
        config.finishUntilList,
        config.homing
    );
}

std::unique_ptr<ClickerBase> ClickerBase::scroll(
    const int delta,
    const float interval,
    const int offsetX,
    const int offsetY,
    const Click position
) {
    return scrollResolved(defaultRunConfig(), delta, interval, offsetX, offsetY, position);
}

std::unique_ptr<ClickerBase> ClickerBase::scrollResolved(
    ResolvedRunConfig config,
    const int delta,
    const float interval,
    const int offsetX,
    const int offsetY,
    const Click position
) {
    const auto executor = [this, &config, delta, interval, offsetX, offsetY, position] {
        if (targetSegmentList.empty()) {
            throw std::runtime_error("未匹配到目标: " + target.toStdString());
        }

        const Segment segment = config.selector(targetSegmentList);
        previousSegment = std::make_unique<Segment>(segment.copy());
        const auto startedAt = std::chrono::steady_clock::now();
        logMessage("开始循环滚动: " + segment.toString());

        if (config.runUntilList.empty()) {
            segment.scroll(delta, 0, offsetX, offsetY, position);
            return;
        }

        while (!stopped(env.stopFlag)) {
            if (timedOut(startedAt, timeout())) {
                throw std::runtime_error("超时，结束运行: " + toString().toStdString());
            }

            bool allFulfilled = true;
            for (Until* condition : config.runUntilList) {
                if (stopped(env.stopFlag)) {
                    allFulfilled = false;
                    break;
                }
                if (condition->fulfilled(previousSegment)) continue;
                allFulfilled = false;
                break;
            }
            if (stopped(env.stopFlag)) break;
            if (allFulfilled) {
                logMessage("所有SCROLL条件满足，结束循环滚动");
                env.logFlag.remove(QString("%1-scroll-not-fulfilled").arg(toString()));
                break;
            }

            segment.scroll(delta, 0, offsetX, offsetY, position);
            if (const QString key = QString("%1-scroll-not-fulfilled").arg(toString()); !env.logFlag.contains(key)) {
                env.logFlag[key] = true;
                logMessage("SCROLL条件不满足，继续循环滚动");
            }
            sleep(env.stopFlag, interval);
        }
    };

    return _run(
        "scroll",
        executor,
        config.startWait,
        config.finishWait,
        config.startUntilList,
        config.runUntilList,
        config.finishUntilList,
        config.homing
    );
}

namespace {
    Platform& platform() {
        return env.platform ? *env.platform : defaultPlatform();
    }

    struct ConditionAdoptionState {
        std::unordered_set<Until*> addresses;
        bool hasNull = false;
        bool hasDuplicate = false;
    };

    void adoptConditions(
        const std::vector<Until*>& source,
        std::vector<std::unique_ptr<Until>>& destination,
        ConditionAdoptionState& state
    ) {
        for (Until* condition : source) {
            if (!condition) {
                state.hasNull = true;
                continue;
            }
            if (!state.addresses.insert(condition).second) {
                state.hasDuplicate = true;
                continue;
            }
            destination.emplace_back(condition);
        }
    }

    void validateConditions(
        const ConditionAdoptionState& state
    ) {
        if (state.hasNull) throw std::invalid_argument("Until条件指针不能为空");
        if (state.hasDuplicate) {
            throw std::invalid_argument("同一个Until条件指针不能重复转移所有权");
        }
    }
} // namespace

void ClickerBase::validateLocateConfig(
    const std::vector<Until*>& startUntilList,
    const std::vector<Until*>& runUntilList,
    const std::vector<Until*>& finishUntilList
) {
    if (runUntilList.empty()) return;

    std::vector<std::unique_ptr<Until>> rejected;
    rejected.reserve(startUntilList.size() + runUntilList.size() + finishUntilList.size());
    ConditionAdoptionState adoption;
    adoptConditions(startUntilList, rejected, adoption);
    adoptConditions(runUntilList, rejected, adoption);
    adoptConditions(finishUntilList, rejected, adoption);
    validateConditions(adoption);
    throw std::invalid_argument("locate不支持runUntilList，请使用finishUntilList");
}

std::unique_ptr<ClickerBase> ClickerBase::_createNext(
    const std::vector<std::unique_ptr<Until>>& runUntilList,
    const std::vector<std::unique_ptr<Until>>& finishUntilList
) const {
    const Until* condition = nullptr;
    if (!finishUntilList.empty()) {
        condition = finishUntilList.back().get();
    } else if (!runUntilList.empty()) {
        condition = runUntilList.back().get();
    } else {
        // Keep the safe chainable extension used by click()->end(). Conqueror
        // returned nullptr here even though its own DSL dereferenced it.
        return clone();
    }

    switch (condition->kind) {
        case MatchKind::IMAGE: {
            const auto* imageUntil = dynamic_cast<const ImageUntil*>(condition);
            if (!imageUntil) throw std::logic_error("IMAGE条件类型与MatchKind不一致");
            return std::make_unique<ImageClicker>(
                condition->target,
                imageUntil->targetSegmentList,
                ImageInitConfig{
                    .threshold = imageUntil->config.threshold,
                    .timeout = timeout(),
                    .wait = 0,
                    .mode = imageUntil->config.mode,
                    .region = imageUntil->config.region,
                }
            );
        }
        case MatchKind::TEXT: {
            const auto* textUntil = dynamic_cast<const TextUntil*>(condition);
            if (!textUntil) throw std::logic_error("TEXT条件类型与MatchKind不一致");
            return std::make_unique<TextClicker>(
                condition->target,
                textUntil->targetSegmentList,
                TextInitConfig{
                    .timeout = timeout(),
                    .wait = 0,
                    .mode = textUntil->config.mode,
                    .region = textUntil->config.region,
                    .match = textUntil->textMatch,
                    .resolvedRegion = textUntil->resolvedRegion,
                }
            );
        }
    }
    throw std::invalid_argument("未知MatchKind");
}

void ClickerBase::_start(
    const float startWait,
    const std::vector<std::unique_ptr<Until>>& startUntilList
) {
    sleep(env.stopFlag, startWait);
    if (stopped(env.stopFlag)) return;
    for (const auto& condition : startUntilList) {
        if (stopped(env.stopFlag)) return;
        logMessage("等待开始: " + condition->toString());
        condition->loop(previousSegment, timeout());
        if (stopped(env.stopFlag)) return;
        logMessage("已开始: " + condition->toString());
    }
}

std::unique_ptr<ClickerBase> ClickerBase::_run(
    const QString& name,
    const std::function<void()>& executor,
    const float startWait,
    const float finishWait,
    const std::vector<Until*>& startUntilList,
    const std::vector<Until*>& runUntilList,
    const std::vector<Until*>& finishUntilList,
    const bool homing
) {
    std::vector<std::unique_ptr<Until>> startConditions;
    std::vector<std::unique_ptr<Until>> runConditions;
    std::vector<std::unique_ptr<Until>> finishConditions;
    startConditions.reserve(startUntilList.size());
    runConditions.reserve(runUntilList.size());
    finishConditions.reserve(finishUntilList.size());
    ConditionAdoptionState adoption;
    adoptConditions(startUntilList, startConditions, adoption);
    adoptConditions(runUntilList, runConditions, adoption);
    adoptConditions(finishUntilList, finishConditions, adoption);
    validateConditions(adoption);

    if (homing && !stopped(env.stopFlag)) platform().moveTo(env.hwnd, 0, 0);
    logMessage(toString() + "开始" + name + "流程", "green");
    try {
        _start(startWait, startConditions);
        if (stopped(env.stopFlag)) return clone();
        executor();
        if (stopped(env.stopFlag)) return clone();
        _finish(finishWait, finishConditions);
        if (stopped(env.stopFlag)) return clone();
        auto next = _createNext(runConditions, finishConditions);

        logMessage(toString() + "结束" + name + "流程", "green");
        if (homing && !stopped(env.stopFlag)) platform().moveTo(env.hwnd, 0, 0);
        return next;
    } catch (...) {
        if (homing && !stopped(env.stopFlag)) platform().moveTo(env.hwnd, 0, 0);
        throw;
    }
}

void ClickerBase::_finish(
    const float finishWait,
    const std::vector<std::unique_ptr<Until>>& finishUntilList
) {
    sleep(env.stopFlag, finishWait);
    if (stopped(env.stopFlag)) return;
    for (const auto& condition : finishUntilList) {
        if (stopped(env.stopFlag)) return;
        logMessage("等待结束: " + condition->toString());
        condition->loop(previousSegment, timeout());
        if (stopped(env.stopFlag)) return;
        logMessage("已结束: " + condition->toString());
    }
}
