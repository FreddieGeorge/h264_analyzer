#include "ui/LogDock.h"

LogDock::LogDock(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("LogDock"));
    setReadOnly(true);
    setPlaceholderText(tr("Parser log output will appear here."));
}

void LogDock::appendLine(const QString &message)
{
    appendPlainText(message);
}

