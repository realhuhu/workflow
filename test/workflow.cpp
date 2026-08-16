#include "workflow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

    template <typename Config>
    concept HasSelector = requires(Config config) { config.selector; };

    template <typename ClickerType, typename Config>
    concept AcceptsRunConfig = requires(ClickerType& clicker, Config config) {
        clicker.locate(config);
        clicker.click(config);
        clicker.drag(config);
        clicker.scroll(config);
    };

    template <typename ClickerType>
    concept AcceptsUnconfiguredActions = requires(ClickerType& clicker) {
        clicker.locate();
        clicker.click(0.2f, 1, 2, Click::CENTER);
        clicker.drag(10, false);
        clicker.scroll(-WheelDelta, 0.2f, 1, 2, Click::CENTER);
    };

    static_assert(std::is_abstract_v<ClickerBase>);
    static_assert(HasSelector<ImageRunConfig>);
    static_assert(!HasSelector<TextRunConfig>);
    static_assert(AcceptsRunConfig<ImageClicker, ImageRunConfig>);
    static_assert(!AcceptsRunConfig<ImageClicker, TextRunConfig>);
    static_assert(AcceptsRunConfig<TextClicker, TextRunConfig>);
    static_assert(!AcceptsRunConfig<TextClicker, ImageRunConfig>);
    static_assert(AcceptsRunConfig<ClickerBase, ImageRunConfig>);
    static_assert(AcceptsRunConfig<ClickerBase, TextRunConfig>);
    static_assert(AcceptsUnconfiguredActions<ImageClicker>);
    static_assert(AcceptsUnconfiguredActions<TextClicker>);
    static_assert(AcceptsUnconfiguredActions<ClickerBase>);
    static_assert(std::same_as<decltype(std::declval<ImageClicker&>().click()), std::unique_ptr<ClickerBase>>);
    static_assert(std::same_as<decltype(std::declval<TextClicker&>().click()), std::unique_ptr<ClickerBase>>);

    class Failure final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    std::string printable(
        const QString& value
    ) {
        return value.toUtf8().toStdString();
    }

    void fail(
        const char* file,
        int line,
        const std::string& message
    ) {
        throw Failure(std::string(file) + ":" + std::to_string(line) + ": " + message);
    }

#define EXPECT_TRUE(expression)                                                                                        \
    do {                                                                                                               \
        if (!(expression)) fail(__FILE__, __LINE__, "expected true: " #expression);                                    \
    } while (false)

#define EXPECT_FALSE(expression)                                                                                       \
    do {                                                                                                               \
        if ((expression)) fail(__FILE__, __LINE__, "expected false: " #expression);                                    \
    } while (false)

#define EXPECT_EQ(actual, expected)                                                                                    \
    do {                                                                                                               \
        const auto actualValue = (actual);                                                                             \
        const auto expectedValue = (expected);                                                                         \
        if (!(actualValue == expectedValue)) {                                                                         \
            fail(__FILE__, __LINE__, "values differ: " #actual " != " #expected);                                      \
        }                                                                                                              \
    } while (false)

    template <typename Exception, typename Function>
    void expectThrows(
        Function&& function,
        const char* file,
        int line
    ) {
        try {
            std::forward<Function>(function)();
        } catch (const Exception&) {
            return;
        } catch (const std::exception& error) {
            fail(file, line, std::string("wrong exception type: ") + error.what());
        }
        fail(file, line, "expected exception was not thrown");
    }

#define EXPECT_THROWS(exceptionType, expression)                                                                       \
    expectThrows<exceptionType>([&] { (void)(expression); }, __FILE__, __LINE__)

    void expectRect(
        const QRect& actual,
        const QRect& expected,
        const char* file,
        int line
    ) {
        if (actual == expected) return;
        fail(
            file,
            line,
            "rect differs: actual=(" + std::to_string(actual.x()) + "," + std::to_string(actual.y()) + "," +
                std::to_string(actual.width()) + "," + std::to_string(actual.height()) + ") expected=(" +
                std::to_string(expected.x()) + "," + std::to_string(expected.y()) + "," +
                std::to_string(expected.width()) + "," + std::to_string(expected.height()) + ")"
        );
    }

#define EXPECT_RECT(actual, expected) expectRect((actual), (expected), __FILE__, __LINE__)

    OcrToken token(
        QString text,
        const QRect& box,
        float confidence = 0.95f,
        float boxConfidence = 0.90f
    ) {
        OcrToken result;
        result.text = std::move(text);
        result.box = box;
        result.center = QPoint(box.x() + box.width() / 2, box.y() + box.height() / 2);
        result.confidence = confidence;
        result.boxConfidence = boxConfidence;
        return result;
    }

    OcrRunResult successfulResult(
        std::initializer_list<OcrToken> tokens
    ) {
        OcrRunResult result;
        result.ok = true;
        result.tokens = QVector<OcrToken>(tokens.begin(), tokens.end());
        return result;
    }

    class WheelProbeWindow final {
    public:
        struct Message {
            UINT id;
            WPARAM wParam;
            LPARAM lParam;
        };

        WheelProbeWindow() :
            instance(GetModuleHandleW(nullptr)), className(
                                                     L"WorkflowWheelProbe-" + std::to_wstring(GetCurrentProcessId()) +
                                                     L"-" + std::to_wstring(GetCurrentThreadId())
                                                 ) {
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = &WheelProbeWindow::windowProcedure;
            windowClass.hInstance = instance;
            windowClass.lpszClassName = className.c_str();
            atom = RegisterClassW(&windowClass);
            if (!atom) throw std::runtime_error("RegisterClassW failed");

            window = CreateWindowExW(
                0,
                className.c_str(),
                L"",
                WS_POPUP,
                100,
                120,
                200,
                160,
                nullptr,
                nullptr,
                instance,
                this
            );
            if (!window) {
                UnregisterClassW(className.c_str(), instance);
                atom = 0;
                throw std::runtime_error("CreateWindowExW failed");
            }
        }

        ~WheelProbeWindow() {
            if (window) DestroyWindow(window);
            if (atom) UnregisterClassW(className.c_str(), instance);
        }

        WheelProbeWindow(const WheelProbeWindow&) = delete;
        WheelProbeWindow& operator=(const WheelProbeWindow&) = delete;

        [[nodiscard]] HWND hwnd() const {
            return window;
        }
        [[nodiscard]] const std::vector<Message>& messages() const {
            return messagesSeen;
        }
        void clear() {
            messagesSeen.clear();
        }

        bool pumpUntil(
            size_t expectedCount,
            int timeoutMs = 250
        ) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (messagesSeen.size() < expectedCount && std::chrono::steady_clock::now() < deadline) {
                MSG message{};
                if (PeekMessageW(&message, window, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            return messagesSeen.size() >= expectedCount;
        }

    private:
        static LRESULT CALLBACK windowProcedure(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam
        ) {
            if (message == WM_NCCREATE) {
                const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
            }
            auto* self = reinterpret_cast<WheelProbeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (self && (message == WM_MOUSEMOVE || message == WM_MOUSEWHEEL)) {
                self->messagesSeen.push_back({message, wParam, lParam});
            }
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        HINSTANCE instance = nullptr;
        std::wstring className;
        ATOM atom = 0;
        HWND window = nullptr;
        std::vector<Message> messagesSeen;
    };

    class FakeOcrProvider final : public OcrProvider {
    public:
        using Handler = std::function<OcrRunResult(const cv::Mat&, int)>;

        int calls = 0;
        std::vector<cv::Mat> inputs;
        Handler handler;

        OcrRunResult recognize(
            const cv::Mat& image
        ) override {
            ++calls;
            inputs.push_back(image.clone());
            if (handler) return handler(image, calls);
            OcrRunResult result;
            result.ok = true;
            return result;
        }
    };

    class FakePlatform final : public Platform {
    public:
        struct PointerEvent {
            QString type;
            QPoint from;
            QPoint to;
            int delta = 0;
        };

        cv::Mat screen;
        int captures = 0;
        std::vector<PointerEvent> events;

        explicit FakePlatform(
            cv::Mat value = {}
        ) : screen(std::move(value)) {
        }

        cv::Mat getScreen(
            HWND,
            Mode mode
        ) override {
            ++captures;
            if (screen.empty()) return {};
            if (mode == Mode::RGB && screen.channels() == 1) {
                cv::Mat converted;
                cv::cvtColor(screen, converted, cv::COLOR_GRAY2BGR);
                return converted;
            }
            if (mode == Mode::GRAY && screen.channels() == 3) {
                cv::Mat converted;
                cv::cvtColor(screen, converted, cv::COLOR_BGR2GRAY);
                return converted;
            }
            return screen.clone();
        }

        void moveTo(
            HWND,
            int x,
            int y
        ) override {
            events.push_back({"move", QPoint(x, y), QPoint(x, y)});
        }

        void leftDown(
            HWND,
            int x,
            int y
        ) override {
            events.push_back({"down", QPoint(x, y), QPoint(x, y)});
        }

        void leftUp(
            HWND,
            int x,
            int y
        ) override {
            events.push_back({"up", QPoint(x, y), QPoint(x, y)});
        }

        void wheel(
            HWND,
            int x,
            int y,
            int delta
        ) override {
            events.push_back({"wheel", QPoint(x, y), QPoint(x, y), delta});
        }

        void drag(
            HWND,
            int xStart,
            int yStart,
            int xEnd,
            int yEnd
        ) override {
            events.push_back({"drag", QPoint(xStart, yStart), QPoint(xEnd, yEnd)});
        }

        std::vector<QPoint> clickPoints() const {
            std::vector<QPoint> result;
            for (const auto& event : events) {
                if (event.type == "up") result.push_back(event.to);
            }
            return result;
        }

        std::vector<PointerEvent> wheelEvents() const {
            std::vector<PointerEvent> result;
            for (const auto& event : events) {
                if (event.type == "wheel") result.push_back(event);
            }
            return result;
        }
    };

    class EnvScope final {
    public:
        EnvScope(
            Platform* platform,
            OcrProvider* ocr
        ) : saved(env) {
            env = {};
            env.hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(1));
            env.emitter = &emitter;
            env.stopFlag = &stopFlag;
            env.platform = platform;
            env.ocr = ocr;
        }

        ~EnvScope() {
            env = saved;
        }

        void requestStop() {
            stopFlag.store(true);
        }

        EnvScope(const EnvScope&) = delete;
        EnvScope& operator=(const EnvScope&) = delete;

    private:
        Env saved;
        Emitter emitter;
        std::atomic<bool> stopFlag{false};
    };

    class TestUntil : public ImageUntil {
    protected:
        explicit TestUntil(
            const ImageUntilConfig& config = {}
        ) : ImageUntil(QStringLiteral("test"), config) {
        }

        std::vector<Segment> scan(
            std::unique_ptr<Segment>&
        ) override {
            return {};
        }
    };

    class FilterUntil final : public TestUntil {
    public:
        explicit FilterUntil(
            const ImageUntilConfig& config
        ) : TestUntil(config) {
        }
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            return false;
        }
        QString toString() const override {
            return "filter-test";
        }
    };

    class CountingUntil final : public TestUntil {
    public:
        explicit CountingUntil(
            int* destructions
        ) : destructionCount(destructions) {
        }
        ~CountingUntil() override {
            ++*destructionCount;
        }
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            return true;
        }
        QString toString() const override {
            return "counting-test";
        }

    private:
        int* destructionCount;
    };

    class StopUntil final : public TestUntil {
    public:
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            if (env.stopFlag) env.stopFlag->store(true);
            return false;
        }
        QString toString() const override {
            return "stop-test";
        }
    };

    class StopTrueUntil final : public TestUntil {
    public:
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            if (env.stopFlag) env.stopFlag->store(true);
            return true;
        }
        QString toString() const override {
            return "stop-true-test";
        }
    };

    class ProbeUntil final : public TestUntil {
    public:
        explicit ProbeUntil(
            bool* called
        ) : wasCalled(called) {
        }
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            *wasCalled = true;
            return true;
        }
        QString toString() const override {
            return "probe-test";
        }

    private:
        bool* wasCalled;
    };

    class SequenceUntil final : public TestUntil {
    public:
        explicit SequenceUntil(
            int* checks
        ) : checkCount(checks) {
        }
        bool flag(
            std::unique_ptr<Segment>&
        ) override {
            ++*checkCount;
            return *checkCount >= 2;
        }
        QString toString() const override {
            return "sequence-test";
        }

    private:
        int* checkCount;
    };

    void testWin32MouseWheelMessageContractAndBoundaries() {
        WheelProbeWindow window;

        POINT expectedScreen{7, 9};
        EXPECT_TRUE(ClientToScreen(window.hwnd(), &expectedScreen));
        Mouse::wheel(window.hwnd(), 7, 9, SHRT_MAX);
        EXPECT_TRUE(window.pumpUntil(2));
        EXPECT_EQ(window.messages().size(), size_t(2));
        EXPECT_EQ(window.messages()[0].id, static_cast<UINT>(WM_MOUSEMOVE));
        EXPECT_EQ(GET_X_LPARAM(window.messages()[0].lParam), 7);
        EXPECT_EQ(GET_Y_LPARAM(window.messages()[0].lParam), 9);
        EXPECT_EQ(window.messages()[1].id, static_cast<UINT>(WM_MOUSEWHEEL));
        EXPECT_EQ(GET_WHEEL_DELTA_WPARAM(window.messages()[1].wParam), SHRT_MAX);
        EXPECT_EQ(GET_X_LPARAM(window.messages()[1].lParam), expectedScreen.x);
        EXPECT_EQ(GET_Y_LPARAM(window.messages()[1].lParam), expectedScreen.y);

        window.clear();
        Mouse::wheel(window.hwnd(), 7, 9, SHRT_MIN);
        EXPECT_TRUE(window.pumpUntil(2));
        EXPECT_EQ(GET_WHEEL_DELTA_WPARAM(window.messages()[1].wParam), SHRT_MIN);

        window.clear();
        RECT client{};
        EXPECT_TRUE(GetClientRect(window.hwnd(), &client));
        Mouse::wheel(window.hwnd(), client.right - 1, client.bottom - 1, WHEEL_DELTA);
        EXPECT_TRUE(window.pumpUntil(2));

        window.clear();
        EXPECT_THROWS(std::out_of_range, Mouse::wheel(window.hwnd(), 7, 9, SHRT_MAX + 1));
        EXPECT_THROWS(std::out_of_range, Mouse::wheel(window.hwnd(), 7, 9, SHRT_MIN - 1));
        EXPECT_THROWS(std::out_of_range, Mouse::wheel(window.hwnd(), -1, 9, WHEEL_DELTA));
        EXPECT_THROWS(std::out_of_range, Mouse::wheel(window.hwnd(), client.right, 9, WHEEL_DELTA));
        EXPECT_THROWS(std::out_of_range, Mouse::wheel(window.hwnd(), 7, client.bottom, WHEEL_DELTA));
        EXPECT_TRUE(window.messages().empty());
    }

    void testOcrCropsBeforeProviderAndMapsCoordinates() {
        cv::Mat raw(80, 100, CV_8UC3);
        for (int y = 0; y < raw.rows; ++y) {
            for (int x = 0; x < raw.cols; ++x) {
                raw.at<cv::Vec3b>(y, x) =
                    cv::Vec3b(static_cast<uchar>(x), static_cast<uchar>(y), static_cast<uchar>((x + y) % 255));
            }
        }

        FakePlatform platform;
        FakeOcrProvider provider;
        provider.handler = [&](const cv::Mat& image, int) {
            EXPECT_EQ(image.cols, 30);
            EXPECT_EQ(image.rows, 25);
            EXPECT_EQ(image.at<cv::Vec3b>(0, 0), raw.at<cv::Vec3b>(10, 20));
            return successfulResult({token("确认", QRect(3, 4, 10, 6))});
        };
        EnvScope scope(&platform, &provider);

        const QRect roi(20, 10, 30, 25);
        const OcrRunResult result = OCR::recognize(raw, roi);
        EXPECT_TRUE(result.ok);
        EXPECT_EQ(provider.calls, 1);
        EXPECT_RECT(result.region, roi);
        EXPECT_EQ(result.tokens.size(), 1);
        EXPECT_RECT(result.tokens.front().box, QRect(23, 14, 10, 6));
        EXPECT_EQ(result.tokens.front().center, QPoint(28, 17));
    }

    void testEmptyOcrIntersectionSkipsProvider() {
        cv::Mat raw(30, 40, CV_8UC3, cv::Scalar(1, 2, 3));
        FakePlatform platform;
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);

        const OcrRunResult result = OCR::recognize(raw, QRect(100, 100, 20, 20));
        EXPECT_EQ(provider.calls, 0);
        EXPECT_TRUE(result.tokens.isEmpty());
        EXPECT_TRUE(result.region.isEmpty());

        FakePlatform window(raw);
        env.platform = &window;
        TextUntilConfig config;
        config.match = TextMatch::EXACT;
        config.onPrevious = Previous::INNER;
        config.region = QRect(0, 0, 10, 10);
        Text condition("不会识别", config);
        auto previous = std::make_unique<Segment>(20, 15, 10, 10, 1.0f);
        EXPECT_FALSE(condition.flag(previous));
        EXPECT_EQ(window.captures, 1);
        EXPECT_EQ(provider.calls, 0);

        TextClicker emptyText(
            QStringLiteral("不会识别"),
            TextInitConfig{
                .mode = Mode::RGB,
                .region = QRect(100, 100, 10, 10),
            }
        );
        EXPECT_FALSE(emptyText.founded());
        EXPECT_EQ(emptyText.kind, MatchKind::TEXT);
        EXPECT_EQ(emptyText.target, QStringLiteral("不会识别"));
        EXPECT_EQ(provider.calls, 0);

        const int capturesBeforeClone = window.captures;
        scope.requestStop();
        auto clonedEmptyText = emptyText.click();
        EXPECT_TRUE(static_cast<bool>(clonedEmptyText));
        EXPECT_FALSE(clonedEmptyText->founded());
        EXPECT_EQ(clonedEmptyText->kind, MatchKind::TEXT);
        EXPECT_EQ(clonedEmptyText->target, emptyText.target);
        EXPECT_EQ(window.captures, capturesBeforeClone);
        EXPECT_EQ(provider.calls, 0);
    }

    void testResolveRegionAllPreviousRelations() {
        const cv::Mat raw(800, 1000, CV_8UC3, cv::Scalar(0, 0, 0));
        const Segment previous(400, 300, 200, 100, 1.0f);

        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::NONE, &previous), QRect(0, 0, 1000, 800));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::LEFT, &previous), QRect(0, 0, 400, 800));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::RIGHT, &previous), QRect(600, 0, 400, 800));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::TOP, &previous), QRect(0, 0, 1000, 300));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::DOWN, &previous), QRect(0, 400, 1000, 400));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::LEFT_CENTER, &previous), QRect(0, 300, 400, 100));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::RIGHT_CENTER, &previous), QRect(600, 300, 400, 100));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::TOP_CENTER, &previous), QRect(400, 0, 200, 300));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::DOWN_CENTER, &previous), QRect(400, 400, 200, 400));
        EXPECT_RECT(OCR::resolveRegion(raw, {}, Previous::INNER, &previous), QRect(400, 300, 200, 100));

        EXPECT_RECT(
            OCR::resolveRegion(raw, QRect(450, 320, 100, 50), Previous::INNER, &previous),
            QRect(450, 320, 100, 50)
        );
        EXPECT_RECT(
            OCR::resolveRegion(raw, {}, Previous::INNER, &previous, QMargins(10, 20, 30, 40)),
            QRect(390, 280, 240, 160)
        );
        EXPECT_TRUE(OCR::resolveRegion(raw, QRect(0, 0, 100, 100), Previous::INNER, &previous).isEmpty());
        EXPECT_THROWS(std::runtime_error, OCR::resolveRegion(raw, {}, Previous::INNER, nullptr));
    }

    void testFindAnyAndAnyTextUseOneInferenceAndCandidateOrder() {
        const cv::Mat raw(90, 120, CV_8UC3, cv::Scalar(3, 4, 5));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult(
                {
                    token("alpha", QRect(5, 6, 20, 10), 0.80f),
                    token("beta", QRect(40, 20, 20, 10), 0.95f),
                }
            );
        };
        EnvScope scope(&platform, &provider);

        TextMatchConfig match;
        match.match = TextMatch::EXACT;
        QString matched;
        const auto direct = TextMatcher::findAnyPositions(raw, {"beta", "alpha"}, match, {}, &matched);
        EXPECT_EQ(provider.calls, 1);
        EXPECT_EQ(matched, QString("beta"));
        EXPECT_EQ(direct.size(), size_t(1));
        EXPECT_EQ(direct.front().x, 40);
        EXPECT_EQ(direct.front().y, 20);

        provider.calls = 0;
        provider.inputs.clear();
        platform.captures = 0;
        AnyText condition({"beta", "alpha"}, TextUntilConfig{});
        std::unique_ptr<Segment> previous;
        EXPECT_TRUE(condition.flag(previous));
        EXPECT_EQ(platform.captures, 1);
        EXPECT_EQ(provider.calls, 1);
        EXPECT_EQ(condition.target, QString("beta"));
        EXPECT_EQ(condition.targetSegmentList.size(), size_t(1));

        provider.calls = 0;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult(
                {
                    token("alpha", QRect(5, 6, 20, 10)),
                    token("beta", QRect(45, 25, 10, 8)),
                }
            );
        };
        TextUntilConfig relativeConfig;
        relativeConfig.onPrevious = Previous::INNER;
        relativeConfig.match = TextMatch::EXACT;
        relativeConfig.cropToPrevious = false;
        AnyText relativeCondition({"alpha", "beta"}, relativeConfig);
        auto relativePrevious = std::make_unique<Segment>(40, 20, 30, 20, 1.0f);
        EXPECT_TRUE(relativeCondition.flag(relativePrevious));
        EXPECT_EQ(provider.calls, 1);
        EXPECT_EQ(relativeCondition.target, QString("beta"));
    }

    void testTextMatchModesAndUniqueFuzzyCandidate() {
        const QVector<OcrToken> tokens{
            token("请确认操作", QRect(0, 0, 40, 10), 0.70f),
            token("确 认", QRect(0, 20, 20, 10), 0.95f),
        };

        TextMatchConfig exact;
        exact.match = TextMatch::EXACT;
        exact.normalize = true;
        const auto exactMatches = TextMatcher::matchTokens(tokens, "确认", exact);
        EXPECT_EQ(exactMatches.size(), size_t(1));
        EXPECT_EQ(exactMatches.front().y, 20);

        TextMatchConfig contains;
        contains.match = TextMatch::CONTAINS;
        contains.normalize = true;
        const auto containsMatches = TextMatcher::matchTokens(tokens, "确认", contains);
        EXPECT_EQ(containsMatches.size(), size_t(2));

        TextMatchConfig fuzzy;
        fuzzy.match = TextMatch::FUZZY;
        fuzzy.maxEditDistance = 1;
        fuzzy.candidates = QStringList{QStringLiteral("关羽"), QStringLiteral("张飞")};
        fuzzy.uniqueNearest = true;
        const auto unique =
            TextMatcher::matchTokens(QVector<OcrToken>{token("关习", QRect(0, 0, 20, 10))}, "关羽", fuzzy);
        EXPECT_EQ(unique.size(), size_t(1));

        fuzzy.candidates = QStringList{QStringLiteral("刘备"), QStringLiteral("刘邦")};
        const auto ambiguous =
            TextMatcher::matchTokens(QVector<OcrToken>{token("刘各", QRect(0, 0, 20, 10))}, "刘备", fuzzy);
        EXPECT_TRUE(ambiguous.empty());
    }

    void testTextOnPreviousCropsBeforeOcrAndMapsBack() {
        const cv::Mat raw(800, 1000, CV_8UC3, cv::Scalar(9, 8, 7));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat& image, int) {
            EXPECT_EQ(image.cols, 400);
            EXPECT_EQ(image.rows, 100);
            return successfulResult({token("目标", QRect(10, 20, 30, 12))});
        };
        EnvScope scope(&platform, &provider);

        TextUntilConfig config;
        config.match = TextMatch::EXACT;
        config.onPrevious = Previous::LEFT_CENTER;
        config.cropToPrevious = true;
        config.threshold = 0;
        const Segment previous(400, 300, 200, 100, 1.0f);
        ImageClicker seed("cached", previous);
        auto next = seed.locate(
            ImageRunConfig{
                .finishUntilList = {new Text("目标", config)},
                .homing = false,
            }
        );

        EXPECT_TRUE(static_cast<bool>(next));
        EXPECT_EQ(provider.calls, 1);
        EXPECT_EQ(platform.captures, 1);
        EXPECT_EQ(next->kind, MatchKind::TEXT);
        EXPECT_EQ(next->target, QStringLiteral("目标"));
        EXPECT_EQ(next->targetSegmentList.size(), size_t(1));
        const Segment& hit = next->targetSegmentList.front();
        EXPECT_EQ(hit.x, 10);
        EXPECT_EQ(hit.y, 320);
        EXPECT_EQ(hit.centerX(), 25);
        EXPECT_EQ(hit.centerY(), 326);
    }

    void testInfrastructureFailuresDoNotFulfillReverseConditions() {
        FakePlatform emptyPlatform;
        FakeOcrProvider provider;
        EnvScope emptyScope(&emptyPlatform, &provider);
        TextUntilConfig reverseConfig;
        reverseConfig.reverse = true;
        Text reverseText("目标", reverseConfig);
        std::unique_ptr<Segment> previous;
        EXPECT_THROWS(std::runtime_error, reverseText.fulfilled(previous));

        const cv::Mat raw(40, 60, CV_8UC3, cv::Scalar(1, 2, 3));
        FakePlatform validPlatform(raw);
        env.platform = &validPlatform;
        env.ocr = nullptr;
        EXPECT_THROWS(std::runtime_error, reverseText.fulfilled(previous));
    }

    void testOptionalConditionsHonorPreexistingStopWithoutInference() {
        const cv::Mat raw(40, 60, CV_8UC3, cv::Scalar(1, 2, 3));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);
        scope.requestStop();

        ImageUntilConfig imageConfig;
        imageConfig.startWait = 1.0f;
        TextUntilConfig textConfig;
        textConfig.startWait = 1.0f;
        std::unique_ptr<Segment> previous;

        IfImage("unused-image.png", imageConfig).loop(previous, 60.0f);
        IfAnyImage({"unused-a.png", "unused-b.png"}, imageConfig).loop(previous, 60.0f);
        IfText("不会识别", textConfig).loop(previous, 60.0f);
        IfAnyText({"不会识别-a", "不会识别-b"}, textConfig).loop(previous, 60.0f);

        EXPECT_EQ(platform.captures, 0);
        EXPECT_EQ(provider.calls, 0);
    }

    void testConditionOwnershipAndStopAreSafe() {
        const cv::Mat raw(40, 60, CV_8UC3, cv::Scalar(1, 2, 3));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);
        const Segment target(10, 10, 10, 10, 1.0f);
        ImageClicker clicker("cached", target);

        int destructions = 0;
        auto* duplicate = new CountingUntil(&destructions);
        EXPECT_THROWS(
            std::invalid_argument,
            clicker.click(
                ImageRunConfig{
                    .startUntilList = {duplicate, duplicate, new CountingUntil(&destructions)},
                    .homing = false,
                }
            )
        );
        EXPECT_EQ(destructions, 2);
        EXPECT_TRUE(platform.events.empty());

        int nullListDestructions = 0;
        EXPECT_THROWS(
            std::invalid_argument,
            clicker.click(
                ImageRunConfig{
                    .startUntilList = {nullptr, new CountingUntil(&nullListDestructions)},
                    .finishUntilList = {new CountingUntil(&nullListDestructions)},
                    .homing = false,
                }
            )
        );
        EXPECT_EQ(nullListDestructions, 2);

        int rejectedLocateDestructions = 0;
        EXPECT_THROWS(
            std::invalid_argument,
            clicker.locate(
                ImageRunConfig{
                    .runUntilList = {new CountingUntil(&rejectedLocateDestructions)},
                    .homing = false,
                }
            )
        );
        EXPECT_EQ(rejectedLocateDestructions, 1);

        scope.requestStop();
        auto stoppedNext = clicker.click();
        EXPECT_TRUE(static_cast<bool>(stoppedNext));
        EXPECT_TRUE(platform.events.empty());
    }

    void testTextTargetHasNonNullTerminalChain() {
        cv::Mat raw(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);

        Segment initial(10, 15, 20, 10, 0.95f);
        TextClicker clicker(QStringLiteral("确认"), initial);
        EXPECT_EQ(clicker.kind, MatchKind::TEXT);
        EXPECT_EQ(clicker.target, QStringLiteral("确认"));
        EXPECT_EQ(platform.captures, 0);
        EXPECT_EQ(provider.calls, 0);
        auto terminal = clicker.click(TextRunConfig{.homing = false});
        EXPECT_TRUE(static_cast<bool>(terminal));
        EXPECT_EQ(terminal->kind, MatchKind::TEXT);
        EXPECT_EQ(terminal->target, clicker.target);
        EXPECT_EQ(platform.captures, 0);
        EXPECT_EQ(provider.calls, 0);
        terminal->end();
        const auto clicks = platform.clickPoints();
        EXPECT_EQ(clicks.size(), size_t(1));
        EXPECT_EQ(clicks.front(), QPoint(20, 20));
    }

    void testFinishTextPropagatesToNextClickableSegment() {
        cv::Mat raw(100, 120, CV_8UC3, cv::Scalar(0, 0, 0));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int call) {
            if (call == 1) {
                return successfulResult({token("开始", QRect(10, 10, 20, 10))});
            }
            if (call == 2) {
                return successfulResult({token("下一步", QRect(70, 40, 24, 12))});
            }
            throw Failure("unexpected OCR inference");
        };
        EnvScope scope(&platform, &provider);

        TextMatchConfig textMatch;
        textMatch.match = TextMatch::EXACT;
        textMatch.threshold = 0;
        TextClicker clicker(
            QStringLiteral("开始"),
            TextInitConfig{
                .timeout = 2,
                .match = textMatch,
            }
        );
        EXPECT_TRUE(clicker.founded());

        TextUntilConfig untilConfig;
        untilConfig.match = TextMatch::EXACT;
        untilConfig.threshold = 0;
        auto next = clicker.click(
            TextRunConfig{
                .finishUntilList = {new Text("下一步", untilConfig)},
                .homing = false,
            },
            0.0f
        );
        EXPECT_TRUE(static_cast<bool>(next));
        EXPECT_TRUE(next->founded());
        EXPECT_EQ(next->kind, MatchKind::TEXT);
        EXPECT_EQ(next->target, QStringLiteral("下一步"));
        EXPECT_EQ(next->targetSegmentList.size(), size_t(1));
        EXPECT_EQ(next->targetSegmentList.front().x, 70);
        EXPECT_EQ(next->targetSegmentList.front().y, 40);
        EXPECT_EQ(provider.calls, 2);

        auto terminal = next->click(TextRunConfig{.homing = false}, 0.0f);
        EXPECT_TRUE(static_cast<bool>(terminal));
        terminal->end();

        const auto clicks = platform.clickPoints();
        EXPECT_EQ(clicks.size(), size_t(2));
        EXPECT_EQ(clicks[0], QPoint(20, 15));
        EXPECT_EQ(clicks[1], QPoint(82, 46));
    }

    void testScrollOneShotUsesSelectorCoordinatesAndDelta() {
        const cv::Mat raw(100, 120, CV_8UC3, cv::Scalar(0, 0, 0));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);

        const std::vector<Segment> targets{
            Segment(5, 10, 10, 10, 0.9f),
            Segment(30, 40, 20, 10, 0.8f),
        };
        ImageClicker clicker("cached", targets);
        const auto startedAt = std::chrono::steady_clock::now();
        auto next = clicker.scroll(
            ImageRunConfig{
                .selector = positionSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX),
                .homing = false,
            },
            240,
            0.5f,
            3,
            -2,
            Click::RIGHT
        );
        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startedAt).count();

        EXPECT_TRUE(static_cast<bool>(next));
        EXPECT_TRUE(elapsed < 0.25f);
        EXPECT_TRUE(clicker.previousSegment != nullptr);
        EXPECT_TRUE(*clicker.previousSegment == targets.back());
        EXPECT_TRUE(next->previousSegment != nullptr);
        EXPECT_TRUE(*next->previousSegment == targets.back());
        EXPECT_EQ(next->targetSegmentList.size(), targets.size());
        EXPECT_EQ(platform.captures, 0);
        EXPECT_EQ(provider.calls, 0);

        auto wheels = platform.wheelEvents();
        EXPECT_EQ(wheels.size(), size_t(1));
        EXPECT_EQ(wheels.front().from, QPoint(52, 43));
        EXPECT_EQ(wheels.front().delta, 240);

        targets.back().scroll(-WHEEL_DELTA, 0.0f, -2, 4, Click::TOP);
        wheels = platform.wheelEvents();
        EXPECT_EQ(wheels.size(), size_t(2));
        EXPECT_EQ(wheels.back().from, QPoint(38, 44));
        EXPECT_EQ(wheels.back().delta, -WHEEL_DELTA);

        platform.events.clear();
        bool satisfiedChecked = false;
        auto alreadySatisfied = clicker.scroll(
            ImageRunConfig{
                .runUntilList = {new ProbeUntil(&satisfiedChecked)},
                .homing = false,
            },
            -WHEEL_DELTA,
            0.0f
        );
        EXPECT_TRUE(static_cast<bool>(alreadySatisfied));
        EXPECT_TRUE(satisfiedChecked);
        EXPECT_TRUE(platform.wheelEvents().empty());

        int checks = 0;
        auto afterSearch = clicker.scroll(
            ImageRunConfig{
                .runUntilList = {new SequenceUntil(&checks)},
                .homing = false,
            },
            -180,
            0.0f
        );
        EXPECT_TRUE(static_cast<bool>(afterSearch));
        EXPECT_EQ(checks, 2);
        wheels = platform.wheelEvents();
        EXPECT_EQ(wheels.size(), size_t(1));
        EXPECT_EQ(wheels.front().from, QPoint(10, 15));
        EXPECT_EQ(wheels.front().delta, -180);
    }

    void testScrollStopAndTimeoutAreSafe() {
        const cv::Mat raw(60, 80, CV_8UC3, cv::Scalar(0, 0, 0));
        const Segment target(20, 20, 10, 10, 1.0f);

        {
            FakePlatform platform(raw);
            FakeOcrProvider provider;
            EnvScope scope(&platform, &provider);
            ImageClicker clicker("cached", target);
            scope.requestStop();

            auto next = clicker.scroll(ImageRunConfig{.homing = false}, -WHEEL_DELTA, 0.0f);
            EXPECT_TRUE(static_cast<bool>(next));
            EXPECT_TRUE(platform.wheelEvents().empty());
        }

        {
            FakePlatform platform(raw);
            FakeOcrProvider provider;
            EnvScope scope(&platform, &provider);
            ImageClicker clicker("cached", target);
            bool finishCalled = false;

            auto next = clicker.scroll(
                ImageRunConfig{
                    .runUntilList = {new StopUntil()},
                    .finishUntilList = {new ProbeUntil(&finishCalled)},
                    .homing = false,
                },
                -WHEEL_DELTA,
                0.0f
            );
            EXPECT_TRUE(static_cast<bool>(next));
            EXPECT_TRUE(platform.wheelEvents().empty());
            EXPECT_FALSE(finishCalled);
        }

        {
            FakePlatform platform(raw);
            FakeOcrProvider provider;
            EnvScope scope(&platform, &provider);
            ImageClicker clicker("cached", target);
            bool laterRunCalled = false;
            bool finishCalled = false;

            auto next = clicker.scroll(
                ImageRunConfig{
                    .runUntilList = {new StopTrueUntil(), new ProbeUntil(&laterRunCalled)},
                    .finishUntilList = {new ProbeUntil(&finishCalled)},
                    .homing = false,
                },
                -WHEEL_DELTA,
                0.0f
            );
            EXPECT_TRUE(static_cast<bool>(next));
            EXPECT_TRUE(platform.wheelEvents().empty());
            EXPECT_FALSE(laterRunCalled);
            EXPECT_FALSE(finishCalled);
        }

        {
            FakePlatform platform(raw);
            FakeOcrProvider provider;
            EnvScope scope(&platform, &provider);
            ImageClicker clicker("cached", target, ImageInitConfig{.timeout = 0.005f});

            EXPECT_THROWS(
                std::runtime_error,
                clicker.scroll(
                    ImageRunConfig{
                        .runUntilList = {new FilterUntil(ImageUntilConfig{})},
                        .homing = false,
                    },
                    -WHEEL_DELTA,
                    0.02f
                )
            );
            EXPECT_EQ(platform.wheelEvents().size(), size_t(1));
        }
    }

    void testScrollFinishTextCreatesNextTextTarget() {
        const cv::Mat raw(100, 120, CV_8UC3, cv::Scalar(0, 0, 0));
        FakePlatform platform(raw);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult(
                {
                    token("运行", QRect(5, 5, 20, 10)),
                    token("下一步", QRect(70, 40, 24, 12)),
                }
            );
        };
        EnvScope scope(&platform, &provider);

        TextUntilConfig textConfig;
        textConfig.match = TextMatch::EXACT;
        textConfig.interval = 0;
        const Segment target(10, 10, 10, 10, 1.0f);
        ImageClicker clicker("cached", target);
        auto next = clicker.scroll(
            ImageRunConfig{
                .runUntilList = {new Text("运行", textConfig)},
                .finishUntilList = {new Text("下一步", textConfig)},
                .homing = false,
            },
            -WHEEL_DELTA,
            0.0f
        );

        EXPECT_TRUE(static_cast<bool>(next));
        EXPECT_EQ(next->kind, MatchKind::TEXT);
        EXPECT_TRUE(next->founded());
        EXPECT_EQ(next->target, QStringLiteral("下一步"));
        EXPECT_EQ(next->targetSegmentList.size(), size_t(1));
        EXPECT_EQ(next->targetSegmentList.front().centerX(), 82);
        EXPECT_EQ(next->targetSegmentList.front().centerY(), 46);
        EXPECT_EQ(provider.calls, 2);
        EXPECT_TRUE(platform.wheelEvents().empty());

        auto terminal = next->scroll(TextRunConfig{.homing = false}, 360, 0.0f, 1, -1, Click::DOWN);
        EXPECT_TRUE(static_cast<bool>(terminal));
        EXPECT_EQ(terminal->kind, MatchKind::TEXT);
        EXPECT_EQ(terminal->target, QStringLiteral("下一步"));
        EXPECT_EQ(provider.calls, 2);

        const auto wheels = platform.wheelEvents();
        EXPECT_EQ(wheels.size(), size_t(1));
        EXPECT_EQ(wheels[0].from, QPoint(83, 50));
        EXPECT_EQ(wheels[0].delta, 360);
    }

    QString writePng(
        const cv::Mat& image,
        const QString& path
    ) {
        std::vector<uchar> encoded;
        if (!cv::imencode(".png", image, encoded)) throw Failure("failed to encode PNG fixture");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw Failure("failed to create PNG fixture: " + printable(path));
        }
        const qint64 size = static_cast<qint64>(encoded.size());
        if (file.write(reinterpret_cast<const char*>(encoded.data()), size) != size) {
            throw Failure("failed to write PNG fixture");
        }
        return path;
    }

    void testBranchDispatchesIfImageAndContinuesMainChainOnMiss() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat targetTemplate(9, 11, CV_8UC1);
        cv::Mat missingTemplate(9, 11, CV_8UC1);
        cv::Mat screen(80, 100, CV_8UC1);
        cv::RNG targetRng(0x4102);
        cv::RNG missingRng(0x9813);
        cv::RNG screenRng(0x7125);
        targetRng.fill(targetTemplate, cv::RNG::UNIFORM, 0, 256);
        missingRng.fill(missingTemplate, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        targetTemplate.copyTo(screen(cv::Rect(42, 31, targetTemplate.cols, targetTemplate.rows)));

        const QString targetPath = writePng(targetTemplate, directory.filePath("target.png"));
        const QString missingPath = writePng(missingTemplate, directory.filePath("missing.png"));
        FakePlatform platform(screen);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);

        ImageUntilConfig config;
        config.threshold = 0.999f;
        config.interval = 0;
        config.timeout = 1;

        const Segment initial(5, 6, 12, 10, 1.0f);
        ImageClicker start(QStringLiteral("当前页面"), initial);
        start.previousSegment = std::make_unique<Segment>(1, 2, 3, 4, 0.8f);

        bool matchedHandlerCalled = false;
        auto matched = start.branch(
            std::make_unique<IfImage>(targetPath, config),
            BranchMap{
                {targetPath,
                    [&](std::unique_ptr<ClickerBase> current) {
                        matchedHandlerCalled = true;
                        EXPECT_EQ(current->kind, MatchKind::IMAGE);
                        EXPECT_EQ(current->target, targetPath);
                        EXPECT_EQ(current->targetSegmentList.size(), size_t(1));
                        EXPECT_EQ(current->targetSegmentList.front().x, 42);
                        EXPECT_EQ(current->targetSegmentList.front().y, 31);
                        return current->locate(ImageRunConfig{.homing = false});
                    }},
            }
        );

        EXPECT_TRUE(matchedHandlerCalled);
        EXPECT_EQ(matched->target, targetPath);
        EXPECT_TRUE(static_cast<bool>(matched->previousSegment));
        EXPECT_EQ(matched->previousSegment->x, 42);
        auto continued = matched->locate(ImageRunConfig{.homing = false});
        EXPECT_EQ(continued->target, targetPath);
        EXPECT_EQ(continued->kind, MatchKind::IMAGE);

        bool missingHandlerCalled = false;
        auto missed = start.branch(
            std::make_unique<IfImage>(missingPath, config),
            BranchMap{
                {missingPath,
                    [&](std::unique_ptr<ClickerBase> current) {
                        missingHandlerCalled = true;
                        return current;
                    }},
            }
        );

        EXPECT_FALSE(missingHandlerCalled);
        EXPECT_EQ(missed->target, QStringLiteral("当前页面"));
        EXPECT_EQ(missed->kind, MatchKind::IMAGE);
        EXPECT_EQ(missed->targetSegmentList, start.targetSegmentList);
        EXPECT_TRUE(static_cast<bool>(missed->previousSegment));
        EXPECT_EQ(missed->previousSegment->x, 1);
        EXPECT_EQ(missed->previousSegment->y, 2);
        EXPECT_EQ(platform.captures, 2);
        EXPECT_EQ(provider.calls, 0);
    }

    void testBranchDispatchesAnyAndIfAnyBySelectedTarget() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat targetTemplate(8, 10, CV_8UC1);
        cv::Mat missingTemplate(8, 10, CV_8UC1);
        cv::Mat screen(90, 120, CV_8UC1);
        cv::RNG targetRng(0x1537);
        cv::RNG missingRng(0x8642);
        cv::RNG screenRng(0x3901);
        targetRng.fill(targetTemplate, cv::RNG::UNIFORM, 0, 256);
        missingRng.fill(missingTemplate, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        targetTemplate.copyTo(screen(cv::Rect(61, 47, targetTemplate.cols, targetTemplate.rows)));

        const QString targetPath = writePng(targetTemplate, directory.filePath("target.png"));
        const QString missingPath = writePng(missingTemplate, directory.filePath("missing.png"));
        FakePlatform platform(screen);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) { return successfulResult({token("完成", QRect(25, 18, 20, 10))}); };
        EnvScope scope(&platform, &provider);

        ImageUntilConfig imageConfig;
        imageConfig.threshold = 0.999f;
        imageConfig.interval = 0;
        imageConfig.timeout = 1;
        TextUntilConfig textConfig;
        textConfig.match = TextMatch::EXACT;
        textConfig.interval = 0;
        textConfig.timeout = 1;

        const Segment initial(8, 9, 14, 12, 1.0f);
        ImageClicker start(QStringLiteral("当前页面"), initial);
        bool wrongImageHandlerCalled = false;
        bool selectedImageHandlerCalled = false;
        auto imageBranch = start.branch(
            std::make_unique<AnyImage>(std::vector<QString>{missingPath, targetPath}, imageConfig),
            BranchMap{
                {missingPath,
                    [&](std::unique_ptr<ClickerBase> current) {
                        wrongImageHandlerCalled = true;
                        return current;
                    }},
                {targetPath,
                    [&](std::unique_ptr<ClickerBase> current) {
                        selectedImageHandlerCalled = true;
                        EXPECT_EQ(current->target, targetPath);
                        EXPECT_EQ(current->kind, MatchKind::IMAGE);
                        return current;
                    }},
            }
        );

        EXPECT_FALSE(wrongImageHandlerCalled);
        EXPECT_TRUE(selectedImageHandlerCalled);
        EXPECT_EQ(imageBranch->target, targetPath);
        EXPECT_EQ(imageBranch->targetSegmentList.front().x, 61);
        EXPECT_EQ(imageBranch->targetSegmentList.front().y, 47);

        bool wrongTextHandlerCalled = false;
        bool selectedTextHandlerCalled = false;
        auto textBranch = imageBranch->branch(
            std::make_unique<IfAnyText>(
                std::vector<QString>{QStringLiteral("缺失"), QStringLiteral("完成")},
                textConfig
            ),
            BranchMap{
                {QStringLiteral("缺失"),
                    [&](std::unique_ptr<ClickerBase> current) {
                        wrongTextHandlerCalled = true;
                        return current;
                    }},
                {QStringLiteral("完成"),
                    [&](std::unique_ptr<ClickerBase> current) {
                        selectedTextHandlerCalled = true;
                        EXPECT_EQ(current->target, QStringLiteral("完成"));
                        EXPECT_EQ(current->kind, MatchKind::TEXT);
                        EXPECT_EQ(current->targetSegmentList.front().x, 25);
                        EXPECT_EQ(current->targetSegmentList.front().y, 18);
                        return current;
                    }},
            }
        );

        EXPECT_FALSE(wrongTextHandlerCalled);
        EXPECT_TRUE(selectedTextHandlerCalled);
        EXPECT_EQ(textBranch->target, QStringLiteral("完成"));
        EXPECT_EQ(textBranch->kind, MatchKind::TEXT);

        bool noneHandlerCalled = false;
        auto continued = textBranch->branch(
            std::make_unique<IfAnyText>(
                std::vector<QString>{QStringLiteral("不存在A"), QStringLiteral("不存在B")},
                textConfig
            ),
            BranchMap{
                {QStringLiteral("不存在A"),
                    [&](std::unique_ptr<ClickerBase> current) {
                        noneHandlerCalled = true;
                        return current;
                    }},
                {QStringLiteral("不存在B"),
                    [&](std::unique_ptr<ClickerBase> current) {
                        noneHandlerCalled = true;
                        return current;
                    }},
            }
        );

        EXPECT_FALSE(noneHandlerCalled);
        EXPECT_EQ(continued->target, QStringLiteral("完成"));
        EXPECT_EQ(continued->kind, MatchKind::TEXT);
        EXPECT_EQ(continued->targetSegmentList, textBranch->targetSegmentList);
        EXPECT_EQ(platform.captures, 3);
        EXPECT_EQ(provider.calls, 2);
    }

    void testBranchRejectsInvalidHandlersAndHonorsStop() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat targetTemplate(8, 10, CV_8UC1);
        cv::Mat screen(60, 80, CV_8UC1);
        cv::RNG targetRng(0x5127);
        cv::RNG screenRng(0x7843);
        targetRng.fill(targetTemplate, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        targetTemplate.copyTo(screen(cv::Rect(30, 20, targetTemplate.cols, targetTemplate.rows)));
        const QString targetPath = writePng(targetTemplate, directory.filePath("target.png"));

        FakePlatform platform(screen);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);
        ImageUntilConfig config;
        config.threshold = 0.999f;
        config.interval = 0;
        config.timeout = 1;
        ImageClicker start(QStringLiteral("当前页面"), Segment(4, 5, 12, 10, 1.0f));

        EXPECT_THROWS(std::invalid_argument, start.branch(nullptr, {}));
        EXPECT_THROWS(std::invalid_argument, start.branch(std::make_unique<IfImage>(targetPath, config), {}));

        const BranchMap nullResult{
            {targetPath, [](std::unique_ptr<ClickerBase>) { return std::unique_ptr<ClickerBase>{}; }},
        };
        EXPECT_THROWS(std::runtime_error, start.branch(std::make_unique<IfImage>(targetPath, config), nullResult));

        ImageUntilConfig reverseConfig = config;
        reverseConfig.reverse = true;
        const int capturesBeforeReverse = platform.captures;
        EXPECT_THROWS(
            std::invalid_argument,
            start.branch(
                std::make_unique<IfImage>(targetPath, reverseConfig),
                BranchMap{{targetPath, [](std::unique_ptr<ClickerBase> current) { return current; }}}
            )
        );
        EXPECT_EQ(platform.captures, capturesBeforeReverse);

        platform.screen.release();
        EXPECT_THROWS(
            std::runtime_error,
            start.branch(
                std::make_unique<IfImage>(targetPath, config),
                BranchMap{{targetPath, [](std::unique_ptr<ClickerBase> current) { return current; }}}
            )
        );

        bool stoppedHandlerCalled = false;
        auto stopped = start.branch(
            std::make_unique<StopTrueUntil>(),
            BranchMap{
                {QStringLiteral("test"),
                    [&](std::unique_ptr<ClickerBase> current) {
                        stoppedHandlerCalled = true;
                        return current;
                    }},
            }
        );
        EXPECT_FALSE(stoppedHandlerCalled);
        EXPECT_EQ(stopped->target, QStringLiteral("当前页面"));
        EXPECT_EQ(stopped->kind, MatchKind::IMAGE);
        EXPECT_EQ(stopped->targetSegmentList, start.targetSegmentList);
    }

    void testJsonWorkflowExecutesSelectedBranchHandler() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat targetTemplate(8, 10, CV_8UC1);
        cv::Mat missingTemplate(8, 10, CV_8UC1);
        cv::Mat screen(70, 90, CV_8UC1);
        cv::RNG targetRng(0x2741);
        cv::RNG missingRng(0x6193);
        cv::RNG screenRng(0x8532);
        targetRng.fill(targetTemplate, cv::RNG::UNIFORM, 0, 256);
        missingRng.fill(missingTemplate, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        targetTemplate.copyTo(screen(cv::Rect(40, 30, targetTemplate.cols, targetTemplate.rows)));

        const QString targetPath = writePng(targetTemplate, directory.filePath("target.png"));
        const QString missingPath = writePng(missingTemplate, directory.filePath("missing.png"));
        FakePlatform platform(screen);
        FakeOcrProvider provider;
        EnvScope scope(&platform, &provider);

        const auto clickStep = [](const int offsetX, const int offsetY) {
            return QJsonObject{
                {"action", "click"},
                {"runConfig", QJsonObject{{"homing", false}}},
                {"interval", 0},
                {"offsetX", offsetX},
                {"offsetY", offsetY},
            };
        };
        QJsonObject branches;
        branches.insert(missingPath, QJsonArray{clickStep(-20, 0)});
        branches.insert(targetPath, QJsonArray{clickStep(3, -2)});

        const Workflow workflow = parseWorkflow(
            QJsonObject{
                {"version", 1},
                {"name", "JSON branch"},
                {"clicker",
                    QJsonObject{
                        {"kind", "IMAGE"},
                        {"target", targetPath},
                        {"config", QJsonObject{{"threshold", 0.999}, {"timeout", 1}}},
                    }},
                {"steps",
                    QJsonArray{
                        QJsonObject{
                            {"action", "branch"},
                            {"condition",
                                QJsonObject{
                                    {"type", "AnyImage"},
                                    {"targets", QJsonArray{missingPath, targetPath}},
                                    {"config", QJsonObject{{"threshold", 0.999}, {"interval", 0}, {"timeout", 1}}},
                                }},
                            {"branches", branches},
                        },
                    }},
            }
        );

        EXPECT_EQ(workflow.stepCount(), size_t(1));
        const auto result = workflow.run();
        EXPECT_TRUE(static_cast<bool>(result));
        EXPECT_EQ(result->target, targetPath);
        EXPECT_EQ(result->kind, MatchKind::IMAGE);
        const auto clicks = platform.clickPoints();
        EXPECT_EQ(clicks.size(), size_t(1));
        EXPECT_EQ(clicks.front(), QPoint(48, 32));
        EXPECT_EQ(platform.captures, 2);
        EXPECT_EQ(provider.calls, 0);
    }

    QJsonObject imageCondition(
        const QString& type,
        const QString& target,
        const float threshold = 0.999f
    ) {
        return {
            {"type", type},
            {"target", target},
            {"config",
                QJsonObject{
                    {"threshold", threshold},
                    {"interval", 0},
                }},
        };
    }

    QJsonObject textCondition(
        const QString& type,
        const QString& target
    ) {
        return {
            {"type", type},
            {"target", target},
            {"config",
                QJsonObject{
                    {"match", "EXACT"},
                    {"interval", 0},
                }},
        };
    }

    QJsonObject mixedWorkflowJson(
        const QString& initialPath,
        const QString& finalPath
    ) {
        return {
            {"version", 1},
            {"name", QString::fromUtf8("JSON图片文字链")},
            {"clicker",
                QJsonObject{
                    {"kind", "IMAGE"},
                    {"target", initialPath},
                    {"config",
                        QJsonObject{
                            {"threshold", 0.999},
                            {"mode", "GRAY"},
                        }},
                }},
            {"steps",
                QJsonArray{
                    QJsonObject{
                        {"action", "locate"},
                        {"runConfig",
                            QJsonObject{
                                {"finishUntilList", QJsonArray{textCondition("Text", QString::fromUtf8("文字目标"))}},
                                {"homing", false},
                            }},
                    },
                    QJsonObject{
                        {"action", "locate"},
                        {"runConfig",
                            QJsonObject{
                                {"finishUntilList", QJsonArray{imageCondition("Image", finalPath)}},
                                {"homing", false},
                            }},
                    },
                    QJsonObject{
                        {"action", "scroll"},
                        {"runConfig",
                            QJsonObject{
                                {"selector",
                                    QJsonObject{
                                        {"type", "position"},
                                        {"basis", "X_CENTER"},
                                        {"method", "MAX"},
                                    }},
                                {"homing", false},
                            }},
                        {"delta", WheelDelta},
                        {"interval", 0},
                        {"offsetX", 2},
                        {"offsetY", -3},
                        {"position", "CENTER"},
                    },
                }},
        };
    }

    void testJsonWorkflowRunsTheExistingClickerChainAndReturnsTheLastClicker() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat initialTemplate(9, 11, CV_8UC1);
        cv::Mat finalTemplate(8, 10, CV_8UC1);
        cv::Mat screen(90, 120, CV_8UC1);
        cv::RNG initialRng(0x7512);
        cv::RNG finalRng(0x2357);
        cv::RNG screenRng(0x9154);
        initialRng.fill(initialTemplate, cv::RNG::UNIFORM, 0, 256);
        finalRng.fill(finalTemplate, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        initialTemplate.copyTo(screen(cv::Rect(12, 17, initialTemplate.cols, initialTemplate.rows)));
        finalTemplate.copyTo(screen(cv::Rect(76, 54, finalTemplate.cols, finalTemplate.rows)));
        const QString initialPath = writePng(initialTemplate, directory.filePath("initial.png"));
        const QString finalPath = writePng(finalTemplate, directory.filePath("final.png"));

        FakePlatform platform(screen);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult({token(QString::fromUtf8("文字目标"), QRect(35, 28, 24, 12))});
        };
        EnvScope scope(&platform, &provider);

        const QByteArray json = QJsonDocument(mixedWorkflowJson(initialPath, finalPath)).toJson(QJsonDocument::Compact);
        const Workflow workflow = parseWorkflow(json);
        EXPECT_EQ(workflow.name(), QString::fromUtf8("JSON图片文字链"));
        EXPECT_EQ(workflow.stepCount(), std::size_t(3));

        auto last = workflow.run();
        EXPECT_TRUE(static_cast<bool>(last));
        EXPECT_EQ(last->kind, MatchKind::IMAGE);
        EXPECT_EQ(last->target, finalPath);
        EXPECT_TRUE(last->founded());
        EXPECT_EQ(last->targetSegmentList.front().x, 76);
        EXPECT_EQ(last->targetSegmentList.front().y, 54);
        EXPECT_EQ(provider.calls, 1);
        EXPECT_EQ(platform.wheelEvents().size(), std::size_t(1));
        EXPECT_EQ(platform.wheelEvents().front().from, QPoint(83, 55));
        EXPECT_EQ(platform.wheelEvents().front().delta, WheelDelta);

        const QString workflowPath = directory.filePath("workflow.json");
        QFile file(workflowPath);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        EXPECT_EQ(file.write(json), static_cast<qint64>(json.size()));
        file.close();

        const Workflow fromFile = parseWorkflowFile(workflowPath);
        auto rerun = fromFile.run();
        EXPECT_EQ(rerun->kind, MatchKind::IMAGE);
        EXPECT_EQ(rerun->target, finalPath);
        EXPECT_EQ(provider.calls, 2);
        EXPECT_EQ(platform.wheelEvents().size(), std::size_t(2));
    }

    void testJsonWorkflowRejectsInvalidApiCombinationsDuringParsing() {
        const auto valid = mixedWorkflowJson("initial.png", "final.png");

        auto badVersion = valid;
        badVersion["version"] = 2;
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(badVersion));

        auto emptyScales = valid;
        auto imageClicker = emptyScales["clicker"].toObject();
        auto imageConfig = imageClicker["config"].toObject();
        imageConfig["scales"] = QJsonArray{};
        imageClicker["config"] = imageConfig;
        emptyScales["clicker"] = imageClicker;
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(emptyScales));

        auto invalidScale = valid;
        imageClicker = invalidScale["clicker"].toObject();
        imageConfig = imageClicker["config"].toObject();
        imageConfig["scales"] = QJsonArray{0};
        imageClicker["config"] = imageConfig;
        invalidScale["clicker"] = imageClicker;
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(invalidScale));

        auto textSelector = valid;
        auto clicker = textSelector["clicker"].toObject();
        clicker["kind"] = "TEXT";
        textSelector["clicker"] = clicker;
        auto steps = textSelector["steps"].toArray();
        auto first = steps[0].toObject();
        auto runConfig = first["runConfig"].toObject();
        runConfig["selector"] = QJsonObject{{"type", "similarity"}};
        first["runConfig"] = runConfig;
        steps[0] = first;
        textSelector["steps"] = steps;
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(textSelector));

        auto locateWithRunUntil = valid;
        steps = locateWithRunUntil["steps"].toArray();
        first = steps[0].toObject();
        runConfig = first["runConfig"].toObject();
        runConfig["runUntilList"] = QJsonArray{imageCondition("Image", "target.png")};
        first["runConfig"] = runConfig;
        steps[0] = first;
        locateWithRunUntil["steps"] = steps;
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(locateWithRunUntil));

        const QJsonObject reverseBranch{
            {"version", 1},
            {"name", "Invalid reverse branch"},
            {"clicker",
                QJsonObject{
                    {"kind", "IMAGE"},
                    {"target", "initial.png"},
                    {"config", QJsonObject{}},
                }},
            {"steps",
                QJsonArray{
                    QJsonObject{
                        {"action", "branch"},
                        {"condition",
                            QJsonObject{
                                {"type", "IfImage"},
                                {"target", "target.png"},
                                {"config", QJsonObject{{"reverse", true}}},
                            }},
                        {"branches", QJsonObject{{"target.png", QJsonArray{}}}},
                    },
                }},
        };
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(reverseBranch));

        EXPECT_THROWS(std::invalid_argument, parseWorkflow(QByteArray("[1,2,3]")));
        EXPECT_THROWS(std::invalid_argument, parseWorkflow(QByteArray("{broken")));
    }

    void testJsonWorkflowParsesEveryCurrentActionAndUntilType() {
        QJsonArray allConditions;
        for (const QString& type : QStringList{"Image", "ImageStable", "IfImage"}) {
            allConditions.append(imageCondition(type, "image.png"));
        }
        for (const QString& type : QStringList{"AnyImage", "IfAnyImage"}) {
            allConditions.append(
                QJsonObject{
                    {"type", type},
                    {"targets", QJsonArray{"first.png", "second.png"}},
                    {"config", QJsonObject{{"interval", 0}}},
                }
            );
        }
        for (const QString& type : QStringList{"Text", "TextStable", "IfText"}) {
            allConditions.append(textCondition(type, QString::fromUtf8("文字")));
        }
        for (const QString& type : QStringList{"AnyText", "IfAnyText"}) {
            allConditions.append(
                QJsonObject{
                    {"type", type},
                    {"targets", QJsonArray{QString::fromUtf8("甲"), QString::fromUtf8("乙")}},
                    {"config", QJsonObject{{"match", "EXACT"}, {"interval", 0}}},
                }
            );
        }

        const QJsonObject object{
            {"version", 1},
            {"name", "API coverage"},
            {"clicker",
                QJsonObject{
                    {"kind", "IMAGE"},
                    {"target", "initial.png"},
                    {"config", QJsonObject{}},
                }},
            {"steps",
                QJsonArray{
                    QJsonObject{
                        {"action", "click"},
                        {"runConfig",
                            QJsonObject{
                                {"startUntilList", allConditions},
                                {"selector",
                                    QJsonObject{
                                        {"type", "orderedRandom"},
                                        {"basis", "Y_CENTER"},
                                        {"method", "MIN"},
                                        {"top", 2},
                                    }},
                            }},
                    },
                    QJsonObject{
                        {"action", "drag"},
                        {"runConfig",
                            QJsonObject{
                                {"finishUntilList", QJsonArray{textCondition("Text", QString::fromUtf8("完成"))}},
                            }},
                        {"step", 12},
                        {"reverse", true},
                    },
                    QJsonObject{
                        {"action", "locate"},
                        {"runConfig",
                            QJsonObject{
                                {"finishUntilList",
                                    QJsonArray{
                                        QJsonObject{
                                            {"type", "AnyImage"},
                                            {"targets", QJsonArray{"first.png", "second.png"}},
                                        },
                                    }},
                            }},
                    },
                    QJsonObject{
                        {"action", "scroll"},
                        {"runConfig", QJsonObject{}},
                    },
                }},
        };
        const Workflow workflow = parseWorkflow(object);
        EXPECT_EQ(workflow.stepCount(), std::size_t(4));

        const QJsonObject textEntry{
            {"version", 1},
            {"name", "Text entry"},
            {"clicker",
                QJsonObject{
                    {"kind", "TEXT"},
                    {"target", QString::fromUtf8("确认")},
                    {"config",
                        QJsonObject{
                            {"mode", "RGB"},
                            {"region", QJsonObject{{"x", 10}, {"y", 20}, {"width", 100}, {"height", 50}}},
                            {"match",
                                QJsonObject{
                                    {"match", "FUZZY"},
                                    {"caseSensitivity", "INSENSITIVE"},
                                    {"maxEditDistance", 2},
                                    {"candidates", QJsonArray{}},
                                }},
                            {"resolvedRegion", QJsonValue::Null},
                        }},
                }},
            {"steps", QJsonArray{}},
        };
        EXPECT_EQ(parseWorkflow(textEntry).stepCount(), std::size_t(0));
    }

    void testUnifiedClickerImageTextImageChain() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat templ(9, 11, CV_8UC1);
        cv::Mat screen(80, 100, CV_8UC1);
        cv::RNG templateRng(0x7812);
        cv::RNG screenRng(0x9914);
        templateRng.fill(templ, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        templ.copyTo(screen(cv::Rect(62, 43, templ.cols, templ.rows)));
        const QString imagePath = writePng(templ, directory.filePath("finish.png"));

        FakePlatform platform(screen);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult({token("文字目标", QRect(20, 15, 24, 12))});
        };
        EnvScope scope(&platform, &provider);

        TextUntilConfig textConfig;
        textConfig.match = TextMatch::EXACT;
        textConfig.interval = 0;
        ImageUntilConfig imageConfig;
        imageConfig.threshold = 0.999f;
        imageConfig.interval = 0;

        const Segment initial(5, 6, 10, 10, 1.0f);
        ImageClicker start("cached", initial);
        EXPECT_EQ(start.kind, MatchKind::IMAGE);

        auto textNext = start.locate(
            ImageRunConfig{
                .finishUntilList = {new Text("文字目标", textConfig)},
                .homing = false,
            }
        );
        EXPECT_EQ(textNext->kind, MatchKind::TEXT);
        EXPECT_EQ(textNext->target, QStringLiteral("文字目标"));
        EXPECT_EQ(textNext->targetSegmentList.front().x, 20);
        EXPECT_EQ(textNext->targetSegmentList.front().y, 15);
        EXPECT_THROWS(std::invalid_argument, textNext->locate(ImageRunConfig{.homing = false}));

        auto imageNext = textNext->locate(
            TextRunConfig{
                .finishUntilList = {new Image(imagePath, imageConfig)},
                .homing = false,
            }
        );
        EXPECT_EQ(imageNext->kind, MatchKind::IMAGE);
        EXPECT_EQ(imageNext->target, imagePath);
        EXPECT_TRUE(imageNext->founded());
        EXPECT_EQ(imageNext->targetSegmentList.front().x, 62);
        EXPECT_EQ(imageNext->targetSegmentList.front().y, 43);
        EXPECT_EQ(provider.calls, 1);
        EXPECT_THROWS(std::invalid_argument, imageNext->locate(TextRunConfig{.homing = false}));
    }

    void testUnifiedClickerTextAnyImageAnyTextChain() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat templ(8, 10, CV_8UC1);
        cv::Mat missing(8, 10, CV_8UC1);
        cv::Mat screen(80, 100, CV_8UC1);
        cv::RNG templateRng(0x1357);
        cv::RNG missingRng(0x2468);
        cv::RNG screenRng(0x3691);
        templateRng.fill(templ, cv::RNG::UNIFORM, 0, 256);
        missingRng.fill(missing, cv::RNG::UNIFORM, 0, 256);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        templ.copyTo(screen(cv::Rect(35, 28, templ.cols, templ.rows)));
        const QString imagePath = writePng(templ, directory.filePath("target.png"));
        const QString missingPath = writePng(missing, directory.filePath("missing.png"));

        FakePlatform platform(screen);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) { return successfulResult({token("完成", QRect(55, 40, 20, 10))}); };
        EnvScope scope(&platform, &provider);

        ImageUntilConfig imageConfig;
        imageConfig.threshold = 0.999f;
        imageConfig.interval = 0;
        TextUntilConfig textConfig;
        textConfig.match = TextMatch::EXACT;
        textConfig.interval = 0;

        const Segment initial(10, 10, 20, 10, 1.0f);
        TextClicker start(QStringLiteral("开始"), initial);
        EXPECT_EQ(start.kind, MatchKind::TEXT);

        auto imageNext = start.locate(
            TextRunConfig{
                .finishUntilList = {new AnyImage({missingPath, imagePath}, imageConfig)},
                .homing = false,
            }
        );
        EXPECT_EQ(imageNext->kind, MatchKind::IMAGE);
        EXPECT_EQ(imageNext->target, imagePath);

        auto textNext = imageNext->locate(
            ImageRunConfig{
                .finishUntilList = {new AnyText({"missing", "完成"}, textConfig)},
                .homing = false,
            }
        );
        EXPECT_EQ(textNext->kind, MatchKind::TEXT);
        EXPECT_EQ(textNext->target, QStringLiteral("完成"));
        EXPECT_TRUE(textNext->founded());
        EXPECT_EQ(provider.calls, 1);
    }

    void testScrollSupportsEveryUntilFamilyAndOptionalConditionsAreOneShot() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat templ(8, 10, CV_8UC1);
        cv::Mat missingTemplate(8, 10, CV_8UC1);
        cv::RNG templateRng(0x4211);
        cv::RNG missingRng(0x9823);
        templateRng.fill(templ, cv::RNG::UNIFORM, 0, 256);
        missingRng.fill(missingTemplate, cv::RNG::UNIFORM, 0, 256);

        cv::Mat screen(80, 100, CV_8UC1);
        cv::RNG screenRng(0x7854);
        screenRng.fill(screen, cv::RNG::UNIFORM, 0, 80);
        templ.copyTo(screen(cv::Rect(12, 14, templ.cols, templ.rows)));

        const QString path = writePng(templ, directory.filePath("target.png"));
        const QString missingPath = writePng(missingTemplate, directory.filePath("missing.png"));

        FakePlatform platform(screen);
        FakeOcrProvider provider;
        provider.handler = [](const cv::Mat&, int) {
            return successfulResult(
                {
                    token("alpha", QRect(55, 10, 20, 10)),
                    token("beta", QRect(55, 30, 20, 10)),
                }
            );
        };
        EnvScope scope(&platform, &provider);

        ImageUntilConfig imageConfig;
        imageConfig.threshold = 0.999f;
        imageConfig.interval = 0;
        TextUntilConfig textConfig;
        textConfig.match = TextMatch::EXACT;
        textConfig.interval = 0;

        const Segment target(20, 50, 10, 10, 1.0f);
        ImageClicker clicker("cached", target, ImageInitConfig{.timeout = 1.0f});

        auto afterOptional = clicker.scroll(
            ImageRunConfig{
                .startUntilList =
                    {
                        new IfImage(missingPath, imageConfig),
                        new IfAnyImage({missingPath}, imageConfig),
                        new IfText("missing", textConfig),
                        new IfAnyText({"missing"}, textConfig),
                    },
                .homing = false,
            },
            -WHEEL_DELTA,
            0.0f
        );
        EXPECT_TRUE(static_cast<bool>(afterOptional));
        EXPECT_EQ(platform.wheelEvents().size(), size_t(1));
        EXPECT_EQ(platform.captures, 4);
        EXPECT_EQ(provider.calls, 2);

        platform.events.clear();
        platform.captures = 0;
        provider.calls = 0;
        provider.inputs.clear();

        auto next = afterOptional->scroll(
            ImageRunConfig{
                .runUntilList =
                    {
                        new Image(path, imageConfig),
                        new AnyImage({missingPath, path}, imageConfig),
                        new IfImage(path, imageConfig),
                        new IfAnyImage({missingPath, path}, imageConfig),
                        new Text("alpha", textConfig),
                        new AnyText({"missing", "beta"}, textConfig),
                        new IfText("alpha", textConfig),
                        new IfAnyText({"missing", "beta"}, textConfig),
                        new ImageStable(path, imageConfig),
                        new TextStable("alpha", textConfig),
                    },
                .homing = false,
            },
            -240,
            0.0f
        );

        EXPECT_TRUE(static_cast<bool>(next));
        EXPECT_EQ(next->kind, MatchKind::TEXT);
        EXPECT_TRUE(next->founded());
        EXPECT_EQ(next->target, QStringLiteral("alpha"));

        const auto wheels = platform.wheelEvents();
        EXPECT_EQ(wheels.size(), size_t(4));
        for (const auto& event : wheels) {
            EXPECT_EQ(event.from, QPoint(25, 55));
            EXPECT_EQ(event.delta, -240);
        }
        EXPECT_EQ(provider.calls, 23);
    }

    void testImageTemplateMatchingUsesRoiGlobalCoordinatesAndNms() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat templ(12, 14, CV_8UC1);
        cv::RNG templateRng(0x12345);
        templateRng.fill(templ, cv::RNG::UNIFORM, 0, 256);

        cv::Mat source(70, 90, CV_8UC1);
        cv::RNG sourceRng(0x54321);
        sourceRng.fill(source, cv::RNG::UNIFORM, 0, 80);
        templ.copyTo(source(cv::Rect(10, 8, templ.cols, templ.rows)));
        templ.copyTo(source(cv::Rect(55, 30, templ.cols, templ.rows)));

        const QString path = writePng(templ, directory.filePath("template.png"));
        const auto all = CV::findPositions(source, path, 0.999f, Mode::GRAY);
        EXPECT_EQ(all.size(), size_t(2));
        EXPECT_TRUE(std::any_of(all.begin(), all.end(), [](const Segment& hit) { return hit.x == 10 && hit.y == 8; }));
        EXPECT_TRUE(std::any_of(all.begin(), all.end(), [](const Segment& hit) { return hit.x == 55 && hit.y == 30; }));
        for (size_t left = 0; left < all.size(); ++left) {
            for (size_t right = left + 1; right < all.size(); ++right) {
                EXPECT_TRUE(
                    std::abs(all[left].x - all[right].x) >= templ.cols / 2 ||
                    std::abs(all[left].y - all[right].y) >= templ.rows / 2
                );
            }
        }

        const auto inRoi = CV::findPositions(source, path, 0.999f, Mode::GRAY, QRect(45, 20, 35, 35));
        EXPECT_EQ(inRoi.size(), size_t(1));
        EXPECT_EQ(inRoi.front().x, 55);
        EXPECT_EQ(inRoi.front().y, 30);
    }

    void testImageTemplateMatchingUsesConfiguredDisplayScales() {
        QTemporaryDir directory;
        EXPECT_TRUE(directory.isValid());

        cv::Mat templ(24, 32, CV_8UC3);
        cv::RNG templateRng(0x42817);
        templateRng.fill(templ, cv::RNG::UNIFORM, 0, 256);
        const QString path = writePng(templ, directory.filePath("template.png"));
        const std::vector<float> scales{1.0f, 1.25f, 1.3f, 1.5f, 1.75f, 2.0f};

        for (const Mode mode : {Mode::GRAY, Mode::RGB}) {
            for (const float scale : scales) {
                cv::Mat source(180, 240, CV_8UC3);
                cv::RNG sourceRng(static_cast<uint64>(scale * 1000) + (mode == Mode::GRAY ? 1 : 2));
                sourceRng.fill(source, cv::RNG::UNIFORM, 0, 60);

                cv::Mat scaled;
                const cv::Size scaledSize(
                    cvRound(static_cast<double>(templ.cols) * scale),
                    cvRound(static_cast<double>(templ.rows) * scale)
                );
                cv::resize(templ, scaled, scaledSize, 0, 0, cv::INTER_LINEAR);
                const int x = 71;
                const int y = 53;
                scaled.copyTo(source(cv::Rect(x, y, scaled.cols, scaled.rows)));

                const auto hits = CV::findPositions(
                    source,
                    path,
                    0.995f,
                    mode,
                    QRect(x - 5, y - 5, scaled.cols + 10, scaled.rows + 10),
                    std::vector<float>{scale}
                );
                EXPECT_EQ(hits.size(), size_t(1));
                EXPECT_EQ(hits.front().x, x);
                EXPECT_EQ(hits.front().y, y);
                EXPECT_EQ(hits.front().width, scaled.cols);
                EXPECT_EQ(hits.front().height, scaled.rows);
                EXPECT_TRUE(hits.front().score >= 0.995f);

                if (scale == 1.5f) {
                    const auto wrongScale = CV::findPositions(
                        source,
                        path,
                        0.995f,
                        mode,
                        QRect(x - 5, y - 5, scaled.cols + 10, scaled.rows + 10),
                        std::vector<float>{1.0f}
                    );
                    EXPECT_TRUE(wrongScale.empty());
                }
            }
        }

        EXPECT_EQ(ImageInitConfig{}.scales.size(), std::size_t(5));
        EXPECT_EQ(ImageUntilConfig{}.scales.size(), std::size_t(5));
        EXPECT_THROWS(
            std::invalid_argument,
            CV::findPositions(templ, path, 0.9f, Mode::GRAY, {}, std::vector<float>{})
        );
        EXPECT_THROWS(
            std::invalid_argument,
            CV::findPositions(templ, path, 0.9f, Mode::GRAY, {}, std::vector<float>{0})
        );
    }

    void testSelectorsAndPreviousFilter() {
        const std::vector<Segment> candidates{
            Segment(10, 45, 10, 10, 0.2f),
            Segment(70, 45, 10, 10, 0.9f),
            Segment(45, 45, 5, 5, 0.5f),
        };
        EXPECT_EQ(similaritySelector(candidates).score, 0.9f);
        EXPECT_EQ(positionSelector(SelectorBasis::X_CENTER, SelectorMethod::MIN)(candidates).x, 10);
        EXPECT_EQ(positionSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX)(candidates).x, 70);
        const Segment random = randomSelector(candidates);
        EXPECT_TRUE(std::any_of(candidates.begin(), candidates.end(), [&random](const Segment& candidate) {
            return candidate == random;
        }));
        const Segment orderedRandom =
            orderedRandomSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX, 2)(candidates);
        EXPECT_TRUE(orderedRandom.x == 70 || orderedRandom.x == 45);
        EXPECT_THROWS(
            std::invalid_argument,
            orderedRandomSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX, 0)(candidates)
        );
        EXPECT_THROWS(
            std::invalid_argument,
            positionSelector(static_cast<SelectorBasis>(999), SelectorMethod::MIN)(candidates)
        );
        EXPECT_THROWS(std::runtime_error, randomSelector(std::vector<Segment>{}));
        EXPECT_THROWS(std::runtime_error, similaritySelector(std::vector<Segment>{}));

        auto previous = std::make_unique<Segment>(40, 40, 20, 20, 1.0f);
        const Segment leftCenter(10, 45, 10, 10, 1.0f);
        const Segment topCenter(45, 10, 10, 10, 1.0f);
        const Segment inner(45, 45, 5, 5, 1.0f);
        const Segment partialOverlap(35, 45, 10, 10, 1.0f);
        const std::vector<Segment> positions{leftCenter, topCenter, inner, partialOverlap};

        FilterUntil leftFilter(ImageUntilConfig{.onPrevious = Previous::LEFT_CENTER});
        const auto left = leftFilter.filter(positions, previous);
        EXPECT_EQ(left.size(), size_t(1));
        EXPECT_TRUE(left.front() == leftCenter);

        FilterUntil innerFilter(ImageUntilConfig{.onPrevious = Previous::INNER});
        const auto inside = innerFilter.filter(positions, previous);
        EXPECT_EQ(inside.size(), size_t(1));
        EXPECT_TRUE(inside.front() == inner);

        std::unique_ptr<Segment> missingPrevious;
        EXPECT_THROWS(std::runtime_error, innerFilter.filter(positions, missingPrevious));
    }

    void testBundledOcrEngineLoadsAndRuns() {
        const QString bundledModels = QCoreApplication::applicationDirPath() + "/models";
        QTemporaryDir temporary;
        EXPECT_TRUE(temporary.isValid());
        const QString models = QDir(temporary.path()).filePath(QString::fromUtf8("模型"));
        EXPECT_TRUE(QDir().mkpath(models));
        for (const QString& name : QStringList{"det.onnx", "cls.onnx", "rec.onnx", "keys.txt"}) {
            EXPECT_TRUE(QFile::copy(QDir(bundledModels).filePath(name), QDir(models).filePath(name)));
        }

        OcrEngine engine(models);
        QString error;
        EXPECT_TRUE(engine.initialize(&error));
        EXPECT_TRUE(error.isEmpty());

        cv::Mat fixture(180, 640, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::putText(
            fixture,
            "HELLO 12345",
            cv::Point(30, 115),
            cv::FONT_HERSHEY_SIMPLEX,
            2.2,
            cv::Scalar(0, 0, 0),
            5,
            cv::LINE_AA
        );
        const OcrRunResult result = engine.recognize(fixture);
        EXPECT_TRUE(result.ok);
        EXPECT_TRUE(result.error.isEmpty());
        EXPECT_EQ(result.boxedImage.cols, fixture.cols);
        EXPECT_EQ(result.boxedImage.rows, fixture.rows);
        EXPECT_TRUE(!result.tokens.empty());
        EXPECT_TRUE(std::any_of(result.tokens.begin(), result.tokens.end(), [](const OcrToken& token) {
            return !token.text.trimmed().isEmpty() && std::isfinite(token.confidence);
        }));
    }

    void testBrowserJsonWorkflowsAllParse() {
        const QDir root(QString::fromUtf8(WORKFLOW_TEST_WORKFLOW_ROOT));
        const QStringList files = root.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        EXPECT_EQ(files.size(), 32);

        for (const QString& file : files) {
            const Workflow workflow = parseWorkflowFile(root.filePath(file));
            EXPECT_TRUE(!workflow.name().isEmpty());
            EXPECT_TRUE(workflow.stepCount() > 0);
        }
    }

    using Test = std::pair<const char*, std::function<void()>>;

} // namespace

int main(
    int argc,
    char** argv
) {
    QCoreApplication application(argc, argv);
    const std::vector<Test> tests{
        {"OCR crops before provider and maps coordinates", testOcrCropsBeforeProviderAndMapsCoordinates},
        {"empty OCR intersection skips provider", testEmptyOcrIntersectionSkipsProvider},
        {"resolveRegion implements every Previous relation", testResolveRegionAllPreviousRelations},
        {"FindAny and AnyText infer once in candidate order", testFindAnyAndAnyTextUseOneInferenceAndCandidateOrder},
        {"text exact contains fuzzy unique matching", testTextMatchModesAndUniqueFuzzyCandidate},
        {"Text onPrevious crops and maps back", testTextOnPreviousCropsBeforeOcrAndMapsBack},
        {"reverse conditions fail closed on infrastructure errors",
            testInfrastructureFailuresDoNotFulfillReverseConditions},
        {"optional conditions honor a preexisting stop without inference",
            testOptionalConditionsHonorPreexistingStopWithoutInference},
        {"condition ownership and stop suppress input", testConditionOwnershipAndStopAreSafe},
        {"Text target returns a non-null terminal chain", testTextTargetHasNonNullTerminalChain},
        {"finish Text propagates into a clickable next Segment", testFinishTextPropagatesToNextClickableSegment},
        {"Win32 wheel message contract and boundaries", testWin32MouseWheelMessageContractAndBoundaries},
        {"scroll one-shot honors selector coordinates and delta", testScrollOneShotUsesSelectorCoordinatesAndDelta},
        {"scroll stop and timeout are safe", testScrollStopAndTimeoutAreSafe},
        {"scroll finish Text creates the next text target", testScrollFinishTextCreatesNextTextTarget},
        {"unified Clicker chains image to text to image", testUnifiedClickerImageTextImageChain},
        {"unified Clicker preserves AnyImage and AnyText targets", testUnifiedClickerTextAnyImageAnyTextChain},
        {"branch dispatches IfImage and continues the main chain on miss",
            testBranchDispatchesIfImageAndContinuesMainChainOnMiss},
        {"branch dispatches Any and IfAny by the selected target", testBranchDispatchesAnyAndIfAnyBySelectedTarget},
        {"branch rejects invalid handlers and honors stop", testBranchRejectsInvalidHandlersAndHonorsStop},
        {"JSON workflow executes the selected branch handler", testJsonWorkflowExecutesSelectedBranchHandler},
        {"scroll supports every Until family and optional conditions are one-shot",
            testScrollSupportsEveryUntilFamilyAndOptionalConditionsAreOneShot},
        {"JSON workflow executes the existing Clicker chain and returns its last Clicker",
            testJsonWorkflowRunsTheExistingClickerChainAndReturnsTheLastClicker},
        {"JSON workflow rejects invalid API combinations during parsing",
            testJsonWorkflowRejectsInvalidApiCombinationsDuringParsing},
        {"JSON workflow parses every current action and Until type",
            testJsonWorkflowParsesEveryCurrentActionAndUntilType},
        {"all browser JSON workflows parse with the production parser", testBrowserJsonWorkflowsAllParse},
        {"image matching honors ROI global coordinates and NMS",
            testImageTemplateMatchingUsesRoiGlobalCoordinatesAndNms},
        {"image matching uses configured display scales", testImageTemplateMatchingUsesConfiguredDisplayScales},
        {"selectors and Previous filter", testSelectorsAndPreviousFilter},
        {"bundled RapidOCR models load and run", testBundledOcrEngineLoadsAndRuns},
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": unknown exception\n";
        }
    }

    std::cout << tests.size() - static_cast<size_t>(failures) << "/" << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
