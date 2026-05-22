#include "core/decode/FramePacing.h"

#include <chrono>
#include <thread>

FramePacing::FramePacing(double frameRate)
    : m_delayMs(frameRate > 0.0 ? static_cast<unsigned long>(1000.0 / frameRate) : 0UL)
{
}

unsigned long FramePacing::delayMs() const
{
    return m_delayMs;
}

void FramePacing::waitAfterEmittedFrame() const
{
    if (m_delayMs == 0) {
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(m_delayMs));
}
