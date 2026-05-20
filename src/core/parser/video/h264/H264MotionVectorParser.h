#pragma once

#include "core/parser/video/h264/H264Parser.h"

#include <array>

struct H264MacroblockMvState
{
    bool valid = false;
    int refIndex = 0;
    int mvX = 0;
    int mvY = 0;
};

struct H264PartitionMv
{
    int refIndex = 0;
    int mvdX = 0;
    int mvdY = 0;
    int mvX = 0;
    int mvY = 0;
};

enum class H264PredictionList
{
    None,
    L0,
    L1,
    Bi
};

struct H264BPartitionModes
{
    int partitionCount = 1;
    std::array<H264PredictionList, 2> modes {H264PredictionList::None, H264PredictionList::None};
    bool supported = false;
    QString unsupportedCode;
    QString unsupportedMessage;
};

H264MacroblockMvState h264PredictMv(const QVector<H264MacroblockMvState> &states,
                                    int picWidthInMbs,
                                    int address,
                                    int refIndex);
void h264AddMotionVector(MacroblockInfo &mb, int list, int refIndex, int mvX, int mvY);
void h264SetMvState(const MacroblockInfo &mb,
                    QVector<H264MacroblockMvState> &mvStatesL0,
                    QVector<H264MacroblockMvState> &mvStatesL1);
H264BPartitionModes h264BPartitionModes(int mbType);
