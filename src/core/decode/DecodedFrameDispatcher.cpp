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
                                const DecodeEventSink &eventSink)
{
    if (!emitThisFrame) {
        return;
    }

    if (frame) {
        invoke(eventSink.frameDecoded, frame);
        invoke(eventSink.frameReady, frameIndex, frame, analysis);
    }

    if (analysis.hasFrame) {
        invoke(eventSink.frameAnalysisDecoded, analysis);
        invoke(eventSink.accessUnitAnalysisDecoded, analysis);
    }
}
