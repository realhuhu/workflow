#include "matching/selector.h"

#include <algorithm>
#include <stdexcept>

namespace {

    auto coordinateComparator(
        const SelectorBasis basis
    ) {
        return [basis](const Segment& left, const Segment& right) {
            switch (basis) {
                case SelectorBasis::X1:
                    return left.x < right.x;
                case SelectorBasis::Y1:
                    return left.y < right.y;
                case SelectorBasis::X2:
                    return left.right() < right.right();
                case SelectorBasis::Y2:
                    return left.bottom() < right.bottom();
                case SelectorBasis::X_CENTER:
                    return left.centerX() < right.centerX();
                case SelectorBasis::Y_CENTER:
                    return left.centerY() < right.centerY();
            }
            throw std::invalid_argument("未知SelectorBasis");
        };
    }

    bool descending(
        const SelectorMethod method
    ) {
        switch (method) {
            case SelectorMethod::MIN:
                return false;
            case SelectorMethod::MAX:
                return true;
        }
        throw std::invalid_argument("未知SelectorMethod");
    }

} // namespace

Segment similaritySelector(
    const std::vector<Segment>& segments
) {
    if (segments.empty()) throw std::runtime_error("similaritySelector: 列表为空");
    return *std::ranges::max_element(segments, [](const Segment& left, const Segment& right) {
        return left.score < right.score;
    });
}

Selector positionSelector(
    const SelectorBasis basis,
    const SelectorMethod method
) {
    return [basis, method](const std::vector<Segment>& segments) {
        if (segments.empty()) throw std::runtime_error("positionSelector: 列表为空");
        auto sorted = segments;
        std::ranges::sort(sorted, coordinateComparator(basis));
        if (descending(method)) std::ranges::reverse(sorted);
        return sorted.front();
    };
}

Segment randomSelector(
    const std::vector<Segment>& segments
) {
    if (segments.empty()) throw std::runtime_error("randomSelector: 列表为空");
    return choice(segments);
}

Selector orderedRandomSelector(
    const SelectorBasis basis,
    const SelectorMethod method,
    size_t top
) {
    return [basis, method, top](const std::vector<Segment>& segments) {
        if (segments.empty()) throw std::runtime_error("orderedRandomSelector: 列表为空");
        if (top == 0) throw std::invalid_argument("orderedRandomSelector: top必须大于0");

        auto sorted = segments;
        std::ranges::sort(sorted, coordinateComparator(basis));
        if (descending(method)) std::ranges::reverse(sorted);
        const size_t count = (std::min)(top, sorted.size());
        sorted.erase(sorted.begin() + static_cast<std::ptrdiff_t>(count), sorted.end());
        return choice(sorted);
    };
}
