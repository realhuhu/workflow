#ifndef WORKFLOW_FLOW_WORKFLOW_H
#define WORKFLOW_FLOW_WORKFLOW_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <cstddef>
#include <memory>

#include "clickers/base.h"

class Workflow final {
public:
    Workflow(const Workflow&) = default;
    Workflow& operator=(const Workflow&) = default;
    Workflow(Workflow&&) noexcept = default;
    Workflow& operator=(Workflow&&) noexcept = default;
    ~Workflow() = default;

    [[nodiscard]] const QString& name() const;
    [[nodiscard]] std::size_t stepCount() const;
    [[nodiscard]] std::unique_ptr<ClickerBase> run() const;

private:
    class Impl;

    explicit Workflow(std::shared_ptr<const Impl> impl);

    std::shared_ptr<const Impl> impl;

    friend Workflow parseWorkflow(const QJsonObject& object);
};

[[nodiscard]] Workflow parseWorkflow(const QJsonObject& object);
[[nodiscard]] Workflow parseWorkflow(const QByteArray& json);
[[nodiscard]] Workflow parseWorkflowFile(const QString& path);

#endif // WORKFLOW_FLOW_WORKFLOW_H
