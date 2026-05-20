#pragma once

#include "core/model/FrameAnalysis.h"
#include "core/parser/video/h264/H264Parser.h"

FrameAnalysis frameAnalysisFromH264Syntax(const FrameSyntaxInfo &syntaxInfo);
FrameSyntaxInfo h264SyntaxFromFrameAnalysis(const FrameAnalysis &analysis);
