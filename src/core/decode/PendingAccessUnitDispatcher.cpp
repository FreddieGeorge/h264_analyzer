#include "core/decode/PendingAccessUnitDispatcher.h"

#include "core/decode/FFmpegDecoder.h"

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

void dispatchAccessUnitAnalyses(const QVector<FrameAnalysis> &analyses,
                                const DecodeEventSink &eventSink)
{
    for (const FrameAnalysis &analysis : analyses) {
        invoke(eventSink.accessUnitAnalysisDecoded, analysis);
    }
}

void dispatchPendingAccessUnits(FFmpegDecoder &decoder,
                                const DecodeEventSink &eventSink)
{
    dispatchAccessUnitAnalyses(decoder.takePendingAccessUnitAnalyses(), eventSink);
}
