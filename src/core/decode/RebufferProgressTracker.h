#pragma once

#include <optional>

class RebufferProgressTracker
{
public:
    struct Progress
    {
        int startFrameIndex = 0;
        int currentFrameIndex = 0;
        int targetFrameIndex = 0;
    };

    RebufferProgressTracker(int startFrameIndex, int targetFrameIndex);

    bool isActive() const;
    std::optional<Progress> initialProgress() const;
    std::optional<Progress> frameProgress(int currentFrameIndex) const;

private:
    int m_startFrameIndex = 0;
    int m_targetFrameIndex = 0;
};
