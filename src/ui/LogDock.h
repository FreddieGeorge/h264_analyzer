#pragma once

#include <QPlainTextEdit>

class LogDock : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LogDock(QWidget *parent = nullptr);

    void appendLine(const QString &message);
};

