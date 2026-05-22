#include "core/decode/DecodeLoop.h"

#include "core/decode/DecodedFrameAnalysisBuilder.h"
#include "core/decode/DecodedFrameDispatcher.h"
#include "core/decode/DecodeSeekPlanner.h"
#include "core/decode/FFmpegDecoder.h"
#include "core/decode/FramePacing.h"
#include "core/decode/RebufferProgressTracker.h"
#include "core/decode/SeekCheckpointEmitter.h"
#include "core/decode/StreamLogFormatter.h"

#include <utility>

namespace
{
template <typename Callback, typename... Args>
void invoke(const Callback &callback, Args &&...args)
{
    if (callback) {
        callback(std::forward<Args>(args)...);
    }
}

void emitStreamLogMessages(const StreamInfo &streamInfo, const DecodeLoop::Callbacks &callbacks)
{
    for (const QString &message : streamOpenedLogMessages(streamInfo)) {
        invoke(callbacks.logMessage, message);
    }
}

void emitPendingAccessUnits(FFmpegDecoder &decoder, const DecodeLoop::Callbacks &callbacks)
{
    const QVector<FrameAnalysis> analyses = decoder.takePendingAccessUnitAnalyses();
    for (const FrameAnalysis &analysis : analyses) {
        invoke(callbacks.accessUnitAnalysisDecoded, analysis);
    }
}

void emitRebufferProgress(const RebufferProgressTracker::Progress &progress,
                          const DecodeLoop::Callbacks &callbacks)
{
    invoke(callbacks.bufferingProgress,
           progress.startFrameIndex,
           progress.currentFrameIndex,
           progress.targetFrameIndex);
}
}

void DecodeLoop::run(const Request &request, const Callbacks &callbacks)
{
    resetControlsForRun();

    FFmpegDecoder decoder;
    if (!decoder.openFile(request.filePath)) {
        invoke(callbacks.errorOccurred, decoder.lastError());
        return;
    }

    const StreamInfo streamInfo = decoder.getStreamInfo();
    invoke(callbacks.streamOpened, streamInfo);
    emitStreamLogMessages(streamInfo, callbacks);

    const FramePacing framePacing(streamInfo.frameRate);

    const DecodeStartPosition startPosition = prepareDecodeStart(decoder,
                                                                 request.filePath,
                                                                 request.targetFrameIndex,
                                                                 request.checkpoint);
    for (const QString &message : startPosition.logMessages) {
        invoke(callbacks.logMessage, message);
    }
    if (!startPosition.ok) {
        invoke(callbacks.errorOccurred, startPosition.errorMessage);
        return;
    }
    if (startPosition.reopenedStream) {
        invoke(callbacks.streamOpened, startPosition.reopenedStreamInfo);
    }

    int frameIndex = startPosition.frameIndex;
    const RebufferProgressTracker rebufferProgress(frameIndex, request.targetFrameIndex);
    if (const std::optional<RebufferProgressTracker::Progress> progress = rebufferProgress.initialProgress()) {
        emitRebufferProgress(*progress, callbacks);
    }

    bool firstEmittedFrame = true;
    while (!m_stopRequested.load()) {
        const bool emitThisFrame = frameIndex >= request.targetFrameIndex;
        if (emitThisFrame && !waitForPlaybackPermission()) {
            break;
        }

        AVFrame *frame = decoder.decodeNextFrame();
        emitPendingAccessUnits(decoder, callbacks);
        if (frame == nullptr) {
            if (!decoder.lastError().isEmpty()) {
                invoke(callbacks.errorOccurred, decoder.lastError());
            }
            break;
        }

        DecodedVideoFramePtr copy = FFmpegDecoder::copyFrame(frame);
        const FrameAnalysis analysis = buildDecodedFrameAnalysis(decoder.lastFrameAnalysis(),
                                                                 frameIndex,
                                                                 streamInfo,
                                                                 copy);

        if (const std::optional<FrameSeekCheckpoint> seekCheckpoint =
                seekCheckpointForDecodedFrame(decoder.lastFrameSeekCheckpoint(), frameIndex)) {
            invoke(callbacks.seekCheckpointReady, *seekCheckpoint);
        }
        dispatchDecodedFrameEvents(frameIndex, copy, analysis, emitThisFrame, callbacks);

        if (!emitThisFrame) {
            if (const std::optional<RebufferProgressTracker::Progress> progress =
                    rebufferProgress.frameProgress(frameIndex)) {
                emitRebufferProgress(*progress, callbacks);
            }
        }

        if (emitThisFrame && firstEmittedFrame) {
            firstEmittedFrame = false;
            if (request.pauseAfterFirstFrame) {
                setPaused(true);
            }
        }
        ++frameIndex;

        if (emitThisFrame) {
            framePacing.waitAfterEmittedFrame();
        }
    }
}

void DecodeLoop::play()
{
    setPaused(false);
}

void DecodeLoop::pause()
{
    setPaused(true);
}

void DecodeLoop::setPaused(bool paused)
{
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = paused;
    }
    m_controlChanged.notify_all();
}

void DecodeLoop::stepForward()
{
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = true;
        ++m_stepRequests;
    }
    m_controlChanged.notify_all();
}

void DecodeLoop::stop()
{
    m_stopRequested.store(true);
    m_controlChanged.notify_all();
}

void DecodeLoop::resetControlsForRun()
{
    m_stopRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = false;
        m_stepRequests = 0;
    }
    m_controlChanged.notify_all();
}

bool DecodeLoop::waitForPlaybackPermission()
{
    std::unique_lock<std::mutex> lock(m_controlMutex);
    m_controlChanged.wait(lock, [this]() {
        return m_stopRequested.load() || !m_paused || m_stepRequests > 0;
    });

    if (m_stopRequested.load()) {
        return false;
    }

    if (m_paused && m_stepRequests > 0) {
        --m_stepRequests;
    }

    return true;
}
