#pragma once

#include "core/decode/DecodeEventSink.h"

void dispatchDecodedFrameEvents(int frameIndex,
                                const DecodedVideoFramePtr &frame,
                                const FrameAnalysis &analysis,
                                bool emitThisFrame,
                                const DecodeEventSink &eventSink);
