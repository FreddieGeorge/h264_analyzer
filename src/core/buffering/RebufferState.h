#pragma once

#include <algorithm>

class RebufferState
{
public:
    struct StartResult
    {
        bool canceledPrevious = false;
        int canceledTargetFrameIndex = -1;
    };

    struct Progress
    {
        int bufferedFrames = 0;
        int totalFrames = 1;
        int percent = 0;
    };

    StartResult start(int targetFrameIndex, int decoderGeneration)
    {
        StartResult result;
        if (m_targetFrameIndex >= 0 && m_targetFrameIndex != targetFrameIndex) {
            result.canceledPrevious = true;
            result.canceledTargetFrameIndex = m_targetFrameIndex;
        }

        m_targetFrameIndex = targetFrameIndex;
        m_decoderGeneration = decoderGeneration;
        return result;
    }

    void reset()
    {
        m_targetFrameIndex = -1;
        m_decoderGeneration = -1;
    }

    bool hasPending() const
    {
        return m_targetFrameIndex >= 0;
    }

    int targetFrameIndex() const
    {
        return m_targetFrameIndex;
    }

    bool accepts(int decoderGeneration, int targetFrameIndex) const
    {
        return m_targetFrameIndex >= 0
            && m_decoderGeneration == decoderGeneration
            && m_targetFrameIndex == targetFrameIndex;
    }

    bool complete(int decoderGeneration, int frameIndex)
    {
        if (!accepts(decoderGeneration, frameIndex)) {
            return false;
        }

        reset();
        return true;
    }

    static Progress progress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex)
    {
        Progress result;
        result.totalFrames = std::max(1, targetFrameIndex - startFrameIndex);
        result.bufferedFrames = std::clamp(currentFrameIndex - startFrameIndex + 1,
                                           0,
                                           result.totalFrames);
        result.percent = (result.bufferedFrames * 100) / result.totalFrames;
        return result;
    }

private:
    int m_targetFrameIndex = -1;
    int m_decoderGeneration = -1;
};
