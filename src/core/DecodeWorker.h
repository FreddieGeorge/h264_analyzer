#pragma once

#include "core/FFmpegDecoder.h"

#include <QObject>
#include <QString>

#include <atomic>

class DecodeWorker : public QObject
{
    Q_OBJECT

public:
    explicit DecodeWorker(QObject *parent = nullptr);

public slots:
    void decodeFile(const QString &filePath);
    void stop();

signals:
    void streamOpened(const StreamInfo &streamInfo);
    void frameDecoded(const DecodedVideoFramePtr &frame);
    void frameSyntaxDecoded(const FrameSyntaxInfo &syntaxInfo);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void finished();

private:
    std::atomic_bool m_stopRequested = false;
};
