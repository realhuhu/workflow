#include "browser.h"

#include <QApplication>
#include <QLocale>
#include <QStyleFactory>

#include <exception>

int main(
    int argc,
    char* argv[]
) {
    configureDeterministicBrowserProcess();
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("workflow-browser-harness"));
    QApplication::setOrganizationName(QStringLiteral("workflow"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    try {
        FixtureBrowserWindow window;
        window.show();
        return QApplication::exec();
    } catch (const std::exception& exception) {
        qCritical("%s", exception.what());
        return 1;
    }
}
