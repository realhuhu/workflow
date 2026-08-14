#ifndef WORKFLOW_PLATFORM_PLATFORM_H
#define WORKFLOW_PLATFORM_PLATFORM_H

#include <windows.h>

#include <opencv2/core.hpp>

#include <type_traits>

#include "core/types.h"

class Platform {
public:
    virtual ~Platform() = default;

    [[nodiscard]] virtual cv::Mat getScreen(std::remove_pointer_t<HWND>* hwnd, Mode mode) = 0;
    virtual void moveTo(std::remove_pointer_t<HWND>* hwnd, int x, int y) = 0;
    virtual void leftDown(std::remove_pointer_t<HWND>* hwnd, int x, int y) = 0;
    virtual void leftUp(std::remove_pointer_t<HWND>* hwnd, int x, int y) = 0;
    virtual void click(
        std::remove_pointer_t<HWND>* hwnd,
        const int x,
        const int y
    ) {
        leftDown(hwnd, x, y);
        leftUp(hwnd, x, y);
    }
    virtual void wheel(std::remove_pointer_t<HWND>* hwnd, int x, int y, int delta);
    virtual void drag(std::remove_pointer_t<HWND>* hwnd, int xStart, int yStart, int xEnd, int yEnd) = 0;
};

Platform& defaultPlatform();

#endif // WORKFLOW_PLATFORM_PLATFORM_H
