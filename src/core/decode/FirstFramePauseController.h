#pragma once

class FirstFramePauseController
{
public:
    explicit FirstFramePauseController(bool pauseAfterFirstFrame);

    bool shouldPauseAfterFrame(bool emittedFrame);

private:
    bool m_pauseAfterFirstFrame = false;
    bool m_firstEmittedFrameSeen = false;
};
