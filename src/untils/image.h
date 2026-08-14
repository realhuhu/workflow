#ifndef WORKFLOW_UNTILS_IMAGE_H
#define WORKFLOW_UNTILS_IMAGE_H

#include <QRect>

#include <vector>

#include "untils/base.h"

struct ImageUntilConfig {
    Previous onPrevious = Previous::NONE;
    Mode mode = Mode::GRAY;
    float threshold = 0.9f;
    float interval = 0.1f;
    float startWait = 0;
    float finishWait = 0;
    float timeout = -1;
    bool reverse = false;
    QRect region{};
};

class ImageUntil : public Until {
public:
    ImageUntilConfig config;
    std::vector<Segment> targetSegmentList;

protected:
    ImageUntil(QString target, const ImageUntilConfig& config);
    [[nodiscard]] RuntimeConfig runtimeConfig() const override;
};

class Image : public ImageUntil {
public:
    explicit Image(QString target, const ImageUntilConfig& config = {});
    [[nodiscard]] QString toString() const override;

protected:
    [[nodiscard]] std::vector<Segment> scan(std::unique_ptr<Segment>& previous) override;
};

class AnyImage : public ImageUntil {
public:
    std::vector<QString> targetList;

    explicit AnyImage(const std::vector<QString>& targets, const ImageUntilConfig& config = {});
    [[nodiscard]] QString toString() const override;

protected:
    [[nodiscard]] std::vector<Segment> scan(std::unique_ptr<Segment>& previous) override;
};

class ImageStable : public Image {
public:
    std::vector<Segment> positions;

    explicit ImageStable(QString target, const ImageUntilConfig& config = {});
    void preHook(std::unique_ptr<Segment>& previous) override;
    bool flag(std::unique_ptr<Segment>& previous) override;
    [[nodiscard]] QString toString() const override;
};

class IfImage : public Image {
public:
    explicit IfImage(const QString& target, const ImageUntilConfig& config = {});
    void loop(std::unique_ptr<Segment>& previous, float globalTimeout) override;
    [[nodiscard]] QString toString() const override;
};

class IfAnyImage : public AnyImage {
public:
    explicit IfAnyImage(const std::vector<QString>& targets, const ImageUntilConfig& config = {});
    void loop(std::unique_ptr<Segment>& previous, float globalTimeout) override;
    [[nodiscard]] QString toString() const override;
};

#endif // WORKFLOW_UNTILS_IMAGE_H
