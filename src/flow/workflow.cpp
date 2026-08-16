#include "flow/workflow.h"

#include "clickers/image.h"
#include "clickers/text.h"
#include "core/environment.h"
#include "support/timing.h"
#include "untils/image.h"
#include "untils/text.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMargins>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <climits>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    [[noreturn]] void invalidJson(
        const QString& path,
        const QString& message
    ) {
        throw std::invalid_argument(QString("%1: %2").arg(path, message).toUtf8().toStdString());
    }

    void rejectUnknownKeys(
        const QJsonObject& object,
        const QSet<QString>& allowed,
        const QString& path
    ) {
        for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
            if (!allowed.contains(iterator.key())) invalidJson(path + "." + iterator.key(), "未知字段");
        }
    }

    QJsonObject objectValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const bool required = false
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) {
            if (required) invalidJson(path + "." + key, "缺少对象");
            return {};
        }
        if (!value.isObject()) invalidJson(path + "." + key, "必须是对象");
        return value.toObject();
    }

    QJsonArray arrayValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const bool required = false
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) {
            if (required) invalidJson(path + "." + key, "缺少数组");
            return {};
        }
        if (!value.isArray()) invalidJson(path + "." + key, "必须是数组");
        return value.toArray();
    }

    QString stringValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const QString& defaultValue = {},
        const bool required = false
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) {
            if (required) invalidJson(path + "." + key, "缺少字符串");
            return defaultValue;
        }
        if (!value.isString()) invalidJson(path + "." + key, "必须是字符串");
        const QString result = value.toString();
        if (required && result.isEmpty()) invalidJson(path + "." + key, "不能为空");
        return result;
    }

    bool boolValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const bool defaultValue
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) return defaultValue;
        if (!value.isBool()) invalidJson(path + "." + key, "必须是布尔值");
        return value.toBool();
    }

    double numberValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const double defaultValue
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) return defaultValue;
        if (!value.isDouble() || !std::isfinite(value.toDouble())) {
            invalidJson(path + "." + key, "必须是有限数值");
        }
        return value.toDouble();
    }

    float floatValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const float defaultValue
    ) {
        const double value = numberValue(object, key, path, defaultValue);
        if (value < -std::numeric_limits<float>::max() || value > std::numeric_limits<float>::max()) {
            invalidJson(path + "." + key, "超出float范围");
        }
        return static_cast<float>(value);
    }

    int intValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path,
        const int defaultValue
    ) {
        const double value = numberValue(object, key, path, defaultValue);
        if (std::trunc(value) != value || value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) {
            invalidJson(path + "." + key, "必须是int范围内的整数");
        }
        return static_cast<int>(value);
    }

    std::vector<QString> stringList(
        const QJsonArray& array,
        const QString& path,
        const bool allowEmpty = false
    ) {
        if (array.isEmpty() && !allowEmpty) invalidJson(path, "不能为空");
        std::vector<QString> result;
        result.reserve(static_cast<std::size_t>(array.size()));
        for (int index = 0; index < array.size(); ++index) {
            if (!array[index].isString() || array[index].toString().isEmpty()) {
                invalidJson(QString("%1[%2]").arg(path).arg(index), "必须是非空字符串");
            }
            result.push_back(array[index].toString());
        }
        return result;
    }

    QStringList qStringList(
        const QJsonArray& array,
        const QString& path
    ) {
        const auto values = stringList(array, path, true);
        QStringList result;
        result.reserve(static_cast<int>(values.size()));
        for (const auto& value : values)
            result.push_back(value);
        return result;
    }

    QRect parseRect(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"x", "y", "width", "height"}, path);
        const int width = intValue(object, "width", path, 0);
        const int height = intValue(object, "height", path, 0);
        if (width < 0 || height < 0) invalidJson(path, "width和height不能为负数");
        return {intValue(object, "x", path, 0), intValue(object, "y", path, 0), width, height};
    }

    QRect rectValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) return {};
        if (!value.isObject()) invalidJson(path + "." + key, "必须是矩形对象");
        return parseRect(value.toObject(), path + "." + key);
    }

    QMargins marginsValue(
        const QJsonObject& object,
        const QString& key,
        const QString& path
    ) {
        const auto value = object.value(key);
        if (value.isUndefined()) return {};
        if (!value.isObject()) invalidJson(path + "." + key, "必须是边距对象");
        const auto margins = value.toObject();
        const QString marginsPath = path + "." + key;
        rejectUnknownKeys(margins, {"left", "top", "right", "bottom"}, marginsPath);
        const int left = intValue(margins, "left", marginsPath, 0);
        const int top = intValue(margins, "top", marginsPath, 0);
        const int right = intValue(margins, "right", marginsPath, 0);
        const int bottom = intValue(margins, "bottom", marginsPath, 0);
        if (left < 0 || top < 0 || right < 0 || bottom < 0) invalidJson(marginsPath, "边距不能为负数");
        return {left, top, right, bottom};
    }

    Mode parseMode(
        const QString& value,
        const QString& path
    ) {
        if (value == "GRAY") return Mode::GRAY;
        if (value == "RGB") return Mode::RGB;
        invalidJson(path, "必须是GRAY或RGB");
    }

    MatchKind parseMatchKind(
        const QString& value,
        const QString& path
    ) {
        if (value == "IMAGE") return MatchKind::IMAGE;
        if (value == "TEXT") return MatchKind::TEXT;
        invalidJson(path, "必须是IMAGE或TEXT");
    }

    Previous parsePrevious(
        const QString& value,
        const QString& path
    ) {
        if (value == "LEFT") return Previous::LEFT;
        if (value == "TOP") return Previous::TOP;
        if (value == "RIGHT") return Previous::RIGHT;
        if (value == "DOWN") return Previous::DOWN;
        if (value == "LEFT_CENTER") return Previous::LEFT_CENTER;
        if (value == "TOP_CENTER") return Previous::TOP_CENTER;
        if (value == "RIGHT_CENTER") return Previous::RIGHT_CENTER;
        if (value == "DOWN_CENTER") return Previous::DOWN_CENTER;
        if (value == "INNER") return Previous::INNER;
        if (value == "NONE") return Previous::NONE;
        invalidJson(path, "未知Previous值");
    }

    Click parseClick(
        const QString& value,
        const QString& path
    ) {
        if (value == "CENTER") return Click::CENTER;
        if (value == "LEFT") return Click::LEFT;
        if (value == "TOP") return Click::TOP;
        if (value == "RIGHT") return Click::RIGHT;
        if (value == "DOWN") return Click::DOWN;
        invalidJson(path, "未知Click值");
    }

    TextMatch parseTextMatch(
        const QString& value,
        const QString& path
    ) {
        if (value == "EXACT") return TextMatch::EXACT;
        if (value == "CONTAINS") return TextMatch::CONTAINS;
        if (value == "REGEX") return TextMatch::REGEX;
        if (value == "FUZZY") return TextMatch::FUZZY;
        invalidJson(path, "未知TextMatch值");
    }

    Qt::CaseSensitivity parseCaseSensitivity(
        const QString& value,
        const QString& path
    ) {
        if (value == "SENSITIVE") return Qt::CaseSensitive;
        if (value == "INSENSITIVE") return Qt::CaseInsensitive;
        invalidJson(path, "必须是SENSITIVE或INSENSITIVE");
    }

    SelectorBasis parseSelectorBasis(
        const QString& value,
        const QString& path
    ) {
        if (value == "X1") return SelectorBasis::X1;
        if (value == "Y1") return SelectorBasis::Y1;
        if (value == "X2") return SelectorBasis::X2;
        if (value == "Y2") return SelectorBasis::Y2;
        if (value == "X_CENTER") return SelectorBasis::X_CENTER;
        if (value == "Y_CENTER") return SelectorBasis::Y_CENTER;
        invalidJson(path, "未知SelectorBasis值");
    }

    SelectorMethod parseSelectorMethod(
        const QString& value,
        const QString& path
    ) {
        if (value == "MIN") return SelectorMethod::MIN;
        if (value == "MAX") return SelectorMethod::MAX;
        invalidJson(path, "必须是MIN或MAX");
    }

    struct SelectorSpec {
        enum class Type {
            SIMILARITY,
            POSITION,
            RANDOM,
            ORDERED_RANDOM,
        };

        Type type = Type::SIMILARITY;
        SelectorBasis basis = SelectorBasis::X1;
        SelectorMethod method = SelectorMethod::MIN;
        std::size_t top = 1;

        [[nodiscard]] Selector create() const {
            switch (type) {
                case Type::SIMILARITY:
                    return similaritySelector;
                case Type::POSITION:
                    return positionSelector(basis, method);
                case Type::RANDOM:
                    return randomSelector;
                case Type::ORDERED_RANDOM:
                    return orderedRandomSelector(basis, method, top);
            }
            throw std::logic_error("未知SelectorSpec类型");
        }
    };

    SelectorSpec parseSelector(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"type", "basis", "method", "top"}, path);
        const QString type = stringValue(object, "type", path, {}, true);
        SelectorSpec result;
        if (type == "similarity") {
            result.type = SelectorSpec::Type::SIMILARITY;
            if (object.size() != 1) invalidJson(path, "similarity不接受额外参数");
            return result;
        }
        if (type == "random") {
            result.type = SelectorSpec::Type::RANDOM;
            if (object.size() != 1) invalidJson(path, "random不接受额外参数");
            return result;
        }
        if (type != "position" && type != "orderedRandom") invalidJson(path + ".type", "未知selector类型");
        result.basis = parseSelectorBasis(stringValue(object, "basis", path, {}, true), path + ".basis");
        result.method = parseSelectorMethod(stringValue(object, "method", path, {}, true), path + ".method");
        if (type == "position") {
            result.type = SelectorSpec::Type::POSITION;
            if (object.contains("top")) invalidJson(path + ".top", "position不接受top");
            return result;
        }
        result.type = SelectorSpec::Type::ORDERED_RANDOM;
        const int top = intValue(object, "top", path, 1);
        if (top <= 0) invalidJson(path + ".top", "必须大于0");
        result.top = static_cast<std::size_t>(top);
        return result;
    }

    TextMatchConfig parseTextMatchConfig(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(
            object,
            {
                "match",
                "caseSensitivity",
                "normalize",
                "threshold",
                "boxThreshold",
                "maxEditDistance",
                "candidates",
                "uniqueNearest",
            },
            path
        );
        TextMatchConfig result;
        result.match = parseTextMatch(stringValue(object, "match", path, "CONTAINS"), path + ".match");
        result.caseSensitivity = parseCaseSensitivity(
            stringValue(object, "caseSensitivity", path, "INSENSITIVE"),
            path + ".caseSensitivity"
        );
        result.normalize = boolValue(object, "normalize", path, true);
        result.threshold = floatValue(object, "threshold", path, 0);
        result.boxThreshold = floatValue(object, "boxThreshold", path, 0);
        result.maxEditDistance = intValue(object, "maxEditDistance", path, 1);
        if (result.maxEditDistance < 0) invalidJson(path + ".maxEditDistance", "不能为负数");
        if (object.contains("candidates")) {
            result.candidates = qStringList(arrayValue(object, "candidates", path, true), path + ".candidates");
        }
        result.uniqueNearest = boolValue(object, "uniqueNearest", path, true);
        return result;
    }

    ImageInitConfig parseImageInitConfig(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"threshold", "timeout", "wait", "mode", "region"}, path);
        ImageInitConfig result;
        result.threshold = floatValue(object, "threshold", path, 0.9f);
        result.timeout = floatValue(object, "timeout", path, 60);
        result.wait = floatValue(object, "wait", path, 0);
        result.mode = parseMode(stringValue(object, "mode", path, "GRAY"), path + ".mode");
        result.region = rectValue(object, "region", path);
        if (result.timeout < 0 || result.wait < 0) invalidJson(path, "timeout和wait不能为负数");
        return result;
    }

    TextInitConfig parseTextInitConfig(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"timeout", "wait", "mode", "region", "match", "resolvedRegion"}, path);
        TextInitConfig result;
        result.timeout = floatValue(object, "timeout", path, 60);
        result.wait = floatValue(object, "wait", path, 0);
        result.mode = parseMode(stringValue(object, "mode", path, "RGB"), path + ".mode");
        result.region = rectValue(object, "region", path);
        if (object.contains("match")) {
            result.match = parseTextMatchConfig(objectValue(object, "match", path, true), path + ".match");
        }
        if (const auto resolved = object.value("resolvedRegion"); !resolved.isUndefined() && !resolved.isNull()) {
            if (!resolved.isObject()) invalidJson(path + ".resolvedRegion", "必须是矩形对象或null");
            result.resolvedRegion = parseRect(resolved.toObject(), path + ".resolvedRegion");
        }
        if (result.timeout < 0 || result.wait < 0) invalidJson(path, "timeout和wait不能为负数");
        return result;
    }

    ImageUntilConfig parseImageUntilConfig(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(
            object,
            {"onPrevious", "mode", "threshold", "interval", "startWait", "finishWait", "timeout", "reverse", "region"},
            path
        );
        ImageUntilConfig result;
        result.onPrevious = parsePrevious(stringValue(object, "onPrevious", path, "NONE"), path + ".onPrevious");
        result.mode = parseMode(stringValue(object, "mode", path, "GRAY"), path + ".mode");
        result.threshold = floatValue(object, "threshold", path, 0.9f);
        result.interval = floatValue(object, "interval", path, 0.1f);
        result.startWait = floatValue(object, "startWait", path, 0);
        result.finishWait = floatValue(object, "finishWait", path, 0);
        result.timeout = floatValue(object, "timeout", path, -1);
        result.reverse = boolValue(object, "reverse", path, false);
        result.region = rectValue(object, "region", path);
        if (result.interval < 0 || result.startWait < 0 || result.finishWait < 0 || result.timeout < -1) {
            invalidJson(path, "interval/startWait/finishWait不能为负数，timeout只能为-1或非负数");
        }
        return result;
    }

    TextUntilConfig parseTextUntilConfig(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(
            object,
            {
                "onPrevious",
                "mode",
                "threshold",
                "interval",
                "startWait",
                "finishWait",
                "timeout",
                "reverse",
                "match",
                "caseSensitivity",
                "normalize",
                "boxThreshold",
                "maxEditDistance",
                "candidates",
                "uniqueNearest",
                "region",
                "cropToPrevious",
                "cropPadding",
            },
            path
        );
        TextUntilConfig result;
        result.onPrevious = parsePrevious(stringValue(object, "onPrevious", path, "NONE"), path + ".onPrevious");
        result.mode = parseMode(stringValue(object, "mode", path, "RGB"), path + ".mode");
        result.threshold = floatValue(object, "threshold", path, 0);
        result.interval = floatValue(object, "interval", path, 0.1f);
        result.startWait = floatValue(object, "startWait", path, 0);
        result.finishWait = floatValue(object, "finishWait", path, 0);
        result.timeout = floatValue(object, "timeout", path, -1);
        result.reverse = boolValue(object, "reverse", path, false);
        result.match = parseTextMatch(stringValue(object, "match", path, "CONTAINS"), path + ".match");
        result.caseSensitivity = parseCaseSensitivity(
            stringValue(object, "caseSensitivity", path, "INSENSITIVE"),
            path + ".caseSensitivity"
        );
        result.normalize = boolValue(object, "normalize", path, true);
        result.boxThreshold = floatValue(object, "boxThreshold", path, 0);
        result.maxEditDistance = intValue(object, "maxEditDistance", path, 1);
        if (result.maxEditDistance < 0) invalidJson(path + ".maxEditDistance", "不能为负数");
        if (object.contains("candidates")) {
            result.candidates = qStringList(arrayValue(object, "candidates", path, true), path + ".candidates");
        }
        result.uniqueNearest = boolValue(object, "uniqueNearest", path, true);
        result.region = rectValue(object, "region", path);
        result.cropToPrevious = boolValue(object, "cropToPrevious", path, true);
        result.cropPadding = marginsValue(object, "cropPadding", path);
        if (result.interval < 0 || result.startWait < 0 || result.finishWait < 0 || result.timeout < -1) {
            invalidJson(path, "interval/startWait/finishWait不能为负数，timeout只能为-1或非负数");
        }
        return result;
    }

    struct ClickerSpec {
        MatchKind kind = MatchKind::IMAGE;
        QString target;
        ImageInitConfig imageConfig;
        TextInitConfig textConfig;

        [[nodiscard]] std::unique_ptr<ClickerBase> create() const {
            if (kind == MatchKind::IMAGE) return std::make_unique<ImageClicker>(target, imageConfig);
            return std::make_unique<TextClicker>(target, textConfig);
        }
    };

    ClickerSpec parseClicker(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"kind", "target", "config"}, path);
        ClickerSpec result;
        result.kind = parseMatchKind(stringValue(object, "kind", path, {}, true), path + ".kind");
        result.target = stringValue(object, "target", path, {}, true);
        const auto config = objectValue(object, "config", path);
        if (result.kind == MatchKind::IMAGE) {
            result.imageConfig = parseImageInitConfig(config, path + ".config");
        } else {
            result.textConfig = parseTextInitConfig(config, path + ".config");
        }
        return result;
    }

    struct UntilSpec {
        enum class Type {
            IMAGE,
            ANY_IMAGE,
            IMAGE_STABLE,
            IF_IMAGE,
            IF_ANY_IMAGE,
            TEXT,
            ANY_TEXT,
            TEXT_STABLE,
            IF_TEXT,
            IF_ANY_TEXT,
        };

        Type type = Type::IMAGE;
        QString target;
        std::vector<QString> targets;
        ImageUntilConfig imageConfig;
        TextUntilConfig textConfig;

        [[nodiscard]] MatchKind kind() const {
            return type <= Type::IF_ANY_IMAGE ? MatchKind::IMAGE : MatchKind::TEXT;
        }

        [[nodiscard]] std::vector<QString> branchTargets() const {
            if (!targets.empty()) return targets;
            return {target};
        }

        [[nodiscard]] bool isReversed() const {
            return kind() == MatchKind::IMAGE ? imageConfig.reverse : textConfig.reverse;
        }

        [[nodiscard]] std::unique_ptr<Until> create() const {
            switch (type) {
                case Type::IMAGE:
                    return std::make_unique<Image>(target, imageConfig);
                case Type::ANY_IMAGE:
                    return std::make_unique<AnyImage>(targets, imageConfig);
                case Type::IMAGE_STABLE:
                    return std::make_unique<ImageStable>(target, imageConfig);
                case Type::IF_IMAGE:
                    return std::make_unique<IfImage>(target, imageConfig);
                case Type::IF_ANY_IMAGE:
                    return std::make_unique<IfAnyImage>(targets, imageConfig);
                case Type::TEXT:
                    return std::make_unique<Text>(target, textConfig);
                case Type::ANY_TEXT:
                    return std::make_unique<AnyText>(targets, textConfig);
                case Type::TEXT_STABLE:
                    return std::make_unique<TextStable>(target, textConfig);
                case Type::IF_TEXT:
                    return std::make_unique<IfText>(target, textConfig);
                case Type::IF_ANY_TEXT:
                    return std::make_unique<IfAnyText>(targets, textConfig);
            }
            throw std::logic_error("未知UntilSpec类型");
        }
    };

    UntilSpec::Type parseUntilType(
        const QString& value,
        const QString& path
    ) {
        if (value == "Image") return UntilSpec::Type::IMAGE;
        if (value == "AnyImage") return UntilSpec::Type::ANY_IMAGE;
        if (value == "ImageStable") return UntilSpec::Type::IMAGE_STABLE;
        if (value == "IfImage") return UntilSpec::Type::IF_IMAGE;
        if (value == "IfAnyImage") return UntilSpec::Type::IF_ANY_IMAGE;
        if (value == "Text") return UntilSpec::Type::TEXT;
        if (value == "AnyText") return UntilSpec::Type::ANY_TEXT;
        if (value == "TextStable") return UntilSpec::Type::TEXT_STABLE;
        if (value == "IfText") return UntilSpec::Type::IF_TEXT;
        if (value == "IfAnyText") return UntilSpec::Type::IF_ANY_TEXT;
        invalidJson(path, "未知Until类型");
    }

    UntilSpec parseUntil(
        const QJsonObject& object,
        const QString& path
    ) {
        rejectUnknownKeys(object, {"type", "target", "targets", "config"}, path);
        UntilSpec result;
        result.type = parseUntilType(stringValue(object, "type", path, {}, true), path + ".type");
        const bool any = result.type == UntilSpec::Type::ANY_IMAGE || result.type == UntilSpec::Type::IF_ANY_IMAGE ||
                         result.type == UntilSpec::Type::ANY_TEXT || result.type == UntilSpec::Type::IF_ANY_TEXT;
        if (any) {
            if (object.contains("target")) invalidJson(path + ".target", "Any条件必须使用targets");
            result.targets = stringList(arrayValue(object, "targets", path, true), path + ".targets");
        } else {
            if (object.contains("targets")) invalidJson(path + ".targets", "单目标条件必须使用target");
            result.target = stringValue(object, "target", path, {}, true);
        }
        const auto config = objectValue(object, "config", path);
        if (result.kind() == MatchKind::IMAGE) {
            result.imageConfig = parseImageUntilConfig(config, path + ".config");
        } else {
            result.textConfig = parseTextUntilConfig(config, path + ".config");
        }
        return result;
    }

    std::vector<UntilSpec> parseUntilList(
        const QJsonObject& object,
        const QString& key,
        const QString& path
    ) {
        const auto array = arrayValue(object, key, path);
        std::vector<UntilSpec> result;
        result.reserve(static_cast<std::size_t>(array.size()));
        for (int index = 0; index < array.size(); ++index) {
            const QString itemPath = QString("%1.%2[%3]").arg(path, key).arg(index);
            if (!array[index].isObject()) invalidJson(itemPath, "必须是Until对象");
            result.push_back(parseUntil(array[index].toObject(), itemPath));
        }
        return result;
    }

    struct RunSpec {
        float startWait = 0;
        SelectorSpec selector;
        std::vector<UntilSpec> startUntilList;
        std::vector<UntilSpec> runUntilList;
        std::vector<UntilSpec> finishUntilList;
        float finishWait = 0;
        bool homing = true;
    };

    RunSpec parseRunSpec(
        const QJsonObject& object,
        const QString& path,
        const MatchKind inputKind
    ) {
        rejectUnknownKeys(
            object,
            {"startWait", "selector", "startUntilList", "runUntilList", "finishUntilList", "finishWait", "homing"},
            path
        );
        RunSpec result;
        result.startWait = floatValue(object, "startWait", path, 0);
        result.finishWait = floatValue(object, "finishWait", path, 0);
        if (result.startWait < 0 || result.finishWait < 0) invalidJson(path, "等待时间不能为负数");
        result.homing = boolValue(object, "homing", path, true);
        if (object.contains("selector")) {
            if (inputKind == MatchKind::TEXT) invalidJson(path + ".selector", "TextRunConfig不包含selector");
            result.selector = parseSelector(objectValue(object, "selector", path, true), path + ".selector");
        }
        result.startUntilList = parseUntilList(object, "startUntilList", path);
        result.runUntilList = parseUntilList(object, "runUntilList", path);
        result.finishUntilList = parseUntilList(object, "finishUntilList", path);
        return result;
    }

    enum class Action {
        LOCATE,
        CLICK,
        DRAG,
        SCROLL,
        BRANCH,
    };

    struct BranchSpec;

    struct StepSpec {
        Action action = Action::CLICK;
        MatchKind inputKind = MatchKind::IMAGE;
        RunSpec run;
        float interval = 1;
        int offsetX = 0;
        int offsetY = 0;
        Click position = Click::CENTER;
        int step = 10;
        bool reverse = false;
        int delta = -WheelDelta;
        std::shared_ptr<const BranchSpec> branch;
    };

    struct BranchSpec {
        UntilSpec condition;
        std::map<QString, std::vector<StepSpec>> branches;
    };

    struct ParsedStep {
        StepSpec step;
        MatchKind outputKind = MatchKind::IMAGE;
    };

    ParsedStep parseStep(
        const QJsonObject& object,
        const QString& path,
        const MatchKind inputKind
    ) {
        const QString action = stringValue(object, "action", path, {}, true);
        QSet<QString> allowed{"action", "runConfig"};
        StepSpec result;
        result.inputKind = inputKind;
        if (action == "locate") {
            result.action = Action::LOCATE;
        } else if (action == "click") {
            result.action = Action::CLICK;
            allowed.unite({"interval", "offsetX", "offsetY", "position"});
        } else if (action == "drag") {
            result.action = Action::DRAG;
            allowed.unite({"step", "reverse"});
        } else if (action == "scroll") {
            result.action = Action::SCROLL;
            allowed.unite({"delta", "interval", "offsetX", "offsetY", "position"});
        } else if (action == "branch") {
            result.action = Action::BRANCH;
            allowed = {"action", "condition", "branches"};
        } else {
            invalidJson(path + ".action", "必须是locate/click/drag/scroll/branch");
        }
        rejectUnknownKeys(object, allowed, path);

        if (result.action == Action::BRANCH) {
            auto branch = std::make_shared<BranchSpec>();
            branch->condition = parseUntil(objectValue(object, "condition", path, true), path + ".condition");
            if (branch->condition.isReversed()) {
                invalidJson(
                    path + ".condition.config.reverse",
                    "branch不支持reverse=true，因为目标未命中时没有可分派的target"
                );
            }
            const QJsonObject branchObject = objectValue(object, "branches", path, true);
            const std::vector<QString> expectedTargets = branch->condition.branchTargets();
            QSet<QString> expectedTargetSet;
            for (const QString& target : expectedTargets)
                expectedTargetSet.insert(target);

            for (auto iterator = branchObject.constBegin(); iterator != branchObject.constEnd(); ++iterator) {
                const QString branchPath = path + ".branches." + iterator.key();
                if (!expectedTargetSet.contains(iterator.key())) invalidJson(branchPath, "不是condition的候选target");
                if (!iterator.value().isArray()) invalidJson(branchPath, "必须是步骤数组");

                const QJsonArray steps = iterator.value().toArray();
                std::vector<StepSpec> parsedSteps;
                parsedSteps.reserve(static_cast<std::size_t>(steps.size()));
                MatchKind branchKind = branch->condition.kind();
                for (int index = 0; index < steps.size(); ++index) {
                    const QString stepPath = QString("%1[%2]").arg(branchPath).arg(index);
                    if (!steps[index].isObject()) invalidJson(stepPath, "必须是步骤对象");
                    auto parsed = parseStep(steps[index].toObject(), stepPath, branchKind);
                    branchKind = parsed.outputKind;
                    parsedSteps.push_back(std::move(parsed.step));
                }
                if (branchKind != inputKind) {
                    invalidJson(branchPath, "分支结束时必须回到branch之前的MatchKind");
                }
                branch->branches.emplace(iterator.key(), std::move(parsedSteps));
            }

            for (const QString& target : expectedTargetSet) {
                if (!branchObject.contains(target)) {
                    invalidJson(path + ".branches." + target, "缺少condition候选target的处理分支");
                }
            }

            result.branch = std::move(branch);
            return {std::move(result), inputKind};
        }

        result.run = parseRunSpec(objectValue(object, "runConfig", path), path + ".runConfig", inputKind);
        if (result.action == Action::LOCATE && !result.run.runUntilList.empty()) {
            invalidJson(path + ".runConfig.runUntilList", "locate不支持runUntilList，请使用finishUntilList");
        }
        if (result.action == Action::CLICK || result.action == Action::SCROLL) {
            result.interval = floatValue(object, "interval", path, 1);
            if (result.interval < 0) invalidJson(path + ".interval", "不能为负数");
            result.offsetX = intValue(object, "offsetX", path, 0);
            result.offsetY = intValue(object, "offsetY", path, 0);
            result.position = parseClick(stringValue(object, "position", path, "CENTER"), path + ".position");
        }
        if (result.action == Action::DRAG) {
            result.step = intValue(object, "step", path, 10);
            if (result.step <= 0) invalidJson(path + ".step", "必须大于0");
            result.reverse = boolValue(object, "reverse", path, false);
        }
        if (result.action == Action::SCROLL) {
            result.delta = intValue(object, "delta", path, -WheelDelta);
            if (result.delta < SHRT_MIN || result.delta > SHRT_MAX) {
                invalidJson(path + ".delta", "必须在有符号SHORT范围内");
            }
        }

        MatchKind outputKind = inputKind;
        if (!result.run.finishUntilList.empty()) {
            outputKind = result.run.finishUntilList.back().kind();
        } else if (!result.run.runUntilList.empty()) {
            outputKind = result.run.runUntilList.back().kind();
        }
        return {std::move(result), outputKind};
    }

    struct OwnedConditions {
        std::vector<std::unique_ptr<Until>> start;
        std::vector<std::unique_ptr<Until>> run;
        std::vector<std::unique_ptr<Until>> finish;

        explicit OwnedConditions(
            const RunSpec& spec
        ) {
            const auto create = [](const std::vector<UntilSpec>& specs) {
                std::vector<std::unique_ptr<Until>> result;
                result.reserve(specs.size());
                for (const auto& condition : specs)
                    result.push_back(condition.create());
                return result;
            };
            start = create(spec.startUntilList);
            run = create(spec.runUntilList);
            finish = create(spec.finishUntilList);
        }

        [[nodiscard]] static std::vector<Until*> raw(
            const std::vector<std::unique_ptr<Until>>& values
        ) {
            std::vector<Until*> result;
            result.reserve(values.size());
            for (const auto& value : values)
                result.push_back(value.get());
            return result;
        }

        void release() {
            for (auto& value : start)
                (void)value.release();
            for (auto& value : run)
                (void)value.release();
            for (auto& value : finish)
                (void)value.release();
        }
    };

    template <typename ClickerType, typename RunConfig>
    std::unique_ptr<ClickerBase> executeTyped(
        std::unique_ptr<ClickerBase> current,
        const StepSpec& step,
        RunConfig config
    ) {
        auto* clicker = static_cast<ClickerType*>(current.get());
        std::unique_ptr<ClickerBase> next;
        switch (step.action) {
            case Action::LOCATE:
                next = clicker->locate(std::move(config));
                break;
            case Action::CLICK:
                next = clicker->click(std::move(config), step.interval, step.offsetX, step.offsetY, step.position);
                break;
            case Action::DRAG:
                next = clicker->drag(std::move(config), step.step, step.reverse);
                break;
            case Action::SCROLL:
                next = clicker->scroll(
                    std::move(config),
                    step.delta,
                    step.interval,
                    step.offsetX,
                    step.offsetY,
                    step.position
                );
                break;
            case Action::BRANCH:
                throw std::logic_error("branch不能作为普通Clicker动作执行");
        }
        if (!next) throw std::runtime_error("工作流动作未返回Clicker");
        return next;
    }

    std::unique_ptr<ClickerBase> executeStep(std::unique_ptr<ClickerBase> current, const StepSpec& step);

    std::unique_ptr<ClickerBase> executeBranch(
        std::unique_ptr<ClickerBase> current,
        const BranchSpec& spec
    ) {
        BranchMap branches;
        for (const auto& entry : spec.branches) {
            const auto* steps = &entry.second;
            branches.emplace(entry.first, [steps](std::unique_ptr<ClickerBase> branchCurrent) {
                for (const auto& step : *steps) {
                    if (stopped(env.stopFlag)) break;
                    branchCurrent = executeStep(std::move(branchCurrent), step);
                }
                return branchCurrent;
            });
        }
        return current->branch(spec.condition.create(), branches);
    }

    std::unique_ptr<ClickerBase> executeStep(
        std::unique_ptr<ClickerBase> current,
        const StepSpec& step
    ) {
        if (!current) throw std::logic_error("工作流当前Clicker为空");
        if (current->kind != step.inputKind) throw std::logic_error("工作流运行时MatchKind与解析结果不一致");
        if (step.action == Action::BRANCH) {
            if (!step.branch) throw std::logic_error("branch步骤缺少配置");
            return executeBranch(std::move(current), *step.branch);
        }
        OwnedConditions conditions(step.run);
        const auto start = OwnedConditions::raw(conditions.start);
        const auto run = OwnedConditions::raw(conditions.run);
        const auto finish = OwnedConditions::raw(conditions.finish);

        if (step.inputKind == MatchKind::IMAGE) {
            ImageRunConfig config{
                .startWait = step.run.startWait,
                .selector = step.run.selector.create(),
                .startUntilList = start,
                .runUntilList = run,
                .finishUntilList = finish,
                .finishWait = step.run.finishWait,
                .homing = step.run.homing,
            };
            conditions.release();
            return executeTyped<ImageClicker>(std::move(current), step, std::move(config));
        }

        TextRunConfig config{
            .startWait = step.run.startWait,
            .startUntilList = start,
            .runUntilList = run,
            .finishUntilList = finish,
            .finishWait = step.run.finishWait,
            .homing = step.run.homing,
        };
        conditions.release();
        return executeTyped<TextClicker>(std::move(current), step, std::move(config));
    }
} // namespace

class Workflow::Impl {
public:
    QString name;
    ClickerSpec clicker;
    std::vector<StepSpec> steps;
};

Workflow::Workflow(
    std::shared_ptr<const Impl> impl
) : impl(std::move(impl)) {
    if (!this->impl) throw std::invalid_argument("Workflow实现不能为空");
}

const QString& Workflow::name() const {
    return impl->name;
}

std::size_t Workflow::stepCount() const {
    return impl->steps.size();
}

std::unique_ptr<ClickerBase> Workflow::run() const {
    auto current = impl->clicker.create();
    for (const auto& step : impl->steps) {
        if (stopped(env.stopFlag)) break;
        current = executeStep(std::move(current), step);
    }
    return current;
}

Workflow parseWorkflow(
    const QJsonObject& object
) {
    rejectUnknownKeys(object, {"version", "name", "clicker", "steps"}, "workflow");
    const int version = intValue(object, "version", "workflow", 0);
    if (version != 1) invalidJson("workflow.version", "当前仅支持版本1");

    auto impl = std::make_shared<Workflow::Impl>();
    impl->name = stringValue(object, "name", "workflow", {}, true);
    impl->clicker = parseClicker(objectValue(object, "clicker", "workflow", true), "workflow.clicker");

    const auto steps = arrayValue(object, "steps", "workflow", true);
    impl->steps.reserve(static_cast<std::size_t>(steps.size()));
    MatchKind currentKind = impl->clicker.kind;
    for (int index = 0; index < steps.size(); ++index) {
        const QString path = QString("workflow.steps[%1]").arg(index);
        if (!steps[index].isObject()) invalidJson(path, "必须是步骤对象");
        auto parsed = parseStep(steps[index].toObject(), path, currentKind);
        currentKind = parsed.outputKind;
        impl->steps.push_back(std::move(parsed.step));
    }
    return Workflow(std::move(impl));
}

Workflow parseWorkflow(
    const QByteArray& json
) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        throw std::invalid_argument(
            QString("JSON解析失败(offset %1): %2").arg(error.offset).arg(error.errorString()).toUtf8().toStdString()
        );
    }
    if (!document.isObject()) throw std::invalid_argument("工作流JSON根节点必须是对象");
    return parseWorkflow(document.object());
}

Workflow parseWorkflowFile(
    const QString& path
) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(QString("无法读取工作流文件: %1").arg(path).toUtf8().toStdString());
    }
    return parseWorkflow(file.readAll());
}
