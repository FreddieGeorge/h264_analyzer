#pragma once

#include "core/decode/DecodeLoop.h"

void dispatchDecodedFrameEvents(int frameIndex,
                                const DecodedVideoFramePtr &frame,
                                const FrameAnalysis &analysis,
                                bool emitThisFrame,
                                const DecodeLoop::Callbacks &callbacks);
