#pragma once

#include "core/FFmpegDecoder.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <mutex>

class DecodeWorker : public QObject
{
    Q_OBJECT

public:
    explicit DecodeWorker(QObject *parent = nullptr);

public slots:
    void decodeFile(const QString &filePath);
    void decodeFileFromFrame(const QString &filePath, int startFrameIndex, bool pauseAfterFirstFrame);
    void decodeFileFromCheckpoint(const QString &filePath,
                                  int targetFrameIndex,
                                  bool pauseAfterFirstFrame,
                                  const FrameSeekCheckpoint &checkpoint);
    void play();
    void pause();
    void setPaused(bool paused);
    void stepForward();
    void stop();

signals:
    void streamOpened(const StreamInfo &streamInfo);
    void frameDecoded(const DecodedVideoFramePtr &frame);
    void frameAnalysisDecoded(const FrameAnalysis &analysis);
    void seekCheckpointReady(const FrameSeekCheckpoint &checkpoint);
    void bufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex);
    void frameReady(int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void finished();

private:
    bool waitForPlaybackPermission();

    std::atomic_bool m_stopRequested = false;
    std::mutex m_controlMutex;
    std::condition_variable m_controlChanged;
    bool m_paused = false;
    int m_stepRequests = 0;
};
