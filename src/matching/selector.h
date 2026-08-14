#ifndef WORKFLOW_MATCHING_SELECTOR_H
#define WORKFLOW_MATCHING_SELECTOR_H

#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

#include "core/segment.h"

template <typename T>
T choice(
    const std::vector<T>& values
) {
    if (values.empty()) throw std::runtime_error("向量不能为空");
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    const std::uniform_int_distribution<size_t> distribution(0, values.size() - 1);
    return values[distribution(generator)];
}

using Selector = std::function<Segment(const std::vector<Segment>&)>;

enum class SelectorBasis {
    X1,
    Y1,
    X2,
    Y2,
    X_CENTER,
    Y_CENTER,
};

enum class SelectorMethod {
    MIN,
    MAX,
};

Segment similaritySelector(const std::vector<Segment>& segments);
Selector positionSelector(SelectorBasis basis, SelectorMethod method);
Segment randomSelector(const std::vector<Segment>& segments);
Selector orderedRandomSelector(SelectorBasis basis, SelectorMethod method, size_t top);

#endif // WORKFLOW_MATCHING_SELECTOR_H
