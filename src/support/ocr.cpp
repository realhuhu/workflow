#include "support/ocr.h"

#include "core/environment.h"

#include <OcrLite.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

    QString exceptionText(
        const std::exception& exception
    ) {
        const QString utf8 = QString::fromUtf8(exception.what());
        return utf8.isEmpty() ? QStringLiteral("未知异常") : utf8;
    }

    QRect imageBounds(
        const cv::Mat& image
    ) {
        return image.empty() ? QRect{} : QRect(0, 0, image.cols, image.rows);
    }

    bool toBgr8(
        const cv::Mat& source,
        cv::Mat& destination,
        QString& error
    ) {
        destination.release();
        if (source.empty()) {
            error = QStringLiteral("OCR输入图片为空");
            return false;
        }

        try {
            cv::Mat eightBit;
            if (source.depth() == CV_8U) {
                eightBit = source;
            } else {
                double alpha = 1.0;
                double beta = 0.0;
                switch (source.depth()) {
                    case CV_8S:
                        beta = 128.0;
                        break;
                    case CV_16U:
                        alpha = 1.0 / 257.0;
                        break;
                    case CV_16S:
                        alpha = 1.0 / 257.0;
                        beta = 128.0;
                        break;
                    case CV_32S:
                        alpha = 1.0 / 16777216.0;
                        beta = 128.0;
                        break;
                    case CV_32F:
                    case CV_64F: {
                        double minimum = 0.0;
                        double maximum = 0.0;
                        cv::minMaxLoc(source.reshape(1), &minimum, &maximum);
                        if (minimum >= 0.0 && maximum <= 1.0) alpha = 255.0;
                        break;
                    }
                    default:
                        error = QStringLiteral("OCR输入图片位深不受支持");
                        return false;
                }
                source.convertTo(eightBit, CV_MAKETYPE(CV_8U, source.channels()), alpha, beta);
            }

            switch (eightBit.channels()) {
                case 1:
                    cv::cvtColor(eightBit, destination, cv::COLOR_GRAY2BGR);
                    break;
                case 3:
                    destination = eightBit.clone();
                    break;
                case 4:
                    cv::cvtColor(eightBit, destination, cv::COLOR_BGRA2BGR);
                    break;
                default:
                    error = QStringLiteral("OCR输入图片通道数不受支持：%1").arg(eightBit.channels());
                    return false;
            }
        } catch (const std::exception& exception) {
            error = QStringLiteral("OCR图片标准化失败：") + exceptionText(exception);
            destination.release();
            return false;
        } catch (...) {
            error = QStringLiteral("OCR图片标准化失败：未知异常");
            destination.release();
            return false;
        }
        return true;
    }

    std::string nativePath(
        const QString& path
    ) {
        // RapidOCR's Windows bridge decodes this byte string as UTF-8. Keeping the
        // conversion explicit lets model directories contain non-ASCII text.
        return QDir::toNativeSeparators(path).toUtf8().toStdString();
    }

    float textConfidence(
        const TextBlock& block
    ) {
        if (block.charScores.empty()) return block.boxScore;
        const double total = std::accumulate(block.charScores.begin(), block.charScores.end(), 0.0);
        return static_cast<float>(total / static_cast<double>(block.charScores.size()));
    }

} // namespace

class OcrEngine::Impl {
public:
    explicit Impl(
        OcrEngineConfig value
    ) : config(std::move(value)) {
    }

    bool initialize(
        QString* error
    ) {
        std::lock_guard lock(mutex);
        return initializeUnlocked(error);
    }

    OcrRunResult recognize(
        const cv::Mat& image
    ) {
        OcrRunResult output;
        output.region = imageBounds(image);

        cv::Mat bgrImage;
        if (!toBgr8(image, bgrImage, output.error)) return output;

        std::lock_guard lock(mutex);
        if (!initializeUnlocked(&output.error)) return output;

        QElapsedTimer timer;
        timer.start();
        try {
            const OcrResult raw = engine->detect(
                bgrImage,
                std::max(0, config.padding),
                config.maxSideLength,
                config.boxScoreThreshold,
                config.boxThreshold,
                config.unclipRatio,
                config.detectAngle,
                config.mostAngle
            );
            output.elapsedMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
            output.boxedImage = bgrImage.clone();

            int index = 0;
            for (const TextBlock& block : raw.textBlocks) {
                if (block.boxPoint.empty()) continue;

                int minimumX = bgrImage.cols - 1;
                int minimumY = bgrImage.rows - 1;
                int maximumX = 0;
                int maximumY = 0;
                std::vector<cv::Point> polygon;
                polygon.reserve(block.boxPoint.size());
                for (const cv::Point& point : block.boxPoint) {
                    const int x = std::clamp(point.x, 0, bgrImage.cols - 1);
                    const int y = std::clamp(point.y, 0, bgrImage.rows - 1);
                    minimumX = std::min(minimumX, x);
                    minimumY = std::min(minimumY, y);
                    maximumX = std::max(maximumX, x);
                    maximumY = std::max(maximumY, y);
                    polygon.emplace_back(x, y);
                }

                OcrToken token;
                token.text = QString::fromUtf8(block.text.data(), static_cast<int>(block.text.size()));
                token.box = QRect(QPoint(minimumX, minimumY), QPoint(maximumX, maximumY));
                token.center = token.box.center();
                token.confidence = textConfidence(block);
                token.boxConfidence = block.boxScore;
                output.tokens.append(token);

                if (polygon.size() >= 2) {
                    const cv::Point* points = polygon.data();
                    const int pointCount = static_cast<int>(polygon.size());
                    cv::polylines(
                        output.boxedImage,
                        &points,
                        &pointCount,
                        1,
                        true,
                        cv::Scalar(0, 0, 255),
                        1,
                        cv::LINE_AA
                    );
                    cv::putText(
                        output.boxedImage,
                        std::to_string(index),
                        cv::Point(minimumX, std::max(10, minimumY - 2)),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.35,
                        cv::Scalar(255, 0, 0),
                        1,
                        cv::LINE_AA
                    );
                }
                ++index;
            }
            output.ok = true;
            output.error.clear();
        } catch (const std::exception& exception) {
            output.elapsedMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
            output.error = QStringLiteral("OCR推理失败：") + exceptionText(exception);
            output.tokens.clear();
            output.boxedImage.release();
        } catch (...) {
            output.elapsedMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
            output.error = QStringLiteral("OCR推理失败：未知异常");
            output.tokens.clear();
            output.boxedImage.release();
        }
        return output;
    }

private:
    bool initializeUnlocked(
        QString* error
    ) {
        if (error) error->clear();
        if (initialized) return true;

        const QDir models(config.modelsDirectory);
        const QString detectionModel = models.filePath(QStringLiteral("det.onnx"));
        const QString angleModel = models.filePath(QStringLiteral("cls.onnx"));
        const QString recognitionModel = models.filePath(QStringLiteral("rec.onnx"));
        const QString keys = models.filePath(QStringLiteral("keys.txt"));
        for (const QString& path : {detectionModel, angleModel, recognitionModel, keys}) {
            if (const QFileInfo file(path); !file.exists() || !file.isFile() || !file.isReadable()) {
                if (error) *error = QStringLiteral("OCR模型不存在或不可读：") + path;
                return false;
            }
        }

        try {
            auto candidate = std::make_unique<OcrLite>();
            candidate->initLogger(false, false, false);
            candidate->setNumThread(std::max(1, config.numThreads));
            const bool loaded = candidate->initModels(
                nativePath(detectionModel),
                nativePath(angleModel),
                nativePath(recognitionModel),
                nativePath(keys)
            );
            if (!loaded) {
                if (error) *error = QStringLiteral("OCR模型初始化失败");
                return false;
            }
            engine = std::move(candidate);
            initialized = true;
            return true;
        } catch (const std::exception& exception) {
            engine.reset();
            if (error) {
                *error = QStringLiteral("OCR模型初始化失败：") + exceptionText(exception);
            }
            return false;
        } catch (...) {
            engine.reset();
            if (error) *error = QStringLiteral("OCR模型初始化失败：未知异常");
            return false;
        }
    }

    OcrEngineConfig config;
    std::unique_ptr<OcrLite> engine;
    bool initialized = false;
    std::mutex mutex;
};

OcrEngine::OcrEngine(
    QString modelsDirectory
) : impl(std::make_unique<Impl>(OcrEngineConfig{.modelsDirectory = std::move(modelsDirectory)})) {
}

OcrEngine::OcrEngine(
    OcrEngineConfig config
) : impl(std::make_unique<Impl>(std::move(config))) {
}

OcrEngine::~OcrEngine() = default;

bool OcrEngine::initialize(
    QString* error
) const {
    return impl->initialize(error);
}

OcrRunResult OcrEngine::recognize(
    const cv::Mat& image
) {
    return impl->recognize(image);
}

namespace {

    std::mutex& providerMutex() {
        static std::mutex mutex;
        return mutex;
    }

} // namespace

OcrRunResult OCR::recognize(
    const cv::Mat& rawImg,
    const QRect& region
) {
    OcrRunResult output;
    if (rawImg.empty()) {
        output.error = QStringLiteral("OCR输入图片为空");
        return output;
    }
    if (!env.ocr) {
        output.error = QStringLiteral("未配置OCR Provider");
        return output;
    }

    const QRect bounds = imageBounds(rawImg);
    const QRect effectiveRegion = region.isEmpty() ? bounds : region.intersected(bounds);
    output.region = effectiveRegion;
    if (effectiveRegion.isEmpty()) {
        output.error = QStringLiteral("OCR裁剪区域不在图片范围内");
        return output;
    }

    cv::Mat croppedBgr;
    try {
        const cv::Rect
            cropRect(effectiveRegion.x(), effectiveRegion.y(), effectiveRegion.width(), effectiveRegion.height());
        if (const cv::Mat cropped = rawImg(cropRect).clone(); !toBgr8(cropped, croppedBgr, output.error)) return output;
    } catch (const std::exception& exception) {
        output.error = QStringLiteral("OCR裁剪失败：") + exceptionText(exception);
        return output;
    } catch (...) {
        output.error = QStringLiteral("OCR裁剪失败：未知异常");
        return output;
    }

    OcrRunResult local;
    try {
        std::lock_guard lock(providerMutex());
        local = env.ocr->recognize(croppedBgr);
    } catch (const std::exception& exception) {
        output.error = QStringLiteral("OCR Provider异常：") + exceptionText(exception);
        return output;
    } catch (...) {
        output.error = QStringLiteral("OCR Provider异常：未知异常");
        return output;
    }

    output = std::move(local);
    output.region = effectiveRegion;

    if (!output.ok) {
        output.tokens.clear();
        if (output.error.isEmpty()) output.error = QStringLiteral("OCR Provider识别失败");
        return output;
    }

    const QPoint offset = effectiveRegion.topLeft();
    for (OcrToken& token : output.tokens) {
        token.box.translate(offset);
        token.center += offset;
    }
    output.error.clear();
    return output;
}

QRect OCR::resolveRegion(
    const cv::Mat& rawImg,
    const QRect& explicitRegion,
    const Previous onPrevious,
    const Segment* previous,
    const QMargins& padding
) {
    const QRect bounds = imageBounds(rawImg);
    if (bounds.isEmpty()) return {};

    const QRect result = explicitRegion.isEmpty() ? bounds : explicitRegion.intersected(bounds);
    if (onPrevious != Previous::NONE && !previous) {
        throw std::runtime_error("Previous segment为空，无法派生OCR裁剪区域");
    }
    if (result.isEmpty()) return result;

    const int left = previous ? std::clamp(previous->x, 0, bounds.width()) : 0;
    const int top = previous ? std::clamp(previous->y, 0, bounds.height()) : 0;
    const int right = previous ? std::clamp(previous->right(), 0, bounds.width()) : 0;
    const int bottom = previous ? std::clamp(previous->bottom(), 0, bounds.height()) : 0;

    QRect derived;
    switch (onPrevious) {
        case Previous::NONE:
            return result;
        case Previous::LEFT:
            derived = QRect(0, 0, left, bounds.height());
            break;
        case Previous::TOP:
            derived = QRect(0, 0, bounds.width(), top);
            break;
        case Previous::RIGHT:
            derived = QRect(right, 0, bounds.width() - right, bounds.height());
            break;
        case Previous::DOWN:
            derived = QRect(0, bottom, bounds.width(), bounds.height() - bottom);
            break;
        case Previous::LEFT_CENTER:
            derived = QRect(0, top, left, std::max(0, bottom - top));
            break;
        case Previous::TOP_CENTER:
            derived = QRect(left, 0, std::max(0, right - left), top);
            break;
        case Previous::RIGHT_CENTER:
            derived = QRect(right, top, bounds.width() - right, std::max(0, bottom - top));
            break;
        case Previous::DOWN_CENTER:
            derived = QRect(left, bottom, std::max(0, right - left), bounds.height() - bottom);
            break;
        case Previous::INNER:
            derived = QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));
            break;
        default:
            throw std::invalid_argument("未知Previous枚举值");
    }

    derived = derived.marginsAdded(padding).intersected(bounds);
    return result.intersected(derived);
}
