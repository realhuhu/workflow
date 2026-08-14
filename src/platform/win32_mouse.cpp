#include "platform/mouse.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

namespace {

    using Window = std::remove_pointer_t<HWND>;

    std::mutex mouseMutex;

    LPARAM mousePosition(
        const int x,
        const int y
    ) {
        return MAKELPARAM(static_cast<SHORT>(x), static_cast<SHORT>(y));
    }

    RECT clientRect(
        Window* const hwnd
    ) {
        if (!hwnd || !IsWindow(hwnd)) throw std::runtime_error("HWND无效");
        RECT client{};
        if (!GetClientRect(hwnd, &client)) {
            throw std::runtime_error("GetClientRect失败，Win32错误=" + std::to_string(GetLastError()));
        }
        if (client.right <= client.left || client.bottom <= client.top) {
            throw std::runtime_error("窗口客户区尺寸无效");
        }
        return client;
    }

    void validatePoint(
        Window* const hwnd,
        const int x,
        const int y
    ) {
        if (const auto [left, top, right, bottom] = clientRect(hwnd);
            x < left || y < top || x >= right || y >= bottom) {
            throw std::out_of_range("坐标超出窗口客户区: (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    }

    void postChecked(
        Window* const hwnd,
        const UINT message,
        const WPARAM wParam,
        const LPARAM lParam
    ) {
        if (PostMessageW(hwnd, message, wParam, lParam)) return;
        throw std::runtime_error("PostMessage失败，Win32错误=" + std::to_string(GetLastError()));
    }

    void restoreForegroundWindow(
        Window* const original
    ) {
        // The foreground window can disappear while the synthetic input settles.
        // Revalidate at the point of use instead of relying on an earlier check.
        if (!IsWindow(original)) return;
        auto* const currentForeground = GetForegroundWindow();
        if (currentForeground == original) return;

        const DWORD currentThread = GetCurrentThreadId();
        const DWORD foregroundThread = currentForeground ? GetWindowThreadProcessId(currentForeground, nullptr) : 0;
        const DWORD originalThread = GetWindowThreadProcessId(original, nullptr);
        const bool attachedForeground = foregroundThread && foregroundThread != currentThread &&
                                        AttachThreadInput(currentThread, foregroundThread, TRUE);
        const bool attachedOriginal = originalThread && originalThread != currentThread &&
                                      originalThread != foregroundThread &&
                                      AttachThreadInput(currentThread, originalThread, TRUE);
        SetForegroundWindow(original);
        if (attachedOriginal) AttachThreadInput(currentThread, originalThread, FALSE);
        if (attachedForeground) AttachThreadInput(currentThread, foregroundThread, FALSE);
    }

    void restoreForegroundIfTargetActivated(
        Window* const original,
        Window* const target
    ) {
        const auto* const targetRoot = GetAncestor(target, GA_ROOT);
        if (!original || original == targetRoot || original == target) return;

        constexpr auto settleTime = std::chrono::milliseconds(40);
        const auto deadline = std::chrono::steady_clock::now() + settleTime;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (const auto* const current = GetForegroundWindow(); current == targetRoot || current == target) {
                restoreForegroundWindow(original);
            } else if (current != original) {
                // A third window means real user activity; never override it.
                return;
            }
        } while (std::chrono::steady_clock::now() < deadline);

        if (const auto* const current = GetForegroundWindow(); current == targetRoot || current == target) {
            restoreForegroundWindow(original);
        }
    }

} // namespace

void Mouse::moveTo(
    Window* const hwnd,
    const int x,
    const int y
) {
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, x, y);
    postChecked(hwnd, WM_MOUSEMOVE, 0, mousePosition(x, y));
}

void Mouse::leftDown(
    Window* const hwnd,
    const int x,
    const int y
) {
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, x, y);
    postChecked(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, mousePosition(x, y));
}

void Mouse::leftUp(
    Window* const hwnd,
    const int x,
    const int y
) {
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, x, y);
    postChecked(hwnd, WM_LBUTTONUP, 0, mousePosition(x, y));
}

void Mouse::click(
    Window* const hwnd,
    const int x,
    const int y
) {
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, x, y);
    const LPARAM position = mousePosition(x, y);
    postChecked(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, position);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    postChecked(hwnd, WM_LBUTTONUP, 0, position);
}

void Mouse::wheel(
    Window* const hwnd,
    const int x,
    const int y,
    const int delta
) {
    // Keep the hover move and wheel event atomic with respect to every other
    // workflow pointer operation. WM_MOUSEWHEEL uses screen coordinates even
    // though the public API (and the preceding WM_MOUSEMOVE) use client ones.
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, x, y);
    if (delta < SHRT_MIN || delta > SHRT_MAX) {
        throw std::out_of_range("滚轮delta超出16位有符号范围: " + std::to_string(delta));
    }

    POINT screenPoint{x, y};
    if (!ClientToScreen(hwnd, &screenPoint)) {
        throw std::runtime_error("ClientToScreen失败，Win32错误=" + std::to_string(GetLastError()));
    }

    auto* const foreground = GetForegroundWindow();
    const WPARAM wheelParam = MAKEWPARAM(0, static_cast<SHORT>(delta));
    const LPARAM screenParam = MAKELPARAM(static_cast<SHORT>(screenPoint.x), static_cast<SHORT>(screenPoint.y));
    try {
        postChecked(hwnd, WM_MOUSEMOVE, 0, mousePosition(x, y));
        postChecked(hwnd, WM_MOUSEWHEEL, wheelParam, screenParam);
    } catch (...) {
        restoreForegroundIfTargetActivated(foreground, hwnd);
        throw;
    }
    restoreForegroundIfTargetActivated(foreground, hwnd);
}

void Mouse::drag(
    Window* const hwnd,
    const int xStart,
    const int yStart,
    int xEnd,
    int yEnd
) {
    // A drag must not be interleaved with clicks sent by another worker.
    std::lock_guard guard(mouseMutex);
    validatePoint(hwnd, xStart, yStart);
    const auto [left, top, right, bottom] = clientRect(hwnd);
    xEnd = std::clamp(xEnd, static_cast<int>(left), static_cast<int>(right - 1));
    yEnd = std::clamp(yEnd, static_cast<int>(top), static_cast<int>(bottom - 1));
    const LPARAM start = mousePosition(xStart, yStart);
    const LPARAM end = mousePosition(xEnd, yEnd);

    postChecked(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, start);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    postChecked(hwnd, WM_MOUSEMOVE, MK_LBUTTON, end);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    postChecked(hwnd, WM_LBUTTONUP, 0, end);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
