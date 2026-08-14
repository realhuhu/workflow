#ifndef WORKFLOW_CORE_TYPES_H
#define WORKFLOW_CORE_TYPES_H

#include <QString>

inline constexpr int WheelDelta = 120;

enum class Mode {
    GRAY,
    RGB,
};

enum class Click {
    CENTER,
    LEFT,
    TOP,
    RIGHT,
    DOWN,
};

enum class Previous {
    LEFT,
    TOP,
    RIGHT,
    DOWN,
    LEFT_CENTER,
    TOP_CENTER,
    RIGHT_CENTER,
    DOWN_CENTER,
    INNER,
    NONE,
};

enum class MatchKind {
    IMAGE,
    TEXT,
};

enum class TextMatch {
    EXACT,
    CONTAINS,
    REGEX,
    FUZZY,
};

QString PreviousToString(Previous previous);
QString TextMatchToString(TextMatch match);

#endif // WORKFLOW_CORE_TYPES_H
