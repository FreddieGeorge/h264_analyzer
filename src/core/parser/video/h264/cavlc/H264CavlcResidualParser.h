#pragma once

#include "core/parser/video/h264/H264Parser.h"

#include <QVector>

struct H264CavlcResidualBlock
{
    int totalCoeff = 0;
    int trailingOnes = 0;
    int totalZeros = 0;
    QVector<ResidualBlockInfo::Coefficient> coefficients;
    bool valid = false;
};

H264CavlcResidualBlock parseCavlcResidualBlock(BitReader &reader, int nC, int maxNumCoeff);
