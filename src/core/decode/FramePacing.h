#pragma once

class FramePacing
{
public:
    explicit FramePacing(double frameRate);

    unsigned long delayMs() const;
    void waitAfterEmittedFrame() const;

private:
    unsigned long m_delayMs = 0;
};
