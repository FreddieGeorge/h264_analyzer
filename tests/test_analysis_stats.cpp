#include "core/analysis/AnalysisStats.h"

#include <QtGlobal>

#include <cmath>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        qFatal("%s", message);
    }
}
}

int main()
{
    FrameAnalysis video;
    video.frameIndex = 0;
    video.mediaKind = MediaKind::Video;
    video.accessUnitKind = AccessUnitKind::VideoFrame;
    video.frameType = QStringLiteral("P");
    video.hasFrame = true;

    AnalysisRegion parsedMb;
    parsedMb.kind = AnalysisRegionKind::Macroblock;
    parsedMb.parsed = true;
    parsedMb.qp = 22;
    video.regions.append(parsedMb);

    AnalysisRegion skippedMb;
    skippedMb.kind = AnalysisRegionKind::Macroblock;
    skippedMb.skipped = true;
    skippedMb.qp = 26;
    video.regions.append(skippedMb);

    AnalysisMotionVector mv;
    mv.mvXQuarterPel = 3;
    mv.mvYQuarterPel = 4;
    video.motionVectors.append(mv);

    AnalysisMotionVector l1Mv;
    l1Mv.list = 1;
    l1Mv.mvXQuarterPel = 0;
    l1Mv.mvYQuarterPel = 8;
    video.motionVectors.append(l1Mv);

    AnalysisDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("slice.short");
    diagnostic.severity = QStringLiteral("warning");
    video.diagnostics.append(diagnostic);

    FrameAnalysis secondVideo;
    secondVideo.frameIndex = 1;
    secondVideo.mediaKind = MediaKind::Video;
    secondVideo.accessUnitKind = AccessUnitKind::VideoFrame;
    secondVideo.frameType = QStringLiteral("IRAP");
    secondVideo.hasFrame = true;

    AnalysisRegion highQpMb;
    highQpMb.kind = AnalysisRegionKind::Macroblock;
    highQpMb.parsed = true;
    highQpMb.qp = 37;
    secondVideo.regions.append(highQpMb);

    AnalysisDiagnostic errorDiagnostic;
    errorDiagnostic.code = QStringLiteral("nalu.unsupported");
    errorDiagnostic.severity = QStringLiteral("error");
    secondVideo.diagnostics.append(errorDiagnostic);

    FrameAnalysis audio;
    audio.frameIndex = 2;
    audio.mediaKind = MediaKind::Audio;
    audio.accessUnitKind = AccessUnitKind::AudioFrame;

    const AnalysisStats stats = calculateAnalysisStats({video, secondVideo, audio});

    require(stats.totalAccessUnits == 3, "total access unit count mismatch");
    require(stats.videoAccessUnits == 2, "video access unit count mismatch");
    require(stats.audioAccessUnits == 1, "audio access unit count mismatch");
    require(stats.frameCount == 2, "frame count mismatch");
    require(stats.macroblockRegionCount == 3, "macroblock count mismatch");
    require(stats.parsedMacroblockRegionCount == 2, "parsed macroblock count mismatch");
    require(stats.skippedMacroblockRegionCount == 1, "skipped macroblock count mismatch");
    require(stats.qpValueCount == 3, "qp value count mismatch");
    require(stats.minQp == 22, "min qp mismatch");
    require(stats.maxQp == 37, "max qp mismatch");
    require(std::abs(stats.averageQp - 28.333) < 0.001, "average qp mismatch");
    require(stats.qpBuckets.size() == 3, "qp bucket count mismatch");
    require(stats.qpBuckets[0].minQp == 18 && stats.qpBuckets[0].maxQp == 23 && stats.qpBuckets[0].count == 1,
            "qp 18-23 bucket mismatch");
    require(stats.qpBuckets[1].minQp == 24 && stats.qpBuckets[1].maxQp == 29 && stats.qpBuckets[1].count == 1,
            "qp 24-29 bucket mismatch");
    require(stats.qpBuckets[2].minQp == 36 && stats.qpBuckets[2].maxQp == 41 && stats.qpBuckets[2].count == 1,
            "qp 36-41 bucket mismatch");
    require(stats.motionVectorCount == 2, "motion vector count mismatch");
    require(stats.l0MotionVectorCount == 1, "L0 motion vector count mismatch");
    require(stats.l1MotionVectorCount == 1, "L1 motion vector count mismatch");
    require(stats.otherMotionVectorCount == 0, "other motion vector count mismatch");
    require(std::abs(stats.averageMvMagnitudeQuarterPel - 6.5) < 0.001, "average mv magnitude mismatch");
    require(stats.maxMvMagnitudeQuarterPel == 8, "max mv magnitude mismatch");
    require(stats.diagnosticAccessUnits == 2, "diagnostic access unit count mismatch");
    require(stats.diagnosticCount == 2, "diagnostic count mismatch");
    require(stats.diagnostics.size() == 2, "diagnostic summary count mismatch");
    require(stats.diagnostics[0].code == QStringLiteral("nalu.unsupported"), "diagnostic error code mismatch");
    require(stats.diagnostics[1].code == QStringLiteral("slice.short"), "diagnostic warning code mismatch");
    require(stats.frameTypes.size() == 2, "frame type summary count mismatch");
    require(stats.frameTypes.first().type == QStringLiteral("IRAP"), "first frame type mismatch");
    require(stats.frameTypes.first().count == 1, "first frame type count mismatch");
    require(stats.frameTypes.last().type == QStringLiteral("P"), "last frame type mismatch");
    require(stats.frameTypes.last().count == 1, "last frame type count mismatch");

    return 0;
}
