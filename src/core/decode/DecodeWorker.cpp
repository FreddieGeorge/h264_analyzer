#include "core/decode/DecodeWorker.h"

#include <QThread>

DecodeWorker::DecodeWorker(QObject *parent)
    : QObject(parent)
{
}

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
    m_stopRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = false;
        m_stepRequests = 0;
    }

    FFmpegDecoder decoder;
    if (!decoder.openFile(filePath)) {
        emit errorOccurred(decoder.lastError());
        emit finished();
        return;
    }

    const StreamInfo streamInfo = decoder.getStreamInfo();
    emit streamOpened(streamInfo);
    emit logMessage(tr("[Info] Video stream: %1x%2, %3 fps, codec=%4, pix_fmt=%5")
                        .arg(streamInfo.width)
                        .arg(streamInfo.height)
                        .arg(streamInfo.frameRate, 0, 'f', 3)
                        .arg(streamInfo.codecName, streamInfo.pixelFormatName));
    emit logMessage(tr("[Info] Container streams discovered: %1").arg(streamInfo.streams.size()));
    for (const MediaStreamInfo &mediaStream : streamInfo.streams) {
        QString details;
        if (mediaStream.mediaKind == MediaKind::Video) {
            details = tr("%1x%2, %3 fps, pix_fmt=%4")
                          .arg(mediaStream.width)
                          .arg(mediaStream.height)
                          .arg(mediaStream.frameRate, 0, 'f', 3)
                          .arg(mediaStream.pixelFormatName.isEmpty() ? tr("unknown") : mediaStream.pixelFormatName);
        } else if (mediaStream.mediaKind == MediaKind::Audio) {
            details = tr("%1 Hz, %2 ch, sample_fmt=%3, layout=%4")
                          .arg(mediaStream.sampleRate)
                          .arg(mediaStream.channels)
                          .arg(mediaStream.sampleFormatName.isEmpty() ? tr("unknown") : mediaStream.sampleFormatName)
                          .arg(mediaStream.channelLayoutName.isEmpty() ? tr("unknown") : mediaStream.channelLayoutName);
        }
        emit logMessage(tr("[Info]   Stream #%1%2: %3, codec=%4%5")
                            .arg(mediaStream.streamIndex)
                            .arg(mediaStream.selected ? tr(" selected") : QString())
                            .arg(mediaKindName(mediaStream.mediaKind))
                            .arg(mediaStream.codecName.isEmpty() ? codecKindName(mediaStream.codecKind) : mediaStream.codecName)
                            .arg(details.isEmpty() ? QString() : tr(", %1").arg(details)));
    }

    const unsigned long frameDelayMs = streamInfo.frameRate > 0.0
        ? static_cast<unsigned long>(1000.0 / streamInfo.frameRate)
        : 0UL;

    int frameIndex = 0;
    if (checkpoint.frameIndex >= 0) {
        if (decoder.seekToCheckpoint(checkpoint)) {
            frameIndex = checkpoint.frameIndex;
            emit logMessage(tr("[Info] Seeking from checkpoint frame %1 toward frame %2.")
                                .arg(checkpoint.frameIndex)
                                .arg(targetFrameIndex));
        } else {
            const QString seekError = decoder.lastError();
            emit logMessage(tr("[Warning] Checkpoint seek failed (%1); decoding from frame 0.")
                                .arg(seekError.isEmpty() ? tr("unknown error") : seekError));
            decoder.close();
            if (!decoder.openFile(filePath)) {
                emit errorOccurred(decoder.lastError());
                emit finished();
                return;
            }
            emit streamOpened(decoder.getStreamInfo());
        }
    }

    const int rebufferStartFrameIndex = frameIndex;
    if (targetFrameIndex > rebufferStartFrameIndex) {
        emit bufferingProgress(rebufferStartFrameIndex, rebufferStartFrameIndex, targetFrameIndex);
    }

    bool firstEmittedFrame = true;
    auto emitPendingAccessUnits = [&decoder, this]() {
        const QVector<FrameAnalysis> analyses = decoder.takePendingAccessUnitAnalyses();
        for (const FrameAnalysis &analysis : analyses) {
            emit accessUnitAnalysisDecoded(analysis);
        }
    };
    while (!m_stopRequested.load()) {
        const bool emitThisFrame = frameIndex >= targetFrameIndex;
        if (emitThisFrame && !waitForPlaybackPermission()) {
            break;
        }

        AVFrame *frame = decoder.decodeNextFrame();
        emitPendingAccessUnits();
        if (frame == nullptr) {
            if (!decoder.lastError().isEmpty()) {
                emit errorOccurred(decoder.lastError());
            }
            break;
        }

        DecodedVideoFramePtr copy = FFmpegDecoder::copyFrame(frame);
        FrameAnalysis analysis = decoder.lastFrameAnalysis();
        analysis.frameIndex = frameIndex;
        analysis.streamIndex = streamInfo.streamIndex;
        analysis.mediaKind = streamInfo.mediaKind;
        analysis.accessUnitKind = AccessUnitKind::VideoFrame;
        analysis.pts = copy ? copy->pts : analysis.pts;
        if (analysis.codecKind == CodecKind::Unknown) {
            analysis.codecKind = streamInfo.codecKind;
            analysis.codecName = streamInfo.codecName.isEmpty()
                ? codecKindName(streamInfo.codecKind)
                : streamInfo.codecName;
            if (streamInfo.codecKind != CodecKind::H264 && streamInfo.codecKind != CodecKind::HEVC) {
                analysis.diagnostics.append({
                    QStringLiteral("frame"),
                    QStringLiteral("codec_analysis_unsupported"),
                    tr("Playback is available, but bitstream syntax analysis is not implemented for this codec yet."),
                    QStringLiteral("info")
                });
            }
        }
        FrameSeekCheckpoint seekCheckpoint = decoder.lastFrameSeekCheckpoint();
        seekCheckpoint.frameIndex = frameIndex;
        if ((seekCheckpoint.keyframe || seekCheckpoint.idr || frameIndex == 0)
            && (seekCheckpoint.packetPosition >= 0
                || seekCheckpoint.packetPts != AV_NOPTS_VALUE
                || seekCheckpoint.packetDts != AV_NOPTS_VALUE)) {
            emit seekCheckpointReady(seekCheckpoint);
        }
        if (copy && emitThisFrame) {
            emit frameDecoded(copy);
            emit frameReady(frameIndex, copy, analysis);
        }
        if (emitThisFrame && analysis.hasFrame) {
            emit frameAnalysisDecoded(analysis);
            emit accessUnitAnalysisDecoded(analysis);
        }

        if (!emitThisFrame && targetFrameIndex > rebufferStartFrameIndex) {
            const int bufferedFrames = frameIndex - rebufferStartFrameIndex + 1;
            const int totalBufferedFrames = targetFrameIndex - rebufferStartFrameIndex;
            if (bufferedFrames == 1
                || bufferedFrames == totalBufferedFrames
                || bufferedFrames % 10 == 0) {
                emit bufferingProgress(rebufferStartFrameIndex, frameIndex, targetFrameIndex);
            }
        }

        if (emitThisFrame && firstEmittedFrame) {
            firstEmittedFrame = false;
            if (pauseAfterFirstFrame) {
                setPaused(true);
            }
        }
        ++frameIndex;

        if (emitThisFrame && frameDelayMs > 0) {
            QThread::msleep(frameDelayMs);
        }
    }

    emit finished();
}

void DecodeWorker::play()
{
    setPaused(false);
}

void DecodeWorker::pause()
{
    setPaused(true);
}

void DecodeWorker::setPaused(bool paused)
{
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = paused;
    }
    m_controlChanged.notify_all();
}

void DecodeWorker::stepForward()
{
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_paused = true;
        ++m_stepRequests;
    }
    m_controlChanged.notify_all();
}

void DecodeWorker::stop()
{
    m_stopRequested.store(true);
    m_controlChanged.notify_all();
}

bool DecodeWorker::waitForPlaybackPermission()
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
