#pragma once

#include "core/decode/DecodeEventSink.h"
#include "core/model/FrameSeekCheckpoint.h"

#include <QString>

#include <atomic>
#include <condition_variable>
#include <mutex>

class DecodeLoop
{
public:
    struct Request
    {
        QString filePath;
        int targetFrameIndex = 0;
        bool pauseAfterFirstFrame = false;
        FrameSeekCheckpoint checkpoint;
    };

    void run(const Request &request, const DecodeEventSink &eventSink);
    void play();
    void pause();
    void setPaused(bool paused);
    void stepForward();
    void stop();

private:
    void resetControlsForRun();
    bool waitForPlaybackPermission();

    std::atomic_bool m_stopRequested = false;
    std::mutex m_controlMutex;
    std::condition_variable m_controlChanged;
    bool m_paused = false;
    int m_stepRequests = 0;
};
