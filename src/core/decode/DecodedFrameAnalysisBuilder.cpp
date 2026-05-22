#include "core/decode/DecodedFrameAnalysisBuilder.h"

#include <QCoreApplication>

namespace
{
QString trAnalysis(const char *text)
{
    return QCoreApplication::translate("DecodeWorker", text);
}
}

FrameAnalysis buildDecodedFrameAnalysis(const FrameAnalysis &decoderAnalysis,
                                        int frameIndex,
                                        const StreamInfo &streamInfo,
                                        const DecodedVideoFramePtr &frame)
{
    FrameAnalysis analysis = decoderAnalysis;
    analysis.frameIndex = frameIndex;
    analysis.streamIndex = streamInfo.streamIndex;
    analysis.mediaKind = streamInfo.mediaKind;
    analysis.accessUnitKind = AccessUnitKind::VideoFrame;
    analysis.pts = frame ? frame->pts : analysis.pts;

    if (analysis.codecKind != CodecKind::Unknown) {
        return analysis;
    }

    analysis.codecKind = streamInfo.codecKind;
    analysis.codecName = streamInfo.codecName.isEmpty()
        ? codecKindName(streamInfo.codecKind)
        : streamInfo.codecName;
    if (streamInfo.codecKind != CodecKind::H264 && streamInfo.codecKind != CodecKind::HEVC) {
        analysis.diagnostics.append({
            QStringLiteral("frame"),
            QStringLiteral("codec_analysis_unsupported"),
            trAnalysis("Playback is available, but bitstream syntax analysis is not implemented for this codec yet."),
            QStringLiteral("info")
        });
    }

    return analysis;
}
