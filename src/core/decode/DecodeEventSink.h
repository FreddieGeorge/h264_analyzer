#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QString>

#include <functional>

struct DecodeEventSink
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
