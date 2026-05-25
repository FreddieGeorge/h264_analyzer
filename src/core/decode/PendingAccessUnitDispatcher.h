#pragma once

#include "core/decode/DecodeEventSink.h"
#include "core/model/FrameAnalysis.h"

#include <QVector>

class FFmpegDecoder;

void dispatchAccessUnitAnalyses(const QVector<FrameAnalysis> &analyses,
                                const DecodeEventSink &eventSink);

void dispatchPendingAccessUnits(FFmpegDecoder &decoder,
                                const DecodeEventSink &eventSink);
