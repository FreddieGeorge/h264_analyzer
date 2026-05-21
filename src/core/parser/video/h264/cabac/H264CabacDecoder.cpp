#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

namespace
{
constexpr int CabacRangeLps[64][4] = {
    {128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216}, {123, 150, 178, 205},
    {116, 142, 169, 195}, {111, 135, 160, 185}, {105, 128, 152, 175}, {100, 122, 144, 166},
    {95, 116, 137, 158},  {90, 110, 130, 150},  {85, 104, 123, 142},  {81, 99, 117, 135},
    {77, 94, 111, 128},   {73, 89, 105, 122},   {69, 85, 100, 116},   {66, 80, 95, 110},
    {62, 76, 90, 104},    {59, 72, 86, 99},     {56, 69, 81, 94},     {53, 65, 77, 89},
    {51, 62, 73, 85},     {48, 59, 69, 80},     {46, 56, 66, 76},     {43, 53, 63, 72},
    {41, 50, 59, 69},     {39, 48, 56, 65},     {37, 45, 54, 62},     {35, 43, 51, 59},
    {33, 41, 48, 56},     {32, 39, 46, 53},     {30, 37, 43, 50},     {29, 35, 41, 48},
    {27, 33, 39, 45},     {26, 31, 37, 43},     {24, 30, 35, 41},     {23, 28, 33, 39},
    {22, 27, 32, 37},     {21, 26, 30, 35},     {20, 24, 29, 33},     {19, 23, 27, 31},
    {18, 22, 26, 30},     {17, 21, 25, 28},     {16, 20, 23, 27},     {15, 19, 22, 25},
    {14, 18, 21, 24},     {14, 17, 20, 23},     {13, 16, 19, 22},     {12, 15, 18, 21},
    {12, 14, 17, 20},     {11, 14, 16, 19},     {11, 13, 15, 18},     {10, 12, 15, 17},
    {10, 12, 14, 16},     {9, 11, 13, 15},      {9, 11, 12, 14},      {8, 10, 12, 14},
    {8, 9, 11, 13},       {7, 9, 11, 12},       {7, 9, 10, 12},       {7, 8, 10, 11},
    {6, 8, 9, 11},        {6, 7, 9, 10},        {6, 7, 8, 9},         {2, 2, 2, 2},
};

constexpr int CabacStateTransitionLps[64] = {
    0, 0, 1, 2, 2, 4, 4, 5,
    6, 7, 8, 9, 9, 11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18,
    19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29,
    29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36,
    36, 36, 37, 37, 37, 38, 38, 63,
};

int cabacStateTransitionMps(int stateIndex)
{
    return stateIndex == 63 ? 63 : stateIndex + 1;
}

bool isValidContext(const H264CabacContextModel &context)
{
    return context.stateIndex >= 0 && context.stateIndex < 64
           && (context.valueMps == 0 || context.valueMps == 1);
}
}

bool H264CabacDecoder::initialize(BitReader &reader)
{
    m_state = H264CabacDecoderState {};
    if (reader.bitsRemaining() < 9) {
        reader.readBits(9);
        return false;
    }

    m_state.codIRange = 510;
    m_state.codIOffset = static_cast<int>(reader.readBits(9));
    m_state.initialized = !reader.hasError();
    return m_state.initialized;
}

const H264CabacDecoderState &H264CabacDecoder::state() const
{
    return m_state;
}

bool H264CabacDecoder::decodeBin(BitReader &reader,
                                 H264CabacContextModel &context,
                                 int *binValue)
{
    if (!m_state.initialized || !isValidContext(context) || binValue == nullptr) {
        return false;
    }

    const int rangeIdx = (m_state.codIRange >> 6) & 0x03;
    const int rangeLps = CabacRangeLps[context.stateIndex][rangeIdx];
    m_state.codIRange -= rangeLps;

    if (m_state.codIOffset >= m_state.codIRange) {
        *binValue = 1 - context.valueMps;
        m_state.codIOffset -= m_state.codIRange;
        m_state.codIRange = rangeLps;
        if (context.stateIndex == 0) {
            context.valueMps = 1 - context.valueMps;
        }
        context.stateIndex = CabacStateTransitionLps[context.stateIndex];
    } else {
        *binValue = context.valueMps;
        context.stateIndex = cabacStateTransitionMps(context.stateIndex);
    }

    return renormalize(reader);
}

bool H264CabacDecoder::decodeBin(BitReader &reader,
                                 H264CabacContextModelSet &contexts,
                                 int ctxIdx,
                                 int *binValue)
{
    if (!contexts.isInitialized(ctxIdx)) {
        return false;
    }

    H264CabacContextModel context = contexts.model(ctxIdx);
    if (!decodeBin(reader, context, binValue)) {
        return false;
    }
    contexts.setModel(ctxIdx, context);
    return true;
}

bool H264CabacDecoder::decodeBypassBin(BitReader &reader, int *binValue)
{
    if (!m_state.initialized || binValue == nullptr) {
        return false;
    }
    if (reader.bitsRemaining() < 1) {
        reader.readBit();
        return false;
    }

    m_state.codIOffset = (m_state.codIOffset << 1) | (reader.readBit() ? 1 : 0);
    if (m_state.codIOffset >= m_state.codIRange) {
        m_state.codIOffset -= m_state.codIRange;
        *binValue = 1;
    } else {
        *binValue = 0;
    }
    return !reader.hasError();
}

bool H264CabacDecoder::decodeTerminate(BitReader &reader, int *binValue)
{
    if (!m_state.initialized || binValue == nullptr) {
        return false;
    }

    m_state.codIRange -= 2;
    if (m_state.codIOffset >= m_state.codIRange) {
        *binValue = 1;
        return true;
    }

    *binValue = 0;
    return renormalize(reader);
}

bool H264CabacDecoder::consumeAlignmentBits(BitReader &reader)
{
    while ((reader.bitOffset() % 8) != 0 && !reader.hasError()) {
        if (!reader.readBit()) {
            return false;
        }
    }
    return !reader.hasError();
}

H264CabacContextModel H264CabacDecoder::initializedContextModel(int m, int n, int sliceQpY)
{
    return H264CabacContextModelInitializer::initializedContextModel(m, n, sliceQpY);
}

bool H264CabacDecoder::renormalize(BitReader &reader)
{
    while (m_state.codIRange < 256 && !reader.hasError()) {
        if (reader.bitsRemaining() < 1) {
            reader.readBit();
            return false;
        }
        m_state.codIRange <<= 1;
        m_state.codIOffset = (m_state.codIOffset << 1) | (reader.readBit() ? 1 : 0);
    }
    return !reader.hasError();
}
