#include "untils/image.h"

#include "core/environment.h"
#include "matching/image.h"
#include "matching/selector.h"
#include "support/resources.h"
#include "support/timing.h"

#include <QStringList>

#include <stdexcept>
#include <utility>

namespace {
    bool samePosition(
        const std::vector<Segment>& positions
    ) {
        if (positions.size() != 3) return false;
        return positions[0] == positions[1] && positions[1] == positions[2];
    }

    QString joinTargets(
        const std::vector<QString>& targets
    ) {
        QStringList values;
        values.reserve(static_cast<int>(targets.size()));
        for (const auto& target : targets)
            values.push_back(QString("<img src='%1' height='14'>").arg(res(target)));
        return values.join("|");
    }
} // namespace

ImageUntil::ImageUntil(
    QString target,
    const ImageUntilConfig& config
) : Until(std::move(target), MatchKind::IMAGE), config(config) {
}

Until::RuntimeConfig ImageUntil::runtimeConfig() const {
    return {
        .onPrevious = config.onPrevious,
        .interval = config.interval,
        .startWait = config.startWait,
        .finishWait = config.finishWait,
        .timeout = config.timeout,
        .reverse = config.reverse,
    };
}

Image::Image(
    QString target,
    const ImageUntilConfig& config
) : ImageUntil(std::move(target), config) {
}

std::vector<Segment> Image::scan(
    std::unique_ptr<Segment>& previous
) {
    targetSegmentList.clear();
    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) {
        throw std::runtime_error("窗口截图失败: " + toString().toStdString());
    }
    const auto matched = CV::findPositions(screen, target, config.threshold, config.mode, config.region, config.scales);
    targetSegmentList = filter(matched, previous);
    return targetSegmentList;
}

QString Image::toString() const {
    return QString("等待%1<img src='%2' height='14'>").arg(config.reverse ? "消失" : "", res(target));
}

AnyImage::AnyImage(
    const std::vector<QString>& targets,
    const ImageUntilConfig& config
) : ImageUntil({}, config), targetList(targets) {
}

std::vector<Segment> AnyImage::scan(
    std::unique_ptr<Segment>& previous
) {
    targetSegmentList.clear();
    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) {
        throw std::runtime_error("窗口截图失败: " + toString().toStdString());
    }

    for (const auto& currentTarget : targetList) {
        const auto matched =
            CV::findPositions(screen, currentTarget, config.threshold, config.mode, config.region, config.scales);
        auto filtered = filter(matched, previous);
        if (filtered.empty()) continue;
        target = currentTarget;
        targetSegmentList = std::move(filtered);
        return targetSegmentList;
    }
    return {};
}

QString AnyImage::toString() const {
    return QString("等待任意|%1|").arg(joinTargets(targetList));
}

ImageStable::ImageStable(
    QString target,
    const ImageUntilConfig& config
) : Image(std::move(target), config) {
}

void ImageStable::preHook(
    std::unique_ptr<Segment>& previous
) {
    const auto matched = scan(previous);
    if (matched.empty()) {
        positions.clear();
        return;
    }
    positions.push_back(similaritySelector(matched));
    if (positions.size() > 3) positions.erase(positions.begin());
}

bool ImageStable::flag(
    std::unique_ptr<Segment>&
) {
    targetSegmentList.clear();
    if (!samePosition(positions)) return false;
    targetSegmentList = {positions.front()};
    return true;
}

QString ImageStable::toString() const {
    return QString("等待稳定<img src='%1' height='14'>").arg(res(target));
}

IfImage::IfImage(
    const QString& target,
    const ImageUntilConfig& config
) : Image(target, config) {
}

bool IfImage::loop(
    std::unique_ptr<Segment>& previous,
    float
) {
    sleep(env.stopFlag, config.startWait);
    if (stopped(env.stopFlag)) return false;
    return fulfilled(previous);
}

QString IfImage::toString() const {
    return QString("尝试等待<img src='%1' height='14'>").arg(res(target));
}

IfAnyImage::IfAnyImage(
    const std::vector<QString>& targets,
    const ImageUntilConfig& config
) : AnyImage(targets, config) {
}

bool IfAnyImage::loop(
    std::unique_ptr<Segment>& previous,
    float
) {
    sleep(env.stopFlag, config.startWait);
    if (stopped(env.stopFlag)) return false;
    return fulfilled(previous);
}

QString IfAnyImage::toString() const {
    return QString("尝试等待任意|%1|").arg(joinTargets(targetList));
}
