#ifndef WORKFLOW_CORE_SEGMENT_H
#define WORKFLOW_CORE_SEGMENT_H

#include "core/types.h"

class Segment {
public:
    enum class Axis {
        HORIZONTAL,
        VERTICAL,
    };

    enum class Relation {
        LEFT,
        TOP,
        RIGHT,
        DOWN,
        CENTER,
        NONE,
    };

    int x;
    int y;
    int width;
    int height;
    float score;

    Segment(int x, int y, int width, int height, float score);

    void click(float wait = 0.1, int offsetX = 0, int offsetY = 0, Click position = Click::CENTER) const;

    void drag(float wait = 0.1, int distance = 0) const;

    void scroll(
        int delta = -WheelDelta,
        float wait = 0.1,
        int offsetX = 0,
        int offsetY = 0,
        Click position = Click::CENTER
    ) const;

    [[nodiscard]] Relation on(const Segment& segment, Axis axis) const;
    [[nodiscard]] Segment copy() const;
    [[nodiscard]] int right() const;
    [[nodiscard]] int bottom() const;
    [[nodiscard]] int centerX() const;
    [[nodiscard]] int centerY() const;

    friend bool operator==(const Segment& a, const Segment& b);

    [[nodiscard]] QString toString() const;
};

#endif // WORKFLOW_CORE_SEGMENT_H
