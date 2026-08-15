#ifndef WORKFLOW_CLICKERS_BASE_H
#define WORKFLOW_CLICKERS_BASE_H

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/segment.h"
#include "core/types.h"
#include "matching/selector.h"

class Until;

template <typename RunConfig> struct RunConfigAdapter;

class ClickerBase {
public:
    QString target;
    MatchKind kind;
    std::vector<Segment> targetSegmentList;
    std::unique_ptr<Segment> previousSegment;

    virtual ~ClickerBase() = default;
    ClickerBase(const ClickerBase&) = delete;
    ClickerBase& operator=(const ClickerBase&) = delete;

    [[nodiscard]] std::unique_ptr<ClickerBase> _createNext(
        const std::vector<std::unique_ptr<Until>>& runUntilList,
        const std::vector<std::unique_ptr<Until>>& finishUntilList
    ) const;

    void _start(float startWait, const std::vector<std::unique_ptr<Until>>& startUntilList);

    [[nodiscard]] std::unique_ptr<ClickerBase> _run(
        const QString& name,
        const std::function<void()>& executor,
        float startWait,
        float finishWait,
        const std::vector<Until*>& startUntilList,
        const std::vector<Until*>& runUntilList,
        const std::vector<Until*>& finishUntilList,
        bool homing
    );

    void _finish(float finishWait, const std::vector<std::unique_ptr<Until>>& finishUntilList);

    [[nodiscard]] virtual std::unique_ptr<ClickerBase> locate();

    template <typename RunConfig>
        requires requires(const RunConfig& config) {
            config.startWait;
            config.startUntilList;
            config.runUntilList;
            config.finishUntilList;
            config.finishWait;
            config.homing;
            RunConfigAdapter<RunConfig>::selector(config);
        }
    [[nodiscard]] std::unique_ptr<ClickerBase> locate(
        RunConfig runConfig
    ) {
        return locateResolved(resolveRunConfig(runConfig));
    }

    [[nodiscard]] virtual std::unique_ptr<ClickerBase> click(
        float interval = 1,
        int offsetX = 0,
        int offsetY = 0,
        Click position = Click::CENTER
    );

    template <typename RunConfig>
        requires requires(const RunConfig& config) {
            config.startWait;
            config.startUntilList;
            config.runUntilList;
            config.finishUntilList;
            config.finishWait;
            config.homing;
            RunConfigAdapter<RunConfig>::selector(config);
        }
    [[nodiscard]] std::unique_ptr<ClickerBase> click(
        RunConfig runConfig,
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) {
        return clickResolved(resolveRunConfig(runConfig), interval, offsetX, offsetY, position);
    }

    [[nodiscard]] virtual std::unique_ptr<ClickerBase> drag(int step = 10, bool reverse = false);

    template <typename RunConfig>
        requires requires(const RunConfig& config) {
            config.startWait;
            config.startUntilList;
            config.runUntilList;
            config.finishUntilList;
            config.finishWait;
            config.homing;
            RunConfigAdapter<RunConfig>::selector(config);
        }
    [[nodiscard]] std::unique_ptr<ClickerBase> drag(
        RunConfig runConfig,
        const int step = 10,
        const bool reverse = false
    ) {
        return dragResolved(resolveRunConfig(runConfig), step, reverse);
    }

    [[nodiscard]] virtual std::unique_ptr<ClickerBase> scroll(
        int delta = -WheelDelta,
        float interval = 1,
        int offsetX = 0,
        int offsetY = 0,
        Click position = Click::CENTER
    );

    template <typename RunConfig>
        requires requires(const RunConfig& config) {
            config.startWait;
            config.startUntilList;
            config.runUntilList;
            config.finishUntilList;
            config.finishWait;
            config.homing;
            RunConfigAdapter<RunConfig>::selector(config);
        }
    [[nodiscard]] std::unique_ptr<ClickerBase> scroll(
        RunConfig runConfig,
        const int delta = -WheelDelta,
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) {
        return scrollResolved(resolveRunConfig(runConfig), delta, interval, offsetX, offsetY, position);
    }

    static void end();

    [[nodiscard]] bool founded() const;

    [[nodiscard]] virtual QString toString() const;

protected:
    struct ResolvedRunConfig {
        float startWait = 0;
        Selector selector;
        std::vector<Until*> startUntilList{};
        std::vector<Until*> runUntilList{};
        std::vector<Until*> finishUntilList{};
        float finishWait = 0;
        bool homing = true;
    };

    explicit ClickerBase(QString target, MatchKind kind);
    explicit ClickerBase(QString target, const Segment& segment, MatchKind kind);
    explicit ClickerBase(QString target, const std::vector<Segment>& segmentList, MatchKind kind);

    void initialize(float wait);

    static void validateLocateConfig(
        const std::vector<Until*>& startUntilList,
        const std::vector<Until*>& runUntilList,
        const std::vector<Until*>& finishUntilList
    );

    [[nodiscard]] virtual std::vector<Segment> matchTarget() = 0;
    [[nodiscard]] virtual std::unique_ptr<ClickerBase> clone() const = 0;
    [[nodiscard]] virtual float timeout() const = 0;
    [[nodiscard]] virtual ResolvedRunConfig defaultRunConfig() const = 0;

    [[nodiscard]] std::unique_ptr<ClickerBase> locateResolved(ResolvedRunConfig config);

    [[nodiscard]] std::unique_ptr<ClickerBase> clickResolved(
        ResolvedRunConfig config,
        float interval,
        int offsetX,
        int offsetY,
        Click position
    );

    [[nodiscard]] std::unique_ptr<ClickerBase> dragResolved(ResolvedRunConfig config, int step, bool reverse);

    [[nodiscard]] std::unique_ptr<ClickerBase> scrollResolved(
        ResolvedRunConfig config,
        int delta,
        float interval,
        int offsetX,
        int offsetY,
        Click position
    );

    template <typename RunConfig>
    [[nodiscard]] ResolvedRunConfig resolveRunConfig(
        const RunConfig& runConfig
    ) const {
        if (kind != RunConfigAdapter<RunConfig>::kind) {
            throw std::invalid_argument("RunConfig与Clicker的MatchKind不一致");
        }
        return {
            .startWait = runConfig.startWait,
            .selector = RunConfigAdapter<RunConfig>::selector(runConfig),
            .startUntilList = runConfig.startUntilList,
            .runUntilList = runConfig.runUntilList,
            .finishUntilList = runConfig.finishUntilList,
            .finishWait = runConfig.finishWait,
            .homing = runConfig.homing,
        };
    }
};

template <typename InitConfig, typename RunConfig> class Clicker : public ClickerBase {
public:
    InitConfig config;

    [[nodiscard]] std::unique_ptr<ClickerBase> locate() override {
        return locateResolved(this->resolveRunConfig(RunConfig{}));
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> locate(
        RunConfig runConfig
    ) {
        return locateResolved(this->resolveRunConfig(runConfig));
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> click(
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) override {
        return clickResolved(this->resolveRunConfig(RunConfig{}), interval, offsetX, offsetY, position);
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> click(
        RunConfig runConfig,
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) {
        return clickResolved(this->resolveRunConfig(runConfig), interval, offsetX, offsetY, position);
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> drag(
        const int step = 10,
        const bool reverse = false
    ) override {
        return dragResolved(this->resolveRunConfig(RunConfig{}), step, reverse);
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> drag(
        RunConfig runConfig,
        const int step = 10,
        const bool reverse = false
    ) {
        return dragResolved(this->resolveRunConfig(runConfig), step, reverse);
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> scroll(
        const int delta = -WheelDelta,
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) override {
        return scrollResolved(this->resolveRunConfig(RunConfig{}), delta, interval, offsetX, offsetY, position);
    }

    [[nodiscard]] std::unique_ptr<ClickerBase> scroll(
        RunConfig runConfig,
        const int delta = -WheelDelta,
        const float interval = 1,
        const int offsetX = 0,
        const int offsetY = 0,
        const Click position = Click::CENTER
    ) {
        return scrollResolved(this->resolveRunConfig(runConfig), delta, interval, offsetX, offsetY, position);
    }

protected:
    Clicker(
        QString target,
        const MatchKind kind,
        InitConfig initConfig
    ) : ClickerBase(std::move(target), kind), config(std::move(initConfig)) {
    }

    Clicker(
        QString target,
        const Segment& segment,
        const MatchKind kind,
        InitConfig initConfig
    ) : ClickerBase(std::move(target), segment, kind), config(std::move(initConfig)) {
    }

    Clicker(
        QString target,
        const std::vector<Segment>& segmentList,
        const MatchKind kind,
        InitConfig initConfig
    ) : ClickerBase(std::move(target), segmentList, kind), config(std::move(initConfig)) {
    }

    [[nodiscard]] float timeout() const override {
        return config.timeout;
    }

    [[nodiscard]] ResolvedRunConfig defaultRunConfig() const override {
        return this->resolveRunConfig(RunConfig{});
    }
};

#endif // WORKFLOW_CLICKERS_BASE_H
