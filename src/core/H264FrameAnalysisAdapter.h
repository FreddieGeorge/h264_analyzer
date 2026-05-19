#pragma once

#include "core/FrameAnalysis.h"
#include "core/H264Parser.h"

FrameAnalysis frameAnalysisFromH264Syntax(const FrameSyntaxInfo &syntaxInfo);
FrameSyntaxInfo h264SyntaxFromFrameAnalysis(const FrameAnalysis &analysis);
