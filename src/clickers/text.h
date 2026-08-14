#ifndef WORKFLOW_CLICKERS_TEXT_H
#define WORKFLOW_CLICKERS_TEXT_H

#include <optional>

#include "clickers/base.h"
#include "matching/text.h"

struct TextInitConfig {
    float timeout = 60;
    float wait = 0;
    Mode mode = Mode::RGB;
    QRect region{};
    TextMatchConfig match{};
    // nullopt means the OCR region has not been resolved. An engaged empty
    // rectangle is a real empty intersection and must not mean whole window.
    std::optional<QRect> resolvedRegion;
};

struct TextRunConfig {
    float startWait = 0;
    std::vector<Until*> startUntilList{};
    std::vector<Until*> runUntilList{};
    std::vector<Until*> finishUntilList{};
    float finishWait = 0;
    bool homing = true;
};

template <> struct RunConfigAdapter<TextRunConfig> {
    [[nodiscard]] static Selector selector(const TextRunConfig& config);
};

class TextClicker final : public Clicker<TextInitConfig, TextRunConfig> {
public:
    explicit TextClicker(QString target, TextInitConfig config = {});
    explicit TextClicker(QString target, const Segment& segment, TextInitConfig config = {});
    explicit TextClicker(QString target, const std::vector<Segment>& segmentList, TextInitConfig config = {});

protected:
    [[nodiscard]] std::vector<Segment> matchTarget() override;
    [[nodiscard]] std::unique_ptr<ClickerBase> clone() const override;
};

#endif // WORKFLOW_CLICKERS_TEXT_H
