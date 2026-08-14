#include "clickers/text.h"

#include "core/environment.h"
#include "matching/image.h"
#include "matching/text.h"
#include "support/ocr.h"

#include <stdexcept>
#include <utility>

namespace {

    Segment firstSelector(
        const std::vector<Segment>& segments
    ) {
        if (segments.empty()) throw std::runtime_error("文字匹配结果为空");
        return segments.front();
    }

} // namespace

Selector RunConfigAdapter<TextRunConfig>::selector(
    const TextRunConfig&
) {
    return firstSelector;
}

TextClicker::TextClicker(
    QString target,
    TextInitConfig config
) : Clicker(std::move(target), MatchKind::TEXT, std::move(config)) {
    initialize(this->config.wait);
}

TextClicker::TextClicker(
    QString target,
    const Segment& segment,
    TextInitConfig config
) : Clicker(std::move(target), segment, MatchKind::TEXT, std::move(config)) {
}

TextClicker::TextClicker(
    QString target,
    const std::vector<Segment>& segmentList,
    TextInitConfig config
) : Clicker(std::move(target), segmentList, MatchKind::TEXT, std::move(config)) {
}

std::vector<Segment> TextClicker::matchTarget() {
    targetSegmentList.clear();
    if (target.isEmpty()) return targetSegmentList;

    if (config.resolvedRegion.has_value() && config.resolvedRegion->isEmpty()) {
        return targetSegmentList;
    }

    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) throw std::runtime_error("窗口截图失败");
    const QRect ocrRegion =
        config.resolvedRegion.has_value() ? *config.resolvedRegion : OCR::resolveRegion(screen, config.region);
    config.resolvedRegion = ocrRegion;
    if (ocrRegion.isEmpty()) return targetSegmentList;

    targetSegmentList = TextMatcher::findPositions(screen, target, config.match, ocrRegion);
    return targetSegmentList;
}

std::unique_ptr<ClickerBase> TextClicker::clone() const {
    auto clone = std::make_unique<TextClicker>(target, targetSegmentList, config);
    if (previousSegment) {
        clone->previousSegment = std::make_unique<Segment>(previousSegment->copy());
    }
    return clone;
}
