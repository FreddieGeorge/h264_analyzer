#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QObject>
#include <QString>

#include <memory>

class DecodeLoop;

class DecodeWorker : public QObject
{
    Q_OBJECT

public:
    explicit DecodeWorker(QObject *parent = nullptr);
    ~DecodeWorker() override;

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
    void accessUnitAnalysisDecoded(const FrameAnalysis &analysis);
    void seekCheckpointReady(const FrameSeekCheckpoint &checkpoint);
    void bufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex);
    void frameReady(int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void finished();

private:
    std::unique_ptr<DecodeLoop> m_loop;
};
