#include "app/AnalysisStore.h"

#include <algorithm>

AnalysisStore::AnalysisStore(int maxCachedFrames)
    : m_maxCachedFrames(maxCachedFrames)
{
}

void AnalysisStore::resetForNewStream()
{
    resetDecodedData(true);
}

void AnalysisStore::resetDecodedData(bool clearSeekCheckpoints)
{
    m_frameCache.clear();
    m_frameAnalysisByIndex.clear();
    m_accessUnitAnalyses.clear();
    if (clearSeekCheckpoints) {
        m_seekCheckpoints.clear();
    }
    m_currentAnalysis = FrameAnalysis {};
    m_currentFrameIndex = -1;
    m_latestFrameIndex = -1;
    m_hasCurrentAnalysis = false;
}

void AnalysisStore::addFrame(int frameIndex,
                             const DecodedVideoFramePtr &frame,
                             const FrameAnalysis &analysis)
{
    CachedFrame cached;
    cached.index = frameIndex;
    cached.frame = frame;
    cached.analysis = analysis;
    m_frameCache.append(cached);
    while (m_frameCache.size() > m_maxCachedFrames) {
        m_frameCache.removeFirst();
    }

    m_latestFrameIndex = std::max(m_latestFrameIndex, frameIndex);
}

bool AnalysisStore::addAccessUnitAnalysis(const FrameAnalysis &analysis)
{
    if (analysis.frameIndex < 0) {
        return false;
    }

    if (analysis.accessUnitKind == AccessUnitKind::VideoFrame) {
        if (analysis.frameIndex >= m_frameAnalysisByIndex.size()) {
            m_frameAnalysisByIndex.resize(analysis.frameIndex + 1);
        }
        m_frameAnalysisByIndex[analysis.frameIndex] = analysis;
    }

    bool replaced = false;
    for (FrameAnalysis &existing : m_accessUnitAnalyses) {
        if (existing.frameIndex == analysis.frameIndex
            && existing.streamIndex == analysis.streamIndex
            && existing.mediaKind == analysis.mediaKind
            && existing.accessUnitKind == analysis.accessUnitKind) {
            existing = analysis;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_accessUnitAnalyses.append(analysis);
    }
    return true;
}

void AnalysisStore::addSeekCheckpoint(const FrameSeekCheckpoint &checkpoint)
{
    if (checkpoint.frameIndex < 0) {
        return;
    }

    for (FrameSeekCheckpoint &existing : m_seekCheckpoints) {
        if (existing.frameIndex == checkpoint.frameIndex) {
            existing = checkpoint;
            return;
        }
    }

    m_seekCheckpoints.append(checkpoint);
    std::sort(m_seekCheckpoints.begin(), m_seekCheckpoints.end(), [](const FrameSeekCheckpoint &a, const FrameSeekCheckpoint &b) {
        return a.frameIndex < b.frameIndex;
    });
}

const AnalysisStore::CachedFrame *AnalysisStore::cachedFrame(int frameIndex) const
{
    for (const CachedFrame &cached : m_frameCache) {
        if (cached.index == frameIndex) {
            return &cached;
        }
    }
    return nullptr;
}

const AnalysisStore::CachedFrame *AnalysisStore::currentCachedFrame() const
{
    return cachedFrame(m_currentFrameIndex);
}

void AnalysisStore::setCurrentFromCachedFrame(const CachedFrame &cached)
{
    m_currentFrameIndex = cached.index;
    m_currentAnalysis = cached.analysis;
    m_hasCurrentAnalysis = true;
}

void AnalysisStore::setCurrentAnalysis(const FrameAnalysis &analysis)
{
    m_currentAnalysis = analysis;
    m_currentFrameIndex = analysis.accessUnitKind == AccessUnitKind::VideoFrame
        ? analysis.frameIndex
        : m_currentFrameIndex;
    m_hasCurrentAnalysis = true;
}

FrameSeekCheckpoint AnalysisStore::bestSeekCheckpointForFrame(int frameIndex) const
{
    FrameSeekCheckpoint checkpoint;
    for (const FrameSeekCheckpoint &candidate : m_seekCheckpoints) {
        if (candidate.frameIndex <= frameIndex
            && candidate.frameIndex >= checkpoint.frameIndex
            && (candidate.keyframe || candidate.idr || candidate.frameIndex == 0)) {
            checkpoint = candidate;
        }
    }
    return checkpoint;
}

const QVector<FrameAnalysis> &AnalysisStore::accessUnitAnalyses() const
{
    return m_accessUnitAnalyses;
}

const FrameAnalysis &AnalysisStore::currentAnalysis() const
{
    return m_currentAnalysis;
}

bool AnalysisStore::hasCurrentAnalysis() const
{
    return m_hasCurrentAnalysis;
}

bool AnalysisStore::hasDecodedSyntax() const
{
    return !m_accessUnitAnalyses.isEmpty();
}

int AnalysisStore::currentFrameIndex() const
{
    return m_currentFrameIndex;
}

int AnalysisStore::latestFrameIndex() const
{
    return m_latestFrameIndex;
}
