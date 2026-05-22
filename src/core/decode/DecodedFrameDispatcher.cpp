#include "core/decode/DecodedFrameDispatcher.h"

#include <utility>

namespace
{
template <typename Callback, typename... Args>
void invoke(const Callback &callback, Args &&...args)
{
    if (callback) {
        callback(std::forward<Args>(args)...);
    }
}
}

void dispatchDecodedFrameEvents(int frameIndex,
                                const DecodedVideoFramePtr &frame,
                                const FrameAnalysis &analysis,
                                bool emitThisFrame,
                                const DecodeLoop::Callbacks &callbacks)
{
    if (!emitThisFrame) {
        return;
    }

    if (frame) {
        invoke(callbacks.frameDecoded, frame);
        invoke(callbacks.frameReady, frameIndex, frame, analysis);
    }

    if (analysis.hasFrame) {
        invoke(callbacks.frameAnalysisDecoded, analysis);
        invoke(callbacks.accessUnitAnalysisDecoded, analysis);
    }
}
