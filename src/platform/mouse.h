#ifndef WORKFLOW_PLATFORM_MOUSE_H
#define WORKFLOW_PLATFORM_MOUSE_H

#include <windows.h>

#include <type_traits>

class Mouse {
public:
    static void moveTo(std::remove_pointer_t<HWND>* hwnd, int x, int y);
    static void leftDown(std::remove_pointer_t<HWND>* hwnd, int x, int y);
    static void leftUp(std::remove_pointer_t<HWND>* hwnd, int x, int y);
    static void click(std::remove_pointer_t<HWND>* hwnd, int x, int y);
    static void wheel(std::remove_pointer_t<HWND>* hwnd, int x, int y, int delta);
    static void drag(std::remove_pointer_t<HWND>* hwnd, int xStart, int yStart, int xEnd, int yEnd);
};

#endif // WORKFLOW_PLATFORM_MOUSE_H
