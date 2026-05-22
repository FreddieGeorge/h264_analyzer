#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"

#include <QVector>

class AnalysisStore
{
public:
    struct CachedFrame
    {
        int index = -1;
        DecodedVideoFramePtr frame;
        FrameAnalysis analysis;
    };

    explicit AnalysisStore(int maxCachedFrames = 80);

    void resetForNewStream();
    void resetDecodedData(bool clearSeekCheckpoints);

    void addFrame(int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis);
    bool addAccessUnitAnalysis(const FrameAnalysis &analysis);
    void addSeekCheckpoint(const FrameSeekCheckpoint &checkpoint);

    const CachedFrame *cachedFrame(int frameIndex) const;
    const CachedFrame *currentCachedFrame() const;
    void setCurrentFromCachedFrame(const CachedFrame &cached);
    void setCurrentAnalysis(const FrameAnalysis &analysis);

    FrameSeekCheckpoint bestSeekCheckpointForFrame(int frameIndex) const;

    const QVector<FrameAnalysis> &accessUnitAnalyses() const;
    const FrameAnalysis &currentAnalysis() const;
    bool hasCurrentAnalysis() const;
    bool hasDecodedSyntax() const;
    int currentFrameIndex() const;
    int latestFrameIndex() const;

private:
    int m_maxCachedFrames = 80;
    QVector<CachedFrame> m_frameCache;
    QVector<FrameAnalysis> m_frameAnalysisByIndex;
    QVector<FrameAnalysis> m_accessUnitAnalyses;
    QVector<FrameSeekCheckpoint> m_seekCheckpoints;
    FrameAnalysis m_currentAnalysis;
    int m_currentFrameIndex = -1;
    int m_latestFrameIndex = -1;
    bool m_hasCurrentAnalysis = false;
};
