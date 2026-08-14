#ifndef WORKFLOW_MATCHING_IMAGE_H
#define WORKFLOW_MATCHING_IMAGE_H

#include <windows.h>

#include <QRect>

#include <opencv2/core.hpp>

#include <type_traits>
#include <vector>

#include "core/segment.h"
#include "core/types.h"

// The class name intentionally remains CV to preserve the Conqueror API.
class CV {
public:
    static cv::Mat getScreen(std::remove_pointer_t<HWND>* hwnd, Mode mode = Mode::GRAY);

    static std::vector<Segment> findPositions(
        const cv::Mat& rawImg,
        const QString& templatePath,
        float threshold = 0.9f,
        Mode mode = Mode::GRAY,
        const QRect& region = {}
    );
};

#endif // WORKFLOW_MATCHING_IMAGE_H
