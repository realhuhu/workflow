#include "platform/platform.h"

#include <opencv2/imgproc.hpp>

#include <stdexcept>
#include <type_traits>

#include "platform/mouse.h"

namespace {
    using Window = std::remove_pointer_t<HWND>;

    class Win32Platform final : public Platform {
    public:
        cv::Mat getScreen(
            Window* const hwnd,
            const Mode mode
        ) override {
            if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd)) return {};

            RECT rect{};
            if (!GetClientRect(hwnd, &rect)) return {};
            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            if (width <= 0 || height <= 0) return {};

            const auto windowDc = GetDC(hwnd);
            if (!windowDc) return {};

            const auto memoryDc = CreateCompatibleDC(windowDc);
            const auto bitmap = memoryDc ? CreateCompatibleBitmap(windowDc, width, height) : nullptr;
            auto* const oldObject = bitmap ? SelectObject(memoryDc, bitmap) : nullptr;
            if (!memoryDc || !bitmap || !oldObject) {
                if (bitmap) DeleteObject(bitmap);
                if (memoryDc) DeleteDC(memoryDc);
                ReleaseDC(hwnd, windowDc);
                return {};
            }

            // PW_RENDERFULLCONTENT is only available on newer Windows versions.
            // Windows 7 accepts PW_CLIENTONLY; retrying with it also protects
            // legacy systems that reject the newer flag bit.
            BOOL printed = PrintWindow(hwnd, memoryDc, PW_CLIENTONLY | 0x00000002);
            if (!printed) printed = PrintWindow(hwnd, memoryDc, PW_CLIENTONLY);
            // GetDIBits requires the bitmap not to be selected into a DC.
            SelectObject(memoryDc, oldObject);
            const cv::Mat bgra(height, width, CV_8UC4);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            const int copied =
                printed ? GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(height), bgra.data, &info, DIB_RGB_COLORS)
                        : 0;

            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            ReleaseDC(hwnd, windowDc);

            if (!printed || copied != height) return {};

            cv::Mat bgr;
            cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
            if (bgr.empty()) return {};
            if (mode == Mode::RGB) return bgr;

            cv::Mat gray;
            cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
            return gray;
        }

        void moveTo(
            Window* const hwnd,
            const int x,
            const int y
        ) override {
            Mouse::moveTo(hwnd, x, y);
        }
        void leftDown(
            Window* const hwnd,
            const int x,
            const int y
        ) override {
            Mouse::leftDown(hwnd, x, y);
        }
        void leftUp(
            Window* const hwnd,
            const int x,
            const int y
        ) override {
            Mouse::leftUp(hwnd, x, y);
        }
        void click(
            Window* const hwnd,
            const int x,
            const int y
        ) override {
            Mouse::click(hwnd, x, y);
        }
        void wheel(
            Window* const hwnd,
            const int x,
            const int y,
            const int delta
        ) override {
            Mouse::wheel(hwnd, x, y, delta);
        }

        void drag(
            Window* const hwnd,
            const int xStart,
            const int yStart,
            const int xEnd,
            const int yEnd
        ) override {
            Mouse::drag(hwnd, xStart, yStart, xEnd, yEnd);
        }
    };

} // namespace

void Platform::wheel(
    HWND,
    int,
    int,
    int
) {
    // Preserve source compatibility for existing injected platforms while
    // making unsupported wheel input fail loudly instead of becoming a no-op.
    throw std::runtime_error("当前Platform未实现滚轮输入");
}

Platform& defaultPlatform() {
    static Win32Platform platform;
    return platform;
}
