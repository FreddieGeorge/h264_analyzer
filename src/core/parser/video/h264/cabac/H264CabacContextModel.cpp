#include "core/parser/video/h264/cabac/H264CabacContextModel.h"

#include <algorithm>
#include <array>

namespace
{
struct InitValue
{
    int m = 0;
    int n = 0;
    bool valid = false;
};

constexpr int CabacInitTableCount = 4;
constexpr int CabacCoveredContextCount = 60;
constexpr int IntraInitTableIndex = 3;

constexpr InitValue initValue(int m, int n)
{
    return {m, n, true};
}

constexpr InitValue invalidInitValue()
{
    return {};
}

using InitRow = std::array<InitValue, CabacInitTableCount>;

constexpr std::array<InitRow, CabacCoveredContextCount> CabacInitValues {{
    InitRow {initValue(20, -15), initValue(20, -15), initValue(20, -15), initValue(20, -15)},
    InitRow {initValue(2, 54), initValue(2, 54), initValue(2, 54), initValue(2, 54)},
    InitRow {initValue(3, 74), initValue(3, 74), initValue(3, 74), initValue(3, 74)},
    InitRow {initValue(20, -15), initValue(20, -15), initValue(20, -15), initValue(20, -15)},
    InitRow {initValue(2, 54), initValue(2, 54), initValue(2, 54), initValue(2, 54)},
    InitRow {initValue(3, 74), initValue(3, 74), initValue(3, 74), initValue(3, 74)},
    InitRow {initValue(-28, 127), initValue(-28, 127), initValue(-28, 127), initValue(-28, 127)},
    InitRow {initValue(-23, 104), initValue(-23, 104), initValue(-23, 104), initValue(-23, 104)},
    InitRow {initValue(-6, 53), initValue(-6, 53), initValue(-6, 53), initValue(-6, 53)},
    InitRow {initValue(-1, 54), initValue(-1, 54), initValue(-1, 54), initValue(-1, 54)},
    InitRow {initValue(7, 51), initValue(7, 51), initValue(7, 51), initValue(7, 51)},
    InitRow {initValue(23, 33), initValue(22, 25), initValue(29, 16), invalidInitValue()},
    InitRow {initValue(23, 2), initValue(34, 0), initValue(25, 0), invalidInitValue()},
    InitRow {initValue(21, 0), initValue(16, 0), initValue(14, 0), invalidInitValue()},
    InitRow {initValue(1, 9), initValue(-2, 9), initValue(-10, 51), invalidInitValue()},
    InitRow {initValue(0, 49), initValue(4, 41), initValue(-3, 62), invalidInitValue()},
    InitRow {initValue(-37, 118), initValue(-29, 118), initValue(-27, 99), invalidInitValue()},
    InitRow {initValue(5, 57), initValue(2, 65), initValue(26, 16), invalidInitValue()},
    InitRow {initValue(-13, 78), initValue(-6, 71), initValue(-4, 85), invalidInitValue()},
    InitRow {initValue(-11, 65), initValue(-13, 79), initValue(-24, 102), invalidInitValue()},
    InitRow {initValue(1, 62), initValue(5, 52), initValue(5, 57), invalidInitValue()},
    InitRow {initValue(12, 49), initValue(9, 50), initValue(6, 57), invalidInitValue()},
    InitRow {initValue(-4, 73), initValue(-3, 70), initValue(-17, 73), invalidInitValue()},
    InitRow {initValue(17, 50), initValue(10, 54), initValue(14, 57), invalidInitValue()},
    InitRow {initValue(18, 64), initValue(26, 34), initValue(20, 40), invalidInitValue()},
    InitRow {initValue(9, 43), initValue(19, 22), initValue(20, 10), invalidInitValue()},
    InitRow {initValue(29, 0), initValue(40, 0), initValue(29, 0), invalidInitValue()},
    InitRow {initValue(26, 67), initValue(57, 2), initValue(54, 0), invalidInitValue()},
    InitRow {initValue(16, 90), initValue(41, 36), initValue(37, 42), invalidInitValue()},
    InitRow {initValue(9, 104), initValue(26, 69), initValue(12, 97), invalidInitValue()},
    InitRow {initValue(-46, 127), initValue(-45, 127), initValue(-32, 127), invalidInitValue()},
    InitRow {initValue(-20, 104), initValue(-15, 101), initValue(-22, 117), invalidInitValue()},
    InitRow {initValue(1, 67), initValue(-4, 76), initValue(-2, 74), invalidInitValue()},
    InitRow {initValue(-13, 78), initValue(-6, 71), initValue(-4, 85), invalidInitValue()},
    InitRow {initValue(-11, 65), initValue(-13, 79), initValue(-24, 102), invalidInitValue()},
    InitRow {initValue(1, 62), initValue(5, 52), initValue(5, 57), invalidInitValue()},
    InitRow {initValue(-6, 86), initValue(6, 69), initValue(-6, 93), invalidInitValue()},
    InitRow {initValue(-17, 95), initValue(-13, 90), initValue(-14, 88), invalidInitValue()},
    InitRow {initValue(-6, 61), initValue(0, 52), initValue(-6, 44), invalidInitValue()},
    InitRow {initValue(9, 45), initValue(8, 43), initValue(4, 55), invalidInitValue()},
    InitRow {initValue(-3, 69), initValue(-2, 69), initValue(-11, 89), invalidInitValue()},
    InitRow {initValue(-6, 81), initValue(-5, 82), initValue(-15, 103), invalidInitValue()},
    InitRow {initValue(-11, 96), initValue(-10, 96), initValue(-21, 116), invalidInitValue()},
    InitRow {initValue(6, 55), initValue(2, 59), initValue(19, 57), invalidInitValue()},
    InitRow {initValue(7, 67), initValue(2, 75), initValue(20, 58), invalidInitValue()},
    InitRow {initValue(-5, 86), initValue(-3, 87), initValue(4, 84), invalidInitValue()},
    InitRow {initValue(2, 88), initValue(-3, 100), initValue(6, 96), invalidInitValue()},
    InitRow {initValue(0, 58), initValue(1, 56), initValue(1, 63), invalidInitValue()},
    InitRow {initValue(-3, 76), initValue(-3, 74), initValue(-5, 85), invalidInitValue()},
    InitRow {initValue(-10, 94), initValue(-6, 85), initValue(-13, 106), invalidInitValue()},
    InitRow {initValue(5, 54), initValue(0, 59), initValue(5, 63), invalidInitValue()},
    InitRow {initValue(4, 69), initValue(-3, 81), initValue(6, 75), invalidInitValue()},
    InitRow {initValue(-3, 81), initValue(-7, 86), initValue(-3, 90), invalidInitValue()},
    InitRow {initValue(0, 88), initValue(-5, 95), initValue(-1, 101), invalidInitValue()},
    InitRow {initValue(-7, 67), initValue(-1, 66), initValue(3, 55), invalidInitValue()},
    InitRow {initValue(-5, 74), initValue(-1, 77), initValue(-4, 79), invalidInitValue()},
    InitRow {initValue(-4, 74), initValue(1, 70), initValue(-2, 75), invalidInitValue()},
    InitRow {initValue(-5, 80), initValue(-2, 86), initValue(-12, 97), invalidInitValue()},
    InitRow {initValue(-7, 72), initValue(-5, 72), initValue(-7, 50), invalidInitValue()},
    InitRow {initValue(1, 58), initValue(0, 61), initValue(1, 60), invalidInitValue()}
}};
}

H264CabacContextModelSet::H264CabacContextModelSet(int modelCount)
    : m_models(std::max(0, modelCount))
    , m_initialized(std::max(0, modelCount), false)
{
}

int H264CabacContextModelSet::size() const
{
    return m_models.size();
}

bool H264CabacContextModelSet::isInitialized(int ctxIdx) const
{
    return ctxIdx >= 0 && ctxIdx < m_initialized.size() && m_initialized[ctxIdx];
}

H264CabacContextModel H264CabacContextModelSet::model(int ctxIdx) const
{
    if (ctxIdx < 0 || ctxIdx >= m_models.size()) {
        return {};
    }
    return m_models[ctxIdx];
}

void H264CabacContextModelSet::setModel(int ctxIdx, const H264CabacContextModel &model)
{
    if (ctxIdx < 0 || ctxIdx >= m_models.size()) {
        return;
    }
    m_models[ctxIdx] = model;
    m_initialized[ctxIdx] = true;
}

H264CabacContextModel H264CabacContextModelInitializer::initializedContextModel(int m, int n, int sliceQpY)
{
    const int clippedQp = std::clamp(sliceQpY, 0, 51);
    const int preCtxState = std::clamp(((m * clippedQp) >> 4) + n, 1, 126);
    if (preCtxState <= 63) {
        return {63 - preCtxState, 0};
    }
    return {preCtxState - 64, 1};
}

H264CabacContextModelSet H264CabacContextModelInitializer::initializeSliceContexts(bool isIntraSlice,
                                                                                  int cabacInitIdc,
                                                                                  int sliceQpY,
                                                                                  int maxCtxIdx)
{
    if (maxCtxIdx < 0) {
        return H264CabacContextModelSet {};
    }

    H264CabacContextModelSet models(maxCtxIdx + 1);
    const int tableIndex = isIntraSlice ? IntraInitTableIndex : cabacInitIdc;
    if (!isIntraSlice && (cabacInitIdc < 0 || cabacInitIdc > 2)) {
        return models;
    }
    if (tableIndex < 0 || tableIndex >= CabacInitTableCount) {
        return models;
    }

    const int coveredMaxCtxIdx = std::min(maxCtxIdx, CabacCoveredContextCount - 1);
    for (int ctxIdx = 0; ctxIdx <= coveredMaxCtxIdx; ++ctxIdx) {
        const InitValue entry = CabacInitValues[ctxIdx][tableIndex];
        if (!entry.valid) {
            continue;
        }
        models.setModel(ctxIdx, initializedContextModel(entry.m, entry.n, sliceQpY));
    }
    return models;
}
