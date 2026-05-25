#include "core/decode/FirstFramePauseController.h"

FirstFramePauseController::FirstFramePauseController(bool pauseAfterFirstFrame)
    : m_pauseAfterFirstFrame(pauseAfterFirstFrame)
{
}

bool FirstFramePauseController::shouldPauseAfterFrame(bool emittedFrame)
{
    if (!emittedFrame || m_firstEmittedFrameSeen) {
        return false;
    }

    m_firstEmittedFrameSeen = true;
    return m_pauseAfterFirstFrame;
}
