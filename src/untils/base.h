#ifndef WORKFLOW_UNTILS_BASE_H
#define WORKFLOW_UNTILS_BASE_H

#include <memory>
#include <vector>

#include "core/segment.h"
#include "core/types.h"

class ImageUntil;
class TextUntil;

class Until {
public:
    QString target;
    const MatchKind kind;

    virtual ~Until() = default;
    virtual bool loop(std::unique_ptr<Segment>& previous, float globalTimeout);
    virtual void preHook(std::unique_ptr<Segment>& previous);
    [[nodiscard]] virtual bool flag(std::unique_ptr<Segment>& previous);
    [[nodiscard]] virtual QString toString() const = 0;

    bool fulfilled(std::unique_ptr<Segment>& previous);
    [[nodiscard]] bool isReversed() const;

    [[nodiscard]] std::vector<Segment> filter(
        const std::vector<Segment>& positions,
        const std::unique_ptr<Segment>& previous
    ) const;

protected:
    struct RuntimeConfig {
        Previous onPrevious = Previous::NONE;
        float interval = 0.1f;
        float startWait = 0;
        float finishWait = 0;
        float timeout = -1;
        bool reverse = false;
    };

    [[nodiscard]] virtual std::vector<Segment> scan(std::unique_ptr<Segment>& previous) = 0;
    [[nodiscard]] virtual RuntimeConfig runtimeConfig() const = 0;

private:
    friend class ImageUntil;
    friend class TextUntil;

    Until(QString target, MatchKind kind);
};

#endif // WORKFLOW_UNTILS_BASE_H
