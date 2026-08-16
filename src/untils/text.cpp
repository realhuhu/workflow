#include "untils/text.h"

#include "core/environment.h"
#include "matching/image.h"
#include "matching/selector.h"
#include "matching/text.h"
#include "support/ocr.h"
#include "support/timing.h"

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
            values.push_back(target);
        return values.join("|");
    }
} // namespace

TextUntil::TextUntil(
    QString target,
    TextUntilConfig config
) : Until(std::move(target), MatchKind::TEXT), config(std::move(config)), textMatch(matchConfig()) {
}

Until::RuntimeConfig TextUntil::runtimeConfig() const {
    return {
        .onPrevious = config.onPrevious,
        .interval = config.interval,
        .startWait = config.startWait,
        .finishWait = config.finishWait,
        .timeout = config.timeout,
        .reverse = config.reverse,
    };
}

TextMatchConfig TextUntil::matchConfig() const {
    TextMatchConfig result;
    result.match = config.match;
    result.caseSensitivity = config.caseSensitivity;
    result.normalize = config.normalize;
    result.threshold = config.threshold;
    result.boxThreshold = config.boxThreshold;
    result.maxEditDistance = config.maxEditDistance;
    result.candidates = config.candidates;
    result.uniqueNearest = config.uniqueNearest;
    return result;
}

QRect TextUntil::resolveOcrRegion(
    const cv::Mat& screen,
    const std::unique_ptr<Segment>& previous
) const {
    if (config.onPrevious != Previous::NONE && !previous) {
        throw std::runtime_error("Previous segment为空: " + toString().toStdString());
    }
    const bool usePrevious = config.cropToPrevious && config.onPrevious != Previous::NONE;
    return OCR::resolveRegion(
        screen,
        config.region,
        usePrevious ? config.onPrevious : Previous::NONE,
        usePrevious ? previous.get() : nullptr,
        config.cropPadding
    );
}

Text::Text(
    QString target,
    TextUntilConfig config
) : TextUntil(std::move(target), std::move(config)) {
}

std::vector<Segment> Text::scan(
    std::unique_ptr<Segment>& previous
) {
    targetSegmentList.clear();
    resolvedRegion.reset();
    textMatch = matchConfig();
    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) {
        throw std::runtime_error("窗口截图失败: " + toString().toStdString());
    }

    resolvedRegion = resolveOcrRegion(screen, previous);
    if (resolvedRegion->isEmpty()) return {};

    const auto matched = TextMatcher::findPositions(screen, target, textMatch, *resolvedRegion);
    targetSegmentList = filter(matched, previous);
    return targetSegmentList;
}

QString Text::toString() const {
    return QString("等待%1文字「%2」").arg(config.reverse ? "消失" : "", target);
}

AnyText::AnyText(
    const std::vector<QString>& targets,
    TextUntilConfig config
) : TextUntil({}, std::move(config)), targetList(targets) {
}

std::vector<Segment> AnyText::scan(
    std::unique_ptr<Segment>& previous
) {
    targetSegmentList.clear();
    target.clear();
    resolvedRegion.reset();
    textMatch = matchConfig();
    if (targetList.empty()) return {};
    if (config.onPrevious != Previous::NONE && !previous) {
        throw std::runtime_error("Previous segment为空: " + toString().toStdString());
    }

    const cv::Mat screen = CV::getScreen(env.hwnd, config.mode);
    if (screen.empty()) {
        throw std::runtime_error("窗口截图失败: " + toString().toStdString());
    }
    resolvedRegion = resolveOcrRegion(screen, previous);
    if (resolvedRegion->isEmpty()) return {};

    const OcrRunResult recognized = OCR::recognize(screen, *resolvedRegion);
    if (!recognized.ok) {
        throw std::runtime_error(recognized.error.toUtf8().toStdString());
    }

    TextMatchConfig candidateConfig = textMatch;
    if (candidateConfig.match == TextMatch::FUZZY && candidateConfig.uniqueNearest) {
        for (const QString& candidate : targetList) {
            if (!candidateConfig.candidates.contains(candidate)) candidateConfig.candidates.append(candidate);
        }
    }
    for (const QString& candidate : targetList) {
        const auto matched = TextMatcher::matchTokens(recognized.tokens, candidate, candidateConfig);
        auto filtered = filter(matched, previous);
        if (filtered.empty()) continue;
        target = candidate;
        targetSegmentList = std::move(filtered);
        return targetSegmentList;
    }
    return {};
}

QString AnyText::toString() const {
    return QString("等待任意文字|%1|").arg(joinTargets(targetList));
}

TextStable::TextStable(
    QString target,
    TextUntilConfig config
) : Text(std::move(target), std::move(config)) {
}

void TextStable::preHook(
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

bool TextStable::flag(
    std::unique_ptr<Segment>&
) {
    targetSegmentList.clear();
    if (!samePosition(positions)) return false;
    targetSegmentList = {positions.front()};
    return true;
}

QString TextStable::toString() const {
    return QString("等待稳定文字「%1」").arg(target);
}

IfText::IfText(
    const QString& target,
    TextUntilConfig config
) : Text(target, std::move(config)) {
}

bool IfText::loop(
    std::unique_ptr<Segment>& previous,
    float
) {
    sleep(env.stopFlag, config.startWait);
    if (stopped(env.stopFlag)) return false;
    return fulfilled(previous);
}

QString IfText::toString() const {
    return QString("尝试等待文字「%1」").arg(target);
}

IfAnyText::IfAnyText(
    const std::vector<QString>& targets,
    TextUntilConfig config
) : AnyText(targets, std::move(config)) {
}

bool IfAnyText::loop(
    std::unique_ptr<Segment>& previous,
    float
) {
    sleep(env.stopFlag, config.startWait);
    if (stopped(env.stopFlag)) return false;
    return fulfilled(previous);
}

QString IfAnyText::toString() const {
    return QString("尝试等待任意文字|%1|").arg(joinTargets(targetList));
}
