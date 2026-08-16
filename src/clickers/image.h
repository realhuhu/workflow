#ifndef WORKFLOW_CLICKERS_IMAGE_H
#define WORKFLOW_CLICKERS_IMAGE_H

#include <QRect>

#include "clickers/base.h"

struct ImageInitConfig {
    float threshold = 0.9f;
    float timeout = 60;
    float wait = 0;
    Mode mode = Mode::GRAY;
    QRect region{};
    std::vector<float> scales{1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
};

struct ImageRunConfig {
    float startWait = 0;
    Selector selector = similaritySelector;
    std::vector<Until*> startUntilList{};
    std::vector<Until*> runUntilList{};
    std::vector<Until*> finishUntilList{};
    float finishWait = 0;
    bool homing = true;
};

template <> struct RunConfigAdapter<ImageRunConfig> {
    static constexpr auto kind = MatchKind::IMAGE;
    [[nodiscard]] static Selector selector(const ImageRunConfig& config);
};

class ImageClicker final : public Clicker<ImageInitConfig, ImageRunConfig> {
public:
    explicit ImageClicker(QString target, const ImageInitConfig& config = {});
    explicit ImageClicker(QString target, const Segment& segment, const ImageInitConfig& config = {});
    explicit ImageClicker(QString target, const std::vector<Segment>& segmentList, const ImageInitConfig& config = {});

protected:
    [[nodiscard]] std::vector<Segment> matchTarget() override;
    [[nodiscard]] std::unique_ptr<ClickerBase> clone() const override;
};

#endif // WORKFLOW_CLICKERS_IMAGE_H
