#include "support/resources.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "core/environment.h"

QString res(
    const QString& name,
    const QString& dir
) {
    if (QFileInfo(name).isAbsolute()) return QDir::cleanPath(name);

    const QString root = env.resourceRoot.isEmpty() ? QCoreApplication::applicationDirPath() : env.resourceRoot;
    const QDir rootDirectory(root);
    if (dir.isEmpty()) return QDir::cleanPath(rootDirectory.filePath(name));
    return QDir::cleanPath(rootDirectory.filePath(QDir(dir).filePath(name)));
}
