#ifndef WORKFLOW_SUPPORT_EMITTER_H
#define WORKFLOW_SUPPORT_EMITTER_H

#include <QObject>
#include <QString>

class Emitter : public QObject {
    Q_OBJECT

public:
    explicit Emitter(QObject* parent = nullptr);

signals:
    void log(const QString& text, const QString& color = "black") const;
    void error(const QString& text) const;
};

#endif // WORKFLOW_SUPPORT_EMITTER_H
