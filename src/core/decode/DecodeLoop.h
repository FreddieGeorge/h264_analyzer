#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QString>

#include <atomic>
#include <condition_variable>
#include <functional>
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

    struct Callbacks
    {
        std::function<void(const StreamInfo &)> streamOpened;
        std::function<void(const DecodedVideoFramePtr &)> frameDecoded;
        std::function<void(const FrameAnalysis &)> frameAnalysisDecoded;
        std::function<void(const FrameAnalysis &)> accessUnitAnalysisDecoded;
        std::function<void(const FrameSeekCheckpoint &)> seekCheckpointReady;
        std::function<void(int, int, int)> bufferingProgress;
        std::function<void(int, const DecodedVideoFramePtr &, const FrameAnalysis &)> frameReady;
        std::function<void(const QString &)> logMessage;
        std::function<void(const QString &)> errorOccurred;
    };

    void run(const Request &request, const Callbacks &callbacks);
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
