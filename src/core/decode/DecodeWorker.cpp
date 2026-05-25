#include "core/decode/DecodeWorker.h"

#include "core/decode/DecodeLoop.h"

DecodeWorker::DecodeWorker(QObject *parent)
    : QObject(parent)
    , m_loop(std::make_unique<DecodeLoop>())
{
}

DecodeWorker::~DecodeWorker() = default;

void DecodeWorker::decodeFile(const QString &filePath)
{
    decodeFileFromFrame(filePath, 0, false);
}

void DecodeWorker::decodeFileFromFrame(const QString &filePath, int startFrameIndex, bool pauseAfterFirstFrame)
{
    decodeFileFromCheckpoint(filePath, startFrameIndex, pauseAfterFirstFrame, FrameSeekCheckpoint {});
}

void DecodeWorker::decodeFileFromCheckpoint(const QString &filePath,
                                            int targetFrameIndex,
                                            bool pauseAfterFirstFrame,
                                            const FrameSeekCheckpoint &checkpoint)
{
    DecodeLoop::Request request;
    request.filePath = filePath;
    request.targetFrameIndex = targetFrameIndex;
    request.pauseAfterFirstFrame = pauseAfterFirstFrame;
    request.checkpoint = checkpoint;

    DecodeEventSink eventSink;
    eventSink.streamOpened = [this](const StreamInfo &streamInfo) { emit streamOpened(streamInfo); };
    eventSink.frameDecoded = [this](const DecodedVideoFramePtr &frame) { emit frameDecoded(frame); };
    eventSink.frameAnalysisDecoded = [this](const FrameAnalysis &analysis) { emit frameAnalysisDecoded(analysis); };
    eventSink.accessUnitAnalysisDecoded = [this](const FrameAnalysis &analysis) { emit accessUnitAnalysisDecoded(analysis); };
    eventSink.seekCheckpointReady = [this](const FrameSeekCheckpoint &seekCheckpoint) {
        emit seekCheckpointReady(seekCheckpoint);
    };
    eventSink.bufferingProgress = [this](int startFrameIndex, int currentFrameIndex, int targetFrameIndex) {
        emit bufferingProgress(startFrameIndex, currentFrameIndex, targetFrameIndex);
    };
    eventSink.frameReady = [this](int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis) {
        emit frameReady(frameIndex, frame, analysis);
    };
    eventSink.logMessage = [this](const QString &message) { emit logMessage(message); };
    eventSink.errorOccurred = [this](const QString &message) { emit errorOccurred(message); };

    m_loop->run(request, eventSink);
    emit finished();
}

void DecodeWorker::play()
{
    m_loop->play();
}

void DecodeWorker::pause()
{
    m_loop->pause();
}

void DecodeWorker::setPaused(bool paused)
{
    m_loop->setPaused(paused);
}

void DecodeWorker::stepForward()
{
    m_loop->stepForward();
}

void DecodeWorker::stop()
{
    m_loop->stop();
}
