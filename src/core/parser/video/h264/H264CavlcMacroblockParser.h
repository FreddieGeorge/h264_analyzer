#pragma once

#include "core/parser/video/h264/H264SliceDataContext.h"

struct H264CavlcMacroblockType
{
    quint32 rawMbType = 0;
    int localMbType = 0;
    bool intra = false;
    bool intra16x16 = false;
    bool iPcm = false;
    bool p8x8 = false;
};

H264MacroblockParseAction h264ParseCavlcMacroblock(H264SliceDataContext &context);
