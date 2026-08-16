#include "clickers/image.h"

#include "core/environment.h"
#include "matching/image.h"

#include <stdexcept>
#include <utility>

Selector RunConfigAdapter<ImageRunConfig>::selector(
    const ImageRunConfig& config
) {
    return config.selector;
}

ImageClicker::ImageClicker(
    QString target,
    const ImageInitConfig& config
) : Clicker(std::move(target), MatchKind::IMAGE, config) {
    initialize(this->config.wait);
}

ImageClicker::ImageClicker(
    QString target,
    const Segment& segment,
    const ImageInitConfig& config
) : Clicker(std::move(target), segment, MatchKind::IMAGE, config) {
}

ImageClicker::ImageClicker(
    QString target,
    const std::vector<Segment>& segmentList,
    const ImageInitConfig& config
) : Clicker(std::move(target), segmentList, MatchKind::IMAGE, config) {
}

std::vector<Segment> ImageClicker::matchTarget() {
    targetSegmentList.clear();
    if (target.isEmpty()) return targetSegmentList;

    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) throw std::runtime_error("窗口截图失败");
    targetSegmentList = CV::findPositions(screen, target, config.threshold, config.mode, config.region, config.scales);
    return targetSegmentList;
}

std::unique_ptr<ClickerBase> ImageClicker::clone() const {
    auto clone = std::make_unique<ImageClicker>(target, targetSegmentList, config);
    if (previousSegment) {
        clone->previousSegment = std::make_unique<Segment>(previousSegment->copy());
    }
    return clone;
}
