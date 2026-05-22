#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/StreamInfo.h"

FrameAnalysis buildDecodedFrameAnalysis(const FrameAnalysis &decoderAnalysis,
                                        int frameIndex,
                                        const StreamInfo &streamInfo,
                                        const DecodedVideoFramePtr &frame);
