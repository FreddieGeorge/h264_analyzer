#include "core/decode/RebufferProgressTracker.h"

RebufferProgressTracker::RebufferProgressTracker(int startFrameIndex, int targetFrameIndex)
    : m_startFrameIndex(startFrameIndex)
    , m_targetFrameIndex(targetFrameIndex)
{
}

bool RebufferProgressTracker::isActive() const
{
    return m_targetFrameIndex > m_startFrameIndex;
}

std::optional<RebufferProgressTracker::Progress> RebufferProgressTracker::initialProgress() const
{
    if (!isActive()) {
        return std::nullopt;
    }

    return Progress {m_startFrameIndex, m_startFrameIndex, m_targetFrameIndex};
}

std::optional<RebufferProgressTracker::Progress> RebufferProgressTracker::frameProgress(int currentFrameIndex) const
{
    if (!isActive()) {
        return std::nullopt;
    }

    const int bufferedFrames = currentFrameIndex - m_startFrameIndex + 1;
    const int totalBufferedFrames = m_targetFrameIndex - m_startFrameIndex;
    if (bufferedFrames == 1
        || bufferedFrames == totalBufferedFrames
        || bufferedFrames % 10 == 0) {
        return Progress {m_startFrameIndex, currentFrameIndex, m_targetFrameIndex};
    }

    return std::nullopt;
}
