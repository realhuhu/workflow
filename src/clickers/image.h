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
