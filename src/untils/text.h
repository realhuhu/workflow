#ifndef WORKFLOW_UNTILS_TEXT_H
#define WORKFLOW_UNTILS_TEXT_H

#include <opencv2/core.hpp>

#include <optional>
#include <vector>

#include "matching/text.h"
#include "untils/base.h"

class ClickerBase;

struct TextUntilConfig {
    Previous onPrevious = Previous::NONE;
    Mode mode = Mode::RGB;
    float threshold = 0;
    float interval = 0.1f;
    float startWait = 0;
    float finishWait = 0;
    float timeout = -1;
    bool reverse = false;

    TextMatch match = TextMatch::CONTAINS;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
    bool normalize = true;
    float boxThreshold = 0;
    int maxEditDistance = 1;
    QStringList candidates{};
    bool uniqueNearest = true;

    QRect region{};
    bool cropToPrevious = true;
    QMargins cropPadding{};
};

class TextUntil : public Until {
public:
    TextUntilConfig config;
    std::vector<Segment> targetSegmentList;

protected:
    friend class ClickerBase;

    TextMatchConfig textMatch{};
    std::optional<QRect> resolvedRegion;

    TextUntil(QString target, TextUntilConfig config);
    [[nodiscard]] RuntimeConfig runtimeConfig() const override;
    [[nodiscard]] TextMatchConfig matchConfig() const;
    [[nodiscard]] QRect resolveOcrRegion(const cv::Mat& screen, const std::unique_ptr<Segment>& previous) const;
};

class Text : public TextUntil {
public:
    explicit Text(QString target, TextUntilConfig config = {});
    [[nodiscard]] QString toString() const override;

protected:
    [[nodiscard]] std::vector<Segment> scan(std::unique_ptr<Segment>& previous) override;
};

class AnyText : public TextUntil {
public:
    std::vector<QString> targetList;

    explicit AnyText(const std::vector<QString>& targets, TextUntilConfig config = {});
    [[nodiscard]] QString toString() const override;

protected:
    [[nodiscard]] std::vector<Segment> scan(std::unique_ptr<Segment>& previous) override;
};

class TextStable : public Text {
public:
    std::vector<Segment> positions;

    explicit TextStable(QString target, TextUntilConfig config = {});
    void preHook(std::unique_ptr<Segment>& previous) override;
    bool flag(std::unique_ptr<Segment>& previous) override;
    [[nodiscard]] QString toString() const override;
};

class IfText : public Text {
public:
    explicit IfText(const QString& target, TextUntilConfig config = {});
    void loop(std::unique_ptr<Segment>& previous, float globalTimeout) override;
    [[nodiscard]] QString toString() const override;
};

class IfAnyText : public AnyText {
public:
    explicit IfAnyText(const std::vector<QString>& targets, TextUntilConfig config = {});
    void loop(std::unique_ptr<Segment>& previous, float globalTimeout) override;
    [[nodiscard]] QString toString() const override;
};

#endif // WORKFLOW_UNTILS_TEXT_H
