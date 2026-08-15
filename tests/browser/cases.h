#ifndef WORKFLOW_TESTS_BROWSER_CASES_H
#define WORKFLOW_TESTS_BROWSER_CASES_H

#include <QString>

#include <vector>

struct BrowserCase {
    int id;
    QString slug;
    QString title;
    QString category;
    QString capability;
    QString target;
    QString expected;
    QString description;
};

[[nodiscard]] const std::vector<BrowserCase>& browserCases();
void runBrowserCase(int id);

#endif // WORKFLOW_TESTS_BROWSER_CASES_H
