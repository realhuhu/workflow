#ifndef WORKFLOW_TEST_APP_CASES_H
#define WORKFLOW_TEST_APP_CASES_H

#include <QString>

#include <vector>

struct TestCase {
    int id;
    QString slug;
    QString title;
    QString category;
    QString capability;
    QString target;
    QString expected;
    QString description;
};

enum class TestWorkflowSource {
    CPP,
    JSON,
};

[[nodiscard]] const std::vector<TestCase>& testCases();
void validateJsonTestCases(const QString& workflowRoot);
void runTestCase(int id, TestWorkflowSource source, const QString& workflowRoot);

#endif // WORKFLOW_TEST_APP_CASES_H
