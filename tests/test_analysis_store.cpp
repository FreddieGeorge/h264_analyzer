#include "app/AnalysisStore.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

FrameAnalysis makeAnalysis(int frameIndex,
                           int streamIndex = 0,
                           MediaKind mediaKind = MediaKind::Video,
                           AccessUnitKind accessUnitKind = AccessUnitKind::VideoFrame,
                           const QString &frameType = QStringLiteral("P"))
{
    FrameAnalysis analysis;
    analysis.frameIndex = frameIndex;
    analysis.streamIndex = streamIndex;
    analysis.mediaKind = mediaKind;
    analysis.accessUnitKind = accessUnitKind;
    analysis.frameType = frameType;
    analysis.hasFrame = accessUnitKind == AccessUnitKind::VideoFrame;
    return analysis;
}

void frameCacheHonorsLimitAndTracksCurrentFrame()
{
    AnalysisStore store(2);
    store.addFrame(0, std::make_shared<DecodedVideoFrame>(), makeAnalysis(0, 0, MediaKind::Video, AccessUnitKind::VideoFrame, QStringLiteral("I")));
    store.addFrame(1, std::make_shared<DecodedVideoFrame>(), makeAnalysis(1, 0, MediaKind::Video, AccessUnitKind::VideoFrame, QStringLiteral("P")));
    store.addFrame(2, std::make_shared<DecodedVideoFrame>(), makeAnalysis(2, 0, MediaKind::Video, AccessUnitKind::VideoFrame, QStringLiteral("B")));

    require(store.cachedFrame(0) == nullptr, "oldest cached frame is evicted");
    require(store.cachedFrame(1) != nullptr, "middle cached frame remains");
    require(store.cachedFrame(2) != nullptr, "newest cached frame remains");
    require(store.latestFrameIndex() == 2, "latest frame index is tracked");

    store.setCurrentFromCachedFrame(*store.cachedFrame(2));
    require(store.currentFrameIndex() == 2, "current frame index is set from cache");
    require(store.hasCurrentAnalysis(), "current analysis is available");
    require(store.currentAnalysis().frameType == QStringLiteral("B"), "current analysis is copied from cache");
    require(store.currentCachedFrame() == store.cachedFrame(2), "current cached frame lookup");
}

void accessUnitAnalysesAreValidatedAndReplaced()
{
    AnalysisStore store;
    require(!store.addAccessUnitAnalysis(makeAnalysis(-1)), "negative frame index analysis is rejected");
    require(!store.hasDecodedSyntax(), "rejected analysis does not populate decoded syntax");

    FrameAnalysis oldAudio = makeAnalysis(4,
                                          1,
                                          MediaKind::Audio,
                                          AccessUnitKind::AudioFrame,
                                          QStringLiteral("old"));
    FrameAnalysis newAudio = oldAudio;
    newAudio.frameType = QStringLiteral("new");

    require(store.addAccessUnitAnalysis(oldAudio), "first audio analysis accepted");
    require(store.addAccessUnitAnalysis(newAudio), "replacement audio analysis accepted");
    require(store.accessUnitAnalyses().size() == 1, "matching access unit replaces existing analysis");
    require(store.accessUnitAnalyses().first().frameType == QStringLiteral("new"), "replacement analysis value");

    FrameAnalysis secondStream = newAudio;
    secondStream.streamIndex = 2;
    require(store.addAccessUnitAnalysis(secondStream), "different stream analysis accepted");
    require(store.accessUnitAnalyses().size() == 2, "different stream does not replace existing analysis");
}

void checkpointLookupUsesBestPriorKeyframeOrIdr()
{
    AnalysisStore store;

    FrameSeekCheckpoint nonKeyframe;
    nonKeyframe.frameIndex = 5;
    store.addSeekCheckpoint(nonKeyframe);

    FrameSeekCheckpoint first;
    first.frameIndex = 0;
    first.keyframe = true;
    store.addSeekCheckpoint(first);

    FrameSeekCheckpoint keyframe;
    keyframe.frameIndex = 10;
    keyframe.keyframe = true;
    store.addSeekCheckpoint(keyframe);

    FrameSeekCheckpoint idr;
    idr.frameIndex = 20;
    idr.idr = true;
    store.addSeekCheckpoint(idr);

    require(store.bestSeekCheckpointForFrame(4).frameIndex == 0, "checkpoint before first keyframe target");
    require(store.bestSeekCheckpointForFrame(12).frameIndex == 10, "best prior keyframe selected");
    require(store.bestSeekCheckpointForFrame(30).frameIndex == 20, "IDR checkpoint is eligible");
}

void resetCanPreserveOrClearCheckpoints()
{
    AnalysisStore store;

    FrameSeekCheckpoint checkpoint;
    checkpoint.frameIndex = 7;
    checkpoint.keyframe = true;
    store.addSeekCheckpoint(checkpoint);
    store.addAccessUnitAnalysis(makeAnalysis(7));

    store.resetDecodedData(false);
    require(!store.hasDecodedSyntax(), "decoded analyses are reset");
    require(store.bestSeekCheckpointForFrame(10).frameIndex == 7, "checkpoint is preserved for replay reset");

    store.resetForNewStream();
    require(store.bestSeekCheckpointForFrame(10).frameIndex == -1, "checkpoint is cleared for new stream reset");
}
}

int main()
{
    frameCacheHonorsLimitAndTracksCurrentFrame();
    accessUnitAnalysesAreValidatedAndReplaced();
    checkpointLookupUsesBestPriorKeyframeOrIdr();
    resetCanPreserveOrClearCheckpoints();

    std::cout << "Analysis store tests passed\n";
    return 0;
}
