#pragma once

#include "core/parser/video/h264/H264CabacContextModel.h"
#include "core/util/BitReader.h"

#include <cstdint>

struct H264CabacDecoderState
{
    int codIRange = 510;
    int codIOffset = 0;
    bool initialized = false;
};

class H264CabacDecoder
{
public:
    bool initialize(BitReader &reader);

    const H264CabacDecoderState &state() const;

    bool decodeBin(BitReader &reader, H264CabacContextModel &context, int *binValue);
    bool decodeBin(BitReader &reader,
                   H264CabacContextModelSet &contexts,
                   int ctxIdx,
                   int *binValue);
    bool decodeBypassBin(BitReader &reader, int *binValue);
    bool decodeTerminate(BitReader &reader, int *binValue);

    static bool consumeAlignmentBits(BitReader &reader);
    static H264CabacContextModel initializedContextModel(int m, int n, int sliceQpY);

private:
    bool renormalize(BitReader &reader);

    H264CabacDecoderState m_state;
};
