#include "core/decode/DecodedFrameAnalysisBuilder.h"
#include "core/decode/DecodedFrameDispatcher.h"
#include "core/decode/DecodeEventSink.h"
#include "core/decode/FirstFramePauseController.h"
#include "core/decode/FramePacing.h"
#include "core/decode/PendingAccessUnitDispatcher.h"
#include "core/decode/RebufferProgressTracker.h"
#include "core/decode/SeekCheckpointEmitter.h"
#include "core/decode/StreamLogFormatter.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

StreamInfo makeStreamInfo()
{
    StreamInfo stream;
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecKind = CodecKind::H264;
    stream.codecName = QStringLiteral("h264");
    stream.pixelFormatName = QStringLiteral("yuv420p");
    stream.width = 1920;
    stream.height = 1080;
    stream.frameRate = 29.970;

    MediaStreamInfo video;
    video.streamIndex = 0;
    video.mediaKind = MediaKind::Video;
    video.codecKind = CodecKind::H264;
    video.codecName = QStringLiteral("h264");
    video.pixelFormatName = QStringLiteral("yuv420p");
    video.width = 1920;
    video.height = 1080;
    video.frameRate = 29.970;
    video.selected = true;
    stream.streams.append(video);

    MediaStreamInfo audio;
    audio.streamIndex = 1;
    audio.mediaKind = MediaKind::Audio;
    audio.codecKind = CodecKind::AAC;
    audio.codecName = QStringLiteral("aac");
    audio.sampleFormatName = QStringLiteral("fltp");
    audio.channelLayoutName = QStringLiteral("stereo");
    audio.sampleRate = 48000;
    audio.channels = 2;
    stream.streams.append(audio);

    return stream;
}

void streamLogFormatterBuildsSummaryAndPerStreamLines()
{
    const QVector<QString> messages = streamOpenedLogMessages(makeStreamInfo());

    require(messages.size() == 4, "stream log message count");
    require(messages[0].contains(QStringLiteral("Video stream: 1920x1080")), "video summary dimensions");
    require(messages[0].contains(QStringLiteral("29.970 fps")), "video summary frame rate");
    require(messages[1] == QStringLiteral("[Info] Container streams discovered: 2"), "container stream count");
    require(messages[2].contains(QStringLiteral("Stream #0 selected")), "selected video stream marker");
    require(messages[2].contains(QStringLiteral("1920x1080")), "video stream details");
    require(messages[3].contains(QStringLiteral("Stream #1")), "audio stream index");
    require(messages[3].contains(QStringLiteral("48000 Hz, 2 ch")), "audio stream details");
}

void checkpointMessagesIncludeContext()
{
    require(checkpointSeekLogMessage(10, 25).contains(QStringLiteral("checkpoint frame 10")),
            "checkpoint seek message source frame");
    require(checkpointSeekLogMessage(10, 25).contains(QStringLiteral("frame 25")),
            "checkpoint seek message target frame");
    require(checkpointSeekFailedLogMessage(QString()).contains(QStringLiteral("unknown error")),
            "checkpoint failure empty error text");
    require(checkpointSeekFailedLogMessage(QStringLiteral("bad seek")).contains(QStringLiteral("bad seek")),
            "checkpoint failure explicit error text");
}

void decodedFrameAnalysisBuilderCompletesUnknownCodecAnalysis()
{
    StreamInfo stream;
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 3;
    stream.codecKind = CodecKind::VP9;
    stream.codecName = QStringLiteral("vp9");

    FrameAnalysis decoderAnalysis;
    decoderAnalysis.hasFrame = true;
    auto frame = std::make_shared<DecodedVideoFrame>();
    frame->pts = 12345;

    const FrameAnalysis analysis = buildDecodedFrameAnalysis(decoderAnalysis, 7, stream, frame);

    require(analysis.frameIndex == 7, "analysis frame index");
    require(analysis.streamIndex == 3, "analysis stream index");
    require(analysis.mediaKind == MediaKind::Video, "analysis media kind");
    require(analysis.accessUnitKind == AccessUnitKind::VideoFrame, "analysis access unit kind");
    require(analysis.pts == 12345, "analysis pts from decoded frame");
    require(analysis.codecKind == CodecKind::VP9, "analysis codec kind from stream");
    require(analysis.codecName == QStringLiteral("vp9"), "analysis codec name from stream");
    require(analysis.diagnostics.size() == 1, "unsupported codec diagnostic added");
    require(analysis.diagnostics.first().code == QStringLiteral("codec_analysis_unsupported"),
            "unsupported codec diagnostic code");
}

void decodedFrameAnalysisBuilderKeepsParsedCodecAnalysis()
{
    StreamInfo stream;
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 1;
    stream.codecKind = CodecKind::VP9;
    stream.codecName = QStringLiteral("vp9");

    FrameAnalysis decoderAnalysis;
    decoderAnalysis.codecKind = CodecKind::H264;
    decoderAnalysis.codecName = QStringLiteral("h264");

    const FrameAnalysis analysis = buildDecodedFrameAnalysis(decoderAnalysis, 2, stream, {});

    require(analysis.codecKind == CodecKind::H264, "parsed codec kind is preserved");
    require(analysis.codecName == QStringLiteral("h264"), "parsed codec name is preserved");
    require(analysis.diagnostics.isEmpty(), "no unsupported diagnostic for parsed analysis");
}

void rebufferProgressTrackerReportsInitialFirstTenthAndFinalFrames()
{
    const RebufferProgressTracker tracker(5, 18);

    const std::optional<RebufferProgressTracker::Progress> initial = tracker.initialProgress();
    require(initial.has_value(), "rebuffer initial progress exists");
    require(initial->startFrameIndex == 5, "rebuffer initial start");
    require(initial->currentFrameIndex == 5, "rebuffer initial current");
    require(initial->targetFrameIndex == 18, "rebuffer initial target");

    require(tracker.frameProgress(5).has_value(), "rebuffer first decoded frame progress");
    require(!tracker.frameProgress(6).has_value(), "rebuffer skips second decoded frame");
    require(tracker.frameProgress(14).has_value(), "rebuffer tenth decoded frame progress");
    require(tracker.frameProgress(17).has_value(), "rebuffer final buffered frame progress");
}

void rebufferProgressTrackerIsInactiveWhenTargetIsNotAhead()
{
    const RebufferProgressTracker sameFrame(5, 5);
    require(!sameFrame.isActive(), "same-frame rebuffer inactive");
    require(!sameFrame.initialProgress().has_value(), "same-frame initial progress absent");
    require(!sameFrame.frameProgress(5).has_value(), "same-frame frame progress absent");

    const RebufferProgressTracker earlierFrame(5, 4);
    require(!earlierFrame.isActive(), "earlier-frame rebuffer inactive");
}

void seekCheckpointEmitterRequiresSyncPointAndSeekAnchor()
{
    FrameSeekCheckpoint checkpoint;
    checkpoint.keyframe = true;
    require(!seekCheckpointForDecodedFrame(checkpoint, 3).has_value(),
            "checkpoint without seek anchor is not emitted");

    checkpoint.packetPosition = 1000;
    checkpoint.keyframe = false;
    checkpoint.idr = false;
    require(!seekCheckpointForDecodedFrame(checkpoint, 3).has_value(),
            "non-sync checkpoint after frame zero is not emitted");

    const std::optional<FrameSeekCheckpoint> frameZero = seekCheckpointForDecodedFrame(checkpoint, 0);
    require(frameZero.has_value(), "frame zero checkpoint is emitted with anchor");
    require(frameZero->frameIndex == 0, "frame zero checkpoint gets frame index");

    checkpoint.keyframe = true;
    const std::optional<FrameSeekCheckpoint> keyframe = seekCheckpointForDecodedFrame(checkpoint, 7);
    require(keyframe.has_value(), "keyframe checkpoint is emitted with anchor");
    require(keyframe->frameIndex == 7, "keyframe checkpoint gets frame index");

    checkpoint.keyframe = false;
    checkpoint.idr = true;
    checkpoint.packetPosition = -1;
    checkpoint.packetPts = 42;
    const std::optional<FrameSeekCheckpoint> idr = seekCheckpointForDecodedFrame(checkpoint, 9);
    require(idr.has_value(), "IDR checkpoint can use PTS anchor");
    require(idr->packetPts == 42, "IDR checkpoint preserves PTS anchor");
}

void framePacingComputesDelayFromFrameRate()
{
    require(FramePacing(0.0).delayMs() == 0, "zero fps has no delay");
    require(FramePacing(-1.0).delayMs() == 0, "negative fps has no delay");
    require(FramePacing(25.0).delayMs() == 40, "25 fps delay");
    require(FramePacing(29.970).delayMs() == 33, "29.97 fps delay");
    require(FramePacing(60.0).delayMs() == 16, "60 fps delay truncates to milliseconds");
}

void decodedFrameDispatcherEmitsVisibleFrameAndAnalysisEvents()
{
    int frameDecodedCount = 0;
    int frameReadyCount = 0;
    int frameAnalysisCount = 0;
    int accessUnitAnalysisCount = 0;
    int readyFrameIndex = -1;

    DecodeEventSink eventSink;
    eventSink.frameDecoded = [&](const DecodedVideoFramePtr &) { ++frameDecodedCount; };
    eventSink.frameReady = [&](int frameIndex, const DecodedVideoFramePtr &, const FrameAnalysis &) {
        ++frameReadyCount;
        readyFrameIndex = frameIndex;
    };
    eventSink.frameAnalysisDecoded = [&](const FrameAnalysis &) { ++frameAnalysisCount; };
    eventSink.accessUnitAnalysisDecoded = [&](const FrameAnalysis &) { ++accessUnitAnalysisCount; };

    FrameAnalysis analysis;
    analysis.hasFrame = true;
    dispatchDecodedFrameEvents(4, std::make_shared<DecodedVideoFrame>(), analysis, true, eventSink);

    require(frameDecodedCount == 1, "visible frame emits frameDecoded");
    require(frameReadyCount == 1, "visible frame emits frameReady");
    require(readyFrameIndex == 4, "frameReady receives frame index");
    require(frameAnalysisCount == 1, "visible analyzed frame emits frameAnalysisDecoded");
    require(accessUnitAnalysisCount == 1, "visible analyzed frame emits accessUnitAnalysisDecoded");
}

void decodedFrameDispatcherSkipsHiddenFrames()
{
    int eventCount = 0;

    DecodeEventSink eventSink;
    eventSink.frameDecoded = [&](const DecodedVideoFramePtr &) { ++eventCount; };
    eventSink.frameReady = [&](int, const DecodedVideoFramePtr &, const FrameAnalysis &) { ++eventCount; };
    eventSink.frameAnalysisDecoded = [&](const FrameAnalysis &) { ++eventCount; };
    eventSink.accessUnitAnalysisDecoded = [&](const FrameAnalysis &) { ++eventCount; };

    FrameAnalysis analysis;
    analysis.hasFrame = true;
    dispatchDecodedFrameEvents(4, std::make_shared<DecodedVideoFrame>(), analysis, false, eventSink);

    require(eventCount == 0, "hidden frame emits no decoded frame events");
}

void decodedFrameDispatcherAllowsAnalysisWithoutFrameCopy()
{
    int frameDecodedCount = 0;
    int frameReadyCount = 0;
    int frameAnalysisCount = 0;
    int accessUnitAnalysisCount = 0;

    DecodeEventSink eventSink;
    eventSink.frameDecoded = [&](const DecodedVideoFramePtr &) { ++frameDecodedCount; };
    eventSink.frameReady = [&](int, const DecodedVideoFramePtr &, const FrameAnalysis &) { ++frameReadyCount; };
    eventSink.frameAnalysisDecoded = [&](const FrameAnalysis &) { ++frameAnalysisCount; };
    eventSink.accessUnitAnalysisDecoded = [&](const FrameAnalysis &) { ++accessUnitAnalysisCount; };

    FrameAnalysis analysis;
    analysis.hasFrame = true;
    dispatchDecodedFrameEvents(4, {}, analysis, true, eventSink);

    require(frameDecodedCount == 0, "missing frame copy does not emit frameDecoded");
    require(frameReadyCount == 0, "missing frame copy does not emit frameReady");
    require(frameAnalysisCount == 1, "analysis event still emits without frame copy");
    require(accessUnitAnalysisCount == 1, "access unit event still emits without frame copy");
}

void decodedFrameDispatcherSkipsAnalysisWhenFrameAnalysisIsAbsent()
{
    int frameDecodedCount = 0;
    int frameReadyCount = 0;
    int frameAnalysisCount = 0;
    int accessUnitAnalysisCount = 0;

    DecodeEventSink eventSink;
    eventSink.frameDecoded = [&](const DecodedVideoFramePtr &) { ++frameDecodedCount; };
    eventSink.frameReady = [&](int, const DecodedVideoFramePtr &, const FrameAnalysis &) { ++frameReadyCount; };
    eventSink.frameAnalysisDecoded = [&](const FrameAnalysis &) { ++frameAnalysisCount; };
    eventSink.accessUnitAnalysisDecoded = [&](const FrameAnalysis &) { ++accessUnitAnalysisCount; };

    FrameAnalysis analysis;
    analysis.hasFrame = false;
    dispatchDecodedFrameEvents(4, std::make_shared<DecodedVideoFrame>(), analysis, true, eventSink);

    require(frameDecodedCount == 1, "frame event emits without frame analysis");
    require(frameReadyCount == 1, "frameReady emits without frame analysis");
    require(frameAnalysisCount == 0, "missing frame analysis does not emit frameAnalysisDecoded");
    require(accessUnitAnalysisCount == 0, "missing frame analysis does not emit accessUnitAnalysisDecoded");
}

void pendingAccessUnitDispatcherEmitsEachAnalysis()
{
    int accessUnitAnalysisCount = 0;
    int lastFrameIndex = -1;

    DecodeEventSink eventSink;
    eventSink.accessUnitAnalysisDecoded = [&](const FrameAnalysis &analysis) {
        ++accessUnitAnalysisCount;
        lastFrameIndex = analysis.frameIndex;
    };

    FrameAnalysis first;
    first.frameIndex = 3;
    FrameAnalysis second;
    second.frameIndex = 9;
    dispatchAccessUnitAnalyses({first, second}, eventSink);

    require(accessUnitAnalysisCount == 2, "each pending access unit analysis is emitted");
    require(lastFrameIndex == 9, "pending access unit analyses preserve order");
}

void firstFramePauseControllerPausesOnlyOnFirstVisibleFrameWhenEnabled()
{
    FirstFramePauseController controller(true);

    require(!controller.shouldPauseAfterFrame(false), "hidden frame does not trigger pause");
    require(controller.shouldPauseAfterFrame(true), "first visible frame triggers pause");
    require(!controller.shouldPauseAfterFrame(true), "second visible frame does not trigger pause");
}

void firstFramePauseControllerNeverPausesWhenDisabled()
{
    FirstFramePauseController controller(false);

    require(!controller.shouldPauseAfterFrame(false), "hidden frame does not trigger pause when disabled");
    require(!controller.shouldPauseAfterFrame(true), "first visible frame does not pause when disabled");
    require(!controller.shouldPauseAfterFrame(true), "later visible frame does not pause when disabled");
}
}

int main()
{
    streamLogFormatterBuildsSummaryAndPerStreamLines();
    checkpointMessagesIncludeContext();
    decodedFrameAnalysisBuilderCompletesUnknownCodecAnalysis();
    decodedFrameAnalysisBuilderKeepsParsedCodecAnalysis();
    rebufferProgressTrackerReportsInitialFirstTenthAndFinalFrames();
    rebufferProgressTrackerIsInactiveWhenTargetIsNotAhead();
    seekCheckpointEmitterRequiresSyncPointAndSeekAnchor();
    framePacingComputesDelayFromFrameRate();
    decodedFrameDispatcherEmitsVisibleFrameAndAnalysisEvents();
    decodedFrameDispatcherSkipsHiddenFrames();
    decodedFrameDispatcherAllowsAnalysisWithoutFrameCopy();
    decodedFrameDispatcherSkipsAnalysisWhenFrameAnalysisIsAbsent();
    pendingAccessUnitDispatcherEmitsEachAnalysis();
    firstFramePauseControllerPausesOnlyOnFirstVisibleFrameWhenEnabled();
    firstFramePauseControllerNeverPausesWhenDisabled();

    std::cout << "Decode helper tests passed\n";
    return 0;
}
