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

[[nodiscard]] const std::vector<TestCase>& testCases();
void runTestCase(int id);

#endif // WORKFLOW_TEST_APP_CASES_H
