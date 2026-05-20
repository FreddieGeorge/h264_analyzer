#pragma once

#include "core/model/FrameAnalysis.h"
#include "core/model/StreamDocument.h"

#include <QObject>
#include <QString>
#include <QVector>

class VideoCanvas;
class QWidget;

class ExportController : public QObject
{
    Q_OBJECT

public:
    explicit ExportController(QWidget *dialogParent, QObject *parent = nullptr);

    void exportSelectedAccessUnitJson(const StreamInfo &streamInfo,
                                      bool hasCurrentAnalysis,
                                      const FrameAnalysis &analysis,
                                      const QString &defaultDirectory);
    void exportAllAccessUnitSyntaxJson(const StreamInfo &streamInfo,
                                       const QVector<FrameAnalysis> &analyses,
                                       const QString &defaultDirectory);
    void exportAccessUnitListCsv(const QVector<FrameAnalysis> &analyses,
                                 const QString &defaultDirectory);
    void exportScreenshot(VideoCanvas *videoCanvas, const QString &defaultDirectory);

signals:
    void exportDirectoryChanged(const QString &directory);
    void statusMessage(const QString &message, int timeoutMs);
    void logMessage(const QString &message);

private:
    void warn(const QString &message);
    void error(const QString &message);
    void reportExported(const QString &directory, const QString &logMessage, const QString &statusMessage);

    QWidget *m_dialogParent = nullptr;
};
