#include "core/parser/video/h264/H264MotionVectorParser.h"

#include <algorithm>

namespace
{
int median(int a, int b, int c)
{
    return a + b + c - std::min({a, b, c}) - std::max({a, b, c});
}

H264MacroblockMvState neighborMv(const QVector<H264MacroblockMvState> &states, int address)
{
    if (address < 0 || address >= states.size()) {
        return H264MacroblockMvState {};
    }
    return states[address];
}
}

H264MacroblockMvState h264PredictMv(const QVector<H264MacroblockMvState> &states,
                                    int picWidthInMbs,
                                    int address,
                                    int refIndex)
{
    const int widthInMbs = std::max(1, picWidthInMbs);
    const int mbX = address % widthInMbs;
    const int leftAddress = mbX > 0 ? address - 1 : -1;
    const int topAddress = address >= widthInMbs ? address - widthInMbs : -1;
    const int topRightAddress = (topAddress >= 0 && mbX + 1 < widthInMbs)
        ? topAddress + 1
        : -1;
    const int topLeftAddress = (topAddress >= 0 && mbX > 0)
        ? topAddress - 1
        : -1;

    const H264MacroblockMvState a = neighborMv(states, leftAddress);
    const H264MacroblockMvState b = neighborMv(states, topAddress);
    H264MacroblockMvState c = neighborMv(states, topRightAddress);
    if (!c.valid) {
        c = neighborMv(states, topLeftAddress);
    }

    QVector<H264MacroblockMvState> matching;
    for (const H264MacroblockMvState &candidate : {a, b, c}) {
        if (candidate.valid && candidate.refIndex == refIndex) {
            matching.append(candidate);
        }
    }
    if (matching.size() == 1) {
        return matching.first();
    }

    H264MacroblockMvState result;
    result.valid = a.valid || b.valid || c.valid;
    result.refIndex = refIndex;
    result.mvX = median(a.valid ? a.mvX : 0, b.valid ? b.mvX : 0, c.valid ? c.mvX : 0);
    result.mvY = median(a.valid ? a.mvY : 0, b.valid ? b.mvY : 0, c.valid ? c.mvY : 0);
    return result;
}

void h264AddMotionVector(MacroblockInfo &mb, int list, int refIndex, int mvX, int mvY)
{
    MotionVectorInfo mv;
    mv.list = list;
    mv.referenceIndex = refIndex;
    mv.mvXQuarterPel = mvX;
    mv.mvYQuarterPel = mvY;
    mb.motionVectors.append(mv);
}

void h264SetMvState(const MacroblockInfo &mb,
                    QVector<H264MacroblockMvState> &mvStatesL0,
                    QVector<H264MacroblockMvState> &mvStatesL1)
{
    if (mb.address < 0 || mb.motionVectors.isEmpty()) {
        return;
    }
    for (const MotionVectorInfo &mv : mb.motionVectors) {
        QVector<H264MacroblockMvState> &states = mv.list == 1 ? mvStatesL1 : mvStatesL0;
        if (mb.address < states.size()) {
            states[mb.address] = {true, mv.referenceIndex, mv.mvXQuarterPel, mv.mvYQuarterPel};
        }
    }
}

H264BPartitionModes h264BPartitionModes(int mbType)
{
    H264BPartitionModes result;
    if (mbType == 0) {
        result.unsupportedCode = QStringLiteral("b_direct_macroblock_unsupported");
        result.unsupportedMessage = QStringLiteral("B_Direct motion vector derivation is not implemented.");
        return result;
    }
    if (mbType == 22) {
        result.unsupportedCode = QStringLiteral("b8x8_sub_macroblock_unsupported");
        result.unsupportedMessage = QStringLiteral("B_8x8 sub-macroblock motion vector parsing is not implemented.");
        return result;
    }
    if (mbType < 1 || mbType > 21) {
        result.unsupportedCode = QStringLiteral("b_slice_macroblock_unsupported");
        result.unsupportedMessage = QStringLiteral("Unsupported B-slice macroblock type %1.").arg(mbType);
        return result;
    }

    result.supported = true;
    if (mbType <= 3) {
        result.partitionCount = 1;
        result.modes[0] = mbType == 1 ? H264PredictionList::L0
            : (mbType == 2 ? H264PredictionList::L1 : H264PredictionList::Bi);
        return result;
    }

    result.partitionCount = 2;
    switch (mbType) {
    case 4:
    case 5:
        result.modes = {H264PredictionList::L0, H264PredictionList::L0};
        break;
    case 6:
    case 7:
        result.modes = {H264PredictionList::L1, H264PredictionList::L1};
        break;
    case 8:
    case 9:
        result.modes = {H264PredictionList::L0, H264PredictionList::L1};
        break;
    case 10:
    case 11:
        result.modes = {H264PredictionList::L1, H264PredictionList::L0};
        break;
    case 12:
    case 13:
        result.modes = {H264PredictionList::L0, H264PredictionList::Bi};
        break;
    case 14:
    case 15:
        result.modes = {H264PredictionList::L1, H264PredictionList::Bi};
        break;
    case 16:
    case 17:
        result.modes = {H264PredictionList::Bi, H264PredictionList::L0};
        break;
    case 18:
    case 19:
        result.modes = {H264PredictionList::Bi, H264PredictionList::L1};
        break;
    case 20:
    case 21:
        result.modes = {H264PredictionList::Bi, H264PredictionList::Bi};
        break;
    }
    return result;
}
