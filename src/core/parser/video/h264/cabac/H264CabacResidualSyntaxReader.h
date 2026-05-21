#pragma once

#include "core/parser/video/h264/cabac/H264CabacContextModel.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxTypes.h"

class BitReader;
class H264CabacDecoder;

enum class H264CabacResidualBlockCategory
{
    Luma4x4
};

H264CabacResidualBlockResult h264ReadCabacResidualCodedBlockFlagZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    H264CabacResidualBlockCategory category);
