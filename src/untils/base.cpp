#include "untils/base.h"

#include "core/environment.h"
#include "support/logging.h"
#include "support/timing.h"

#include <chrono>
#include <stdexcept>
#include <utility>

Until::Until(
    QString target,
    const MatchKind kind
) : target(std::move(target)), kind(kind) {
}

void Until::loop(
    std::unique_ptr<Segment>& previous,
    const float globalTimeout
) {
    const RuntimeConfig runtime = runtimeConfig();
    sleep(env.stopFlag, runtime.startWait);
    const auto startedAt = std::chrono::steady_clock::now();
    const float timeout = runtime.timeout < 0 ? globalTimeout : runtime.timeout;

    while (!stopped(env.stopFlag)) {
        if (timeout >= 0) {
            if (const float elapsed =
                    std::chrono::duration<float>(std::chrono::steady_clock::now() - startedAt).count();
                elapsed > timeout) {
                throw std::runtime_error("超时，结束运行: " + toString().toStdString());
            }
        }

        if (fulfilled(previous)) {
            sleep(env.stopFlag, runtime.finishWait);
            return;
        }
        sleep(env.stopFlag, runtime.interval);
    }
}

bool Until::flag(
    std::unique_ptr<Segment>& previous
) {
    return !scan(previous).empty();
}

bool Until::fulfilled(
    std::unique_ptr<Segment>& previous
) {
    preHook(previous);
    const bool matched = flag(previous);
    const bool isFulfilled = runtimeConfig().reverse ? !matched : matched;
    const QString logKey = QString("%1-until-not-fulfilled").arg(toString());

    if (isFulfilled) {
        logMessage("条件满足: " + toString());
        env.logFlag.remove(logKey);
        return true;
    }

    if (!env.logFlag.contains(logKey)) {
        env.logFlag[logKey] = true;
        logMessage("条件未满足: " + toString());
    }
    return false;
}

std::vector<Segment> Until::filter(
    const std::vector<Segment>& positions,
    const std::unique_ptr<Segment>& previous
) const {
    const Previous onPrevious = runtimeConfig().onPrevious;
    const bool constrained = onPrevious != Previous::NONE;
    if (constrained && !previous) {
        throw std::runtime_error("Previous segment为空: " + toString().toStdString());
    }

    if (constrained) {
        logMessage("筛选: 在" + previous->toString() + PreviousToString(onPrevious));
    }
    std::vector<Segment> result;
    result.reserve(positions.size());
    for (const auto& position : positions) {
        const Segment::Relation horizontal =
            previous ? position.on(*previous, Segment::Axis::HORIZONTAL) : Segment::Relation::NONE;
        const Segment::Relation vertical =
            previous ? position.on(*previous, Segment::Axis::VERTICAL) : Segment::Relation::NONE;
        bool keep = false;
        switch (onPrevious) {
            case Previous::NONE:
                keep = true;
                break;
            case Previous::LEFT:
                keep = horizontal == Segment::Relation::LEFT;
                break;
            case Previous::TOP:
                keep = vertical == Segment::Relation::TOP;
                break;
            case Previous::RIGHT:
                keep = horizontal == Segment::Relation::RIGHT;
                break;
            case Previous::DOWN:
                keep = vertical == Segment::Relation::DOWN;
                break;
            case Previous::LEFT_CENTER:
                keep = horizontal == Segment::Relation::LEFT && vertical == Segment::Relation::CENTER;
                break;
            case Previous::TOP_CENTER:
                keep = vertical == Segment::Relation::TOP && horizontal == Segment::Relation::CENTER;
                break;
            case Previous::RIGHT_CENTER:
                keep = horizontal == Segment::Relation::RIGHT && vertical == Segment::Relation::CENTER;
                break;
            case Previous::DOWN_CENTER:
                keep = vertical == Segment::Relation::DOWN && horizontal == Segment::Relation::CENTER;
                break;
            case Previous::INNER:
                keep = horizontal == Segment::Relation::CENTER && vertical == Segment::Relation::CENTER;
                break;
            default:
                throw std::invalid_argument("未知Previous枚举值");
        }
        if (keep) result.push_back(position);
    }

    if (constrained) {
        logMessage(QString("筛选前: %1, 筛选后: %2").arg(positions.size()).arg(result.size()));
    }
    return result;
}

void Until::preHook(
    std::unique_ptr<Segment>&
) {
}
