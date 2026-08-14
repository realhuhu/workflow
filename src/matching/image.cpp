#include "matching/image.h"

#include "core/environment.h"
#include "platform/platform.h"
#include "support/resources.h"

#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace {
    using Window = std::remove_pointer_t<HWND>;

    cv::Mat decodeImage(
        const QString& path,
        const Mode mode
    ) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        const QByteArray bytes = file.readAll();
        const std::vector<uchar> data(bytes.begin(), bytes.end());
        return cv::imdecode(data, mode == Mode::RGB ? cv::IMREAD_COLOR : cv::IMREAD_GRAYSCALE);
    }

    cv::Mat convertSource(
        const cv::Mat& source,
        const Mode mode
    ) {
        if (source.empty()) return {};
        cv::Mat converted;
        if (mode == Mode::GRAY) {
            if (source.channels() == 1) return source;
            if (source.channels() == 3) cv::cvtColor(source, converted, cv::COLOR_BGR2GRAY);
            else if (source.channels() == 4) cv::cvtColor(source, converted, cv::COLOR_BGRA2GRAY);
            else throw std::runtime_error("不支持的图像通道数");
        } else {
            if (source.channels() == 3) return source;
            if (source.channels() == 1) cv::cvtColor(source, converted, cv::COLOR_GRAY2BGR);
            else if (source.channels() == 4) cv::cvtColor(source, converted, cv::COLOR_BGRA2BGR);
            else throw std::runtime_error("不支持的图像通道数");
        }
        return converted;
    }

    QRect clippedRegion(
        const cv::Mat& image,
        const QRect& requested
    ) {
        const QRect bounds(0, 0, image.cols, image.rows);
        return requested.isEmpty() ? bounds : requested.normalized().intersected(bounds);
    }

    cv::Mat similarityMap(
        const cv::Mat& source,
        const cv::Mat& templ,
        Mode mode
    ) {
        cv::Mat result;
        if (mode == Mode::GRAY) {
            cv::matchTemplate(source, templ, result, cv::TM_CCOEFF_NORMED);
            return result;
        }

        cv::Mat sourceHsv;
        cv::Mat templateHsv;
        cv::cvtColor(source, sourceHsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(templ, templateHsv, cv::COLOR_BGR2HSV);

        std::vector<cv::Mat> sourceBgrChannels;
        std::vector<cv::Mat> templateBgrChannels;
        std::vector<cv::Mat> sourceHsvChannels;
        std::vector<cv::Mat> templateHsvChannels;
        cv::split(source, sourceBgrChannels);
        cv::split(templ, templateBgrChannels);
        cv::split(sourceHsv, sourceHsvChannels);
        cv::split(templateHsv, templateHsvChannels);

        result = cv::Mat::zeros(source.rows - templ.rows + 1, source.cols - templ.cols + 1, CV_32F);
        for (int channel = 0; channel < 3; ++channel) {
            cv::Mat current;
            cv::matchTemplate(sourceBgrChannels[channel], templateBgrChannels[channel], current, cv::TM_CCOEFF_NORMED);
            result += current;
            cv::matchTemplate(sourceHsvChannels[channel], templateHsvChannels[channel], current, cv::TM_CCOEFF_NORMED);
            result += current;
        }
        result /= 6.0f;
        return result;
    }

    std::vector<Segment> matchOne(
        const cv::Mat& rawImg,
        const QString& resolvedTemplatePath,
        float threshold,
        Mode mode,
        const QRect& requestedRegion
    ) {
        const cv::Mat source = convertSource(rawImg, mode);
        const cv::Mat templ = decodeImage(resolvedTemplatePath, mode);
        if (templ.empty()) throw std::runtime_error("无法解码图片: " + resolvedTemplatePath.toStdString());

        const QRect roi = clippedRegion(source, requestedRegion);
        if (roi.isEmpty() || templ.cols > roi.width() || templ.rows > roi.height()) return {};

        const cv::Rect cvRoi(roi.x(), roi.y(), roi.width(), roi.height());
        const cv::Mat search = source(cvRoi);
        cv::Mat scores = similarityMap(search, templ, mode);
        cv::patchNaNs(scores, -1.0);

        struct Candidate {
            cv::Point point;
            float score;
        };
        std::vector<Candidate> candidates;
        std::vector<cv::Point> locations;
        cv::findNonZero(scores >= threshold, locations);
        candidates.reserve(locations.size());
        for (const cv::Point& point : locations) {
            candidates.push_back({point, scores.at<float>(point.y, point.x)});
        }
        std::ranges::sort(candidates, [](const Candidate& left, const Candidate& right) {
            return left.score > right.score;
        });

        // Template matching produces a dense cluster around one visual object.
        // Keep the best point in each half-template neighborhood.
        std::vector<Segment> result;
        for (const auto& [point, score] : candidates) {
            const int globalX = roi.x() + point.x;
            const int globalY = roi.y() + point.y;
            const bool overlaps = std::ranges::any_of(result, [&](const Segment& accepted) {
                return std::abs(accepted.x - globalX) < std::max(1, templ.cols / 2) &&
                       std::abs(accepted.y - globalY) < std::max(1, templ.rows / 2);
            });
            if (overlaps) continue;
            result.emplace_back(globalX, globalY, templ.cols, templ.rows, score);
        }
        return result;
    }
} // namespace

cv::Mat CV::getScreen(
    Window* const hwnd,
    const Mode mode
) {
    Platform& platform = env.platform ? *env.platform : defaultPlatform();
    return platform.getScreen(hwnd, mode);
}

std::vector<Segment> CV::findPositions(
    const cv::Mat& rawImg,
    const QString& templatePath,
    const float threshold,
    const Mode mode,
    const QRect& region
) {
    if (rawImg.empty()) throw std::runtime_error("模板匹配输入图片为空");
    if (threshold < -1.0f || threshold > 1.0f) {
        throw std::invalid_argument("图像匹配 threshold 必须位于 [-1, 1]");
    }

    QStringList candidates;
    if (const QFileInfo direct(templatePath); direct.isAbsolute()) {
        candidates.append(direct.absoluteFilePath());
    } else {
        candidates.append(res(templatePath));
        QString customName = templatePath;
        customName.replace('/', '-').replace('\\', '-');
        candidates.append(res(customName, "自定义图片"));
    }

    bool foundFile = false;
    for (const QString& candidate : candidates) {
        if (!QFileInfo(candidate).isFile()) continue;
        foundFile = true;
        if (auto positions = matchOne(rawImg, candidate, threshold, mode, region); !positions.empty()) return positions;
    }
    if (!foundFile) {
        throw std::runtime_error("文件不存在: " + templatePath.toStdString());
    }
    return {};
}
