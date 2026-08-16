#ifndef WORKFLOW_TEST_APP_WINDOW_H
#define WORKFLOW_TEST_APP_WINDOW_H

#include <QMainWindow>
#include <QStringList>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "cases.h"

class Emitter;
class OcrEngine;
class QCloseEvent;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;
class QThread;
class QWebEngineView;

void configureDeterministicBrowserProcess();

class TestWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit TestWindow(QWidget* parent = nullptr);
    ~TestWindow() override;

    void startAutomaticRun(std::vector<int> caseIds = {}, std::optional<TestWorkflowSource> source = std::nullopt);

signals:
    void automaticRunFinished(int exitCode);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct BatchRun {
        int caseId;
        TestWorkflowSource source;
    };

    QWebEngineView* browser{};
    QListWidget* caseList{};
    QLabel* caseTitle{};
    QLabel* caseDetails{};
    QLabel* environmentStatus{};
    QLabel* runStatus{};
    QComboBox* workflowSource{};
    QPushButton* runButton{};
    QPushButton* runAllButton{};
    QPushButton* resetButton{};
    QPushButton* stopButton{};
    QPushButton* indexButton{};
    QTextEdit* output{};

    Emitter* emitter{};
    std::unique_ptr<OcrEngine> ocr;
    std::atomic_bool stopFlag{false};
    QThread* worker{};
    QString webRoot;
    QString previousLog;
    int currentCaseId = 0;
    bool pageReady = false;
    bool ocrReady = false;
    bool batchRunning = false;
    bool batchWaitingForPage = false;
    int batchRunIndex = 0;
    int batchPassed = 0;
    int batchFailed = 0;
    bool automaticMode = false;
    bool automaticRunCompleted = false;
    int automaticStartupAttempts = 0;
    std::vector<int> automaticCaseIds;
    std::optional<TestWorkflowSource> automaticSource;
    std::vector<BatchRun> batchRuns;
    QStringList automaticResults;

    void buildInterface();
    void configureBrowser();
    void prepareFixtureImages() const;
    void loadIndex();
    void loadCase(int id);
    void synchronizeCaseFromUrl();
    void updateCaseDetails();
    void verifyPageEnvironment();
    void resetCurrentCase();
    void runCurrentCase();
    void runAllCases();
    void runNextBatchCase();
    void runBatchCurrentCase();
    void completeBatchCase(bool success, const QString& message);
    void finishBatch(bool stopped);
    void startAutomaticRunWhenReady();
    void failAutomaticRun(const QString& message);
    void completeAutomaticRun(const QString& summary, int exitCode);
    void printAutomaticResults(const QString& summary) const;
    void startCaseWorker(int id, TestWorkflowSource source);
    void finishCaseWorker(int id, TestWorkflowSource source, bool success, const QString& message);
    void stopCurrentCase();
    void setRunState(const QString& state, const QString& text);
    void updateControls();
    void appendLog(const QString& text, const QString& color = QStringLiteral("black"));
};

#endif // WORKFLOW_TEST_APP_WINDOW_H
