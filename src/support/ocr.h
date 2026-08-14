#ifndef WORKFLOW_SUPPORT_OCR_H
#define WORKFLOW_SUPPORT_OCR_H

#include <QRect>
#include <QVector>

#include <opencv2/core.hpp>

#include <memory>

#include "core/segment.h"
#include "core/types.h"

struct OcrToken {
    QString text;
    QRect box;
    QPoint center;
    float confidence = 0;
    float boxConfidence = 0;
};

struct OcrRunResult {
    bool ok = false;
    QVector<OcrToken> tokens;
    // Diagnostic image in ROI-local coordinates. Tokens below are always
    // translated back to the original client coordinate space.
    cv::Mat boxedImage;
    double elapsedMs = 0;
    QString error;
    QRect region;
};

class OcrProvider {
public:
    virtual ~OcrProvider() = default;
    virtual OcrRunResult recognize(const cv::Mat& image) = 0;
};

struct OcrEngineConfig {
    QString modelsDirectory;
    int numThreads = 1;
    int padding = 50;
    int maxSideLength = 1024;
    float boxScoreThreshold = 0.5f;
    float boxThreshold = 0.3f;
    float unclipRatio = 1.6f;
    bool detectAngle = true;
    bool mostAngle = true;
};

class OcrEngine final : public OcrProvider {
public:
    explicit OcrEngine(QString modelsDirectory);
    explicit OcrEngine(OcrEngineConfig config);
    ~OcrEngine() override;

    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;

    bool initialize(QString* error = nullptr) const;
    OcrRunResult recognize(const cv::Mat& image) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

class OCR {
public:
    // Cropping happens before OcrProvider::recognize. Returned boxes and
    // centers are translated back into the original window coordinate space.
    static OcrRunResult recognize(const cv::Mat& rawImg, const QRect& region = {});

    // Resolve an explicit window ROI and/or an ROI derived from the previous
    // Segment. An empty explicit region means unconstrained, not empty.
    static QRect resolveRegion(
        const cv::Mat& rawImg,
        const QRect& explicitRegion,
        Previous onPrevious = Previous::NONE,
        const Segment* previous = nullptr,
        const QMargins& padding = {}
    );
};

#endif // WORKFLOW_SUPPORT_OCR_H
