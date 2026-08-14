#ifndef WORKFLOW_MATCHING_TEXT_H
#define WORKFLOW_MATCHING_TEXT_H

#include <QStringList>

#include <opencv2/core.hpp>

#include <vector>

#include "core/segment.h"
#include "core/types.h"
#include "support/ocr.h"

struct TextMatchConfig {
    TextMatch match = TextMatch::CONTAINS;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
    bool normalize = true;
    float threshold = 0;
    float boxThreshold = 0;
    int maxEditDistance = 1;
    QStringList candidates{};
    bool uniqueNearest = true;
};

class TextMatcher {
public:
    static std::vector<Segment> findPositions(
        const cv::Mat& rawImg,
        const QString& text,
        const TextMatchConfig& config = {},
        const QRect& region = {}
    );

    static std::vector<Segment> findAnyPositions(
        const cv::Mat& rawImg,
        const std::vector<QString>& texts,
        const TextMatchConfig& config = {},
        const QRect& region = {},
        QString* matchedText = nullptr
    );

    static std::vector<Segment> matchTokens(
        const QVector<OcrToken>& tokens,
        const QString& text,
        const TextMatchConfig& config = {}
    );

    static QString normalized(const QString& text);
    static int editDistance(const QString& left, const QString& right);
    static int containedEditDistance(const QString& ocrText, const QString& target);
};

#endif // WORKFLOW_MATCHING_TEXT_H
