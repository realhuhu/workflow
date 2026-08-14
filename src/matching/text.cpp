#include "matching/text.h"

#include "support/ocr.h"

#include <QRegularExpression>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

    QString normalizedForMatch(
        const QString& text,
        const TextMatchConfig& config
    ) {
        QString result;
        if (config.normalize) {
            result.reserve(text.size());
            for (const QChar character : text) {
                if (character.isLetterOrNumber()) result.append(character);
            }
        } else {
            result = text;
        }
        if (config.caseSensitivity == Qt::CaseInsensitive) result = result.toCaseFolded();
        return result;
    }

    bool passesConfidence(
        const OcrToken& token,
        const TextMatchConfig& config
    ) {
        return std::isfinite(token.confidence) && std::isfinite(token.boxConfidence) &&
               token.confidence >= config.threshold && token.boxConfidence >= config.boxThreshold;
    }

    bool isUniqueNearest(
        const QString& actual,
        const QString& target,
        const int targetDistance,
        const TextMatchConfig& config
    ) {
        if (!config.uniqueNearest || config.candidates.isEmpty()) return true;

        int nearestOther = std::numeric_limits<int>::max();
        for (const QString& candidate : config.candidates) {
            const QString normalizedCandidate = normalizedForMatch(candidate, config);
            if (normalizedCandidate.isEmpty() || normalizedCandidate == target) continue;
            nearestOther = std::min(nearestOther, TextMatcher::containedEditDistance(actual, normalizedCandidate));
        }
        return targetDistance < nearestOther;
    }

    Segment tokenSegment(
        const OcrToken& token
    ) {
        return {token.box.x(), token.box.y(), token.box.width(), token.box.height(), token.confidence};
    }

} // namespace

std::vector<Segment> TextMatcher::findPositions(
    const cv::Mat& rawImg,
    const QString& text,
    const TextMatchConfig& config,
    const QRect& region
) {
    const OcrRunResult result = OCR::recognize(rawImg, region);
    if (!result.ok) {
        throw std::runtime_error(result.error.toUtf8().toStdString());
    }
    return matchTokens(result.tokens, text, config);
}

std::vector<Segment> TextMatcher::findAnyPositions(
    const cv::Mat& rawImg,
    const std::vector<QString>& texts,
    const TextMatchConfig& config,
    const QRect& region,
    QString* matchedText
) {
    if (matchedText) matchedText->clear();
    if (texts.empty()) return {};

    const OcrRunResult result = OCR::recognize(rawImg, region);
    if (!result.ok) {
        throw std::runtime_error(result.error.toUtf8().toStdString());
    }

    TextMatchConfig candidateAwareConfig = config;
    if (candidateAwareConfig.match == TextMatch::FUZZY && candidateAwareConfig.uniqueNearest) {
        for (const QString& candidate : texts) {
            if (!candidateAwareConfig.candidates.contains(candidate)) {
                candidateAwareConfig.candidates.append(candidate);
            }
        }
    }

    for (const QString& text : texts) {
        std::vector<Segment> positions = matchTokens(result.tokens, text, candidateAwareConfig);
        if (positions.empty()) continue;
        if (matchedText) *matchedText = text;
        return positions;
    }
    return {};
}

std::vector<Segment> TextMatcher::matchTokens(
    const QVector<OcrToken>& tokens,
    const QString& text,
    const TextMatchConfig& config
) {
    std::vector<Segment> matches;
    if (text.isEmpty()) return matches;

    QRegularExpression expression;
    if (config.match == TextMatch::REGEX) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (config.caseSensitivity == Qt::CaseInsensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        expression = QRegularExpression(text, options);
        if (!expression.isValid()) return matches;
    }

    const QString target = config.match == TextMatch::REGEX ? text : normalizedForMatch(text, config);
    if (target.isEmpty()) return matches;

    matches.reserve(static_cast<size_t>(tokens.size()));
    for (const OcrToken& token : tokens) {
        if (!passesConfidence(token, config) || token.box.isEmpty()) continue;

        bool matched = false;
        if (config.match == TextMatch::REGEX) {
            matched = expression.match(token.text).hasMatch();
        } else {
            const QString actual = normalizedForMatch(token.text, config);
            if (actual.isEmpty()) continue;
            switch (config.match) {
                case TextMatch::EXACT:
                    matched = actual == target;
                    break;
                case TextMatch::CONTAINS:
                    matched = actual.contains(target, Qt::CaseSensitive);
                    break;
                case TextMatch::FUZZY: {
                    const int distance = containedEditDistance(actual, target);
                    matched = distance <= config.maxEditDistance && isUniqueNearest(actual, target, distance, config);
                    break;
                }
                case TextMatch::REGEX:
                    break;
            }
        }
        if (matched) matches.emplace_back(tokenSegment(token));
    }
    return matches;
}

QString TextMatcher::normalized(
    const QString& text
) {
    QString result;
    result.reserve(text.size());
    for (const QChar character : text) {
        if (character.isLetterOrNumber()) result.append(character.toLower());
    }
    return result;
}

int TextMatcher::editDistance(
    const QString& left,
    const QString& right
) {
    QVector<int> previous(right.size() + 1);
    QVector<int> current(right.size() + 1);
    for (int column = 0; column <= right.size(); ++column)
        previous[column] = column;

    for (int row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (int column = 1; column <= right.size(); ++column) {
            const int substitution = previous[column - 1] + (left.at(row - 1) == right.at(column - 1) ? 0 : 1);
            current[column] = std::min({previous[column] + 1, current[column - 1] + 1, substitution});
        }
        previous.swap(current);
    }
    return previous.at(right.size());
}

int TextMatcher::containedEditDistance(
    const QString& ocrText,
    const QString& target
) {
    if (ocrText.isEmpty() || target.isEmpty()) return std::max(ocrText.size(), target.size());
    if (ocrText.size() <= target.size()) return editDistance(ocrText, target);

    int best = editDistance(ocrText, target);
    for (int start = 0; start + target.size() <= ocrText.size(); ++start) {
        best = std::min(best, editDistance(ocrText.mid(start, target.size()), target));
    }
    return best;
}
