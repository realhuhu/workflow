#include "core/segment.h"

#include "core/environment.h"
#include "platform/platform.h"
#include "support/logging.h"
#include "support/timing.h"

namespace {

    Platform& platform() {
        return env.platform ? *env.platform : defaultPlatform();
    }

} // namespace

Segment::Segment(
    const int x,
    const int y,
    const int width,
    const int height,
    const float score
) : x(x), y(y), width(width), height(height), score(score) {
}

Segment::Relation Segment::on(
    const Segment& segment,
    const Axis axis
) const {
    switch (axis) {
        case Axis::HORIZONTAL:
            if (right() <= segment.x) return Relation::LEFT;
            if (x >= segment.right()) return Relation::RIGHT;
            if (x >= segment.x && right() <= segment.right()) return Relation::CENTER;
            return Relation::NONE;
        case Axis::VERTICAL:
            if (bottom() <= segment.y) return Relation::TOP;
            if (y >= segment.bottom()) return Relation::DOWN;
            if (y >= segment.y && bottom() <= segment.bottom()) return Relation::CENTER;
            return Relation::NONE;
    }
    return Relation::NONE;
}

Segment Segment::copy() const {
    return {x, y, width, height, score};
}

int Segment::right() const {
    return x + width;
}

int Segment::bottom() const {
    return y + height;
}

int Segment::centerX() const {
    return x + width / 2;
}

int Segment::centerY() const {
    return y + height / 2;
}

bool operator==(
    const Segment& a,
    const Segment& b
) {
    return a.centerX() == b.centerX() && a.centerY() == b.centerY();
}

QString Segment::toString() const {
    return QString("(%1, %2)").arg(centerX()).arg(centerY());
}

void Segment::click(
    const float wait,
    const int offsetX,
    const int offsetY,
    const Click position
) const {
    if (stopped(env.stopFlag)) return;

    int clickX = centerX();
    int clickY = centerY();
    switch (position) {
        case Click::CENTER:
            break;
        case Click::LEFT:
            clickX = x;
            break;
        case Click::TOP:
            clickY = y;
            break;
        case Click::RIGHT:
            clickX = right() - 1;
            break;
        case Click::DOWN:
            clickY = bottom() - 1;
            break;
    }
    clickX += offsetX;
    clickY += offsetY;

    platform().click(env.hwnd, clickX, clickY);
    logMessage(QString("点击: (%1,%2)").arg(clickX).arg(clickY));
    sleep(env.stopFlag, wait);
}

void Segment::drag(
    const float wait,
    const int distance
) const {
    if (stopped(env.stopFlag)) return;

    const int xStart = centerX();
    const int yStart = centerY();
    const int xEnd = xStart;
    const int yEnd = yStart + distance;
    platform().drag(env.hwnd, xStart, yStart, xEnd, yEnd);
    logMessage(QString("拖动: 从(%1,%2)到(%3,%4)").arg(xStart).arg(yStart).arg(xEnd).arg(yEnd));
    sleep(env.stopFlag, wait);
}

void Segment::scroll(
    const int delta,
    const float wait,
    const int offsetX,
    const int offsetY,
    const Click position
) const {
    if (stopped(env.stopFlag)) return;

    int scrollX = centerX();
    int scrollY = centerY();
    switch (position) {
        case Click::CENTER:
            break;
        case Click::LEFT:
            scrollX = x;
            break;
        case Click::TOP:
            scrollY = y;
            break;
        case Click::RIGHT:
            scrollX = right() - 1;
            break;
        case Click::DOWN:
            scrollY = bottom() - 1;
            break;
    }
    scrollX += offsetX;
    scrollY += offsetY;

    platform().wheel(env.hwnd, scrollX, scrollY, delta);
    logMessage(QString("滚轮: (%1,%2), delta=%3").arg(scrollX).arg(scrollY).arg(delta));
    sleep(env.stopFlag, wait);
}
