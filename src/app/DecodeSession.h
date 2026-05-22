#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QObject>
#include <QPointer>

class DecodeWorker;
class QThread;

class DecodeSession : public QObject
{
    Q_OBJECT

public:
    explicit DecodeSession(QObject *parent = nullptr);
    ~DecodeSession() override;

    int generation() const;
    bool isActive() const;

    void start(const QString &filePath,
               int startFrameIndex = 0,
               bool pauseAfterFirstFrame = false,
               const FrameSeekCheckpoint &seekCheckpoint = FrameSeekCheckpoint {});
    void stop();
    void play();
    void pause();
    void stepForward();

signals:
    void streamOpened(const StreamInfo &streamInfo);
    void frameReady(int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis);
    void accessUnitAnalysisDecoded(const FrameAnalysis &analysis);
    void seekCheckpointReady(const FrameSeekCheckpoint &checkpoint);
    void bufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void stopped();

private:
    void stopCurrent(bool emitStoppedSignal);
    void finishGeneration(int generation, bool emitStoppedSignal);

    QPointer<QThread> m_thread;
    QPointer<DecodeWorker> m_worker;
    int m_generation = 0;
    int m_threadGeneration = 0;
    int m_finishedGeneration = -1;
};
