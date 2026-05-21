#include "core/parser/video/h264/cavlc/H264CavlcMacroblockResidualParser.h"

#include "core/parser/video/h264/cavlc/H264CavlcResidualParser.h"

#include <array>

namespace
{
int h264Luma4x4Index(int x, int y)
{
    const int i8x8 = (y / 2) * 2 + (x / 2);
    const int i4x4 = (y % 2) * 2 + (x % 2);
    return i8x8 * 4 + i4x4;
}

std::array<int, 2> h264Luma4x4Coord(int index)
{
    const int i8x8 = index / 4;
    const int i4x4 = index % 4;
    const int x = (i8x8 % 2) * 2 + (i4x4 % 2);
    const int y = (i8x8 / 2) * 2 + (i4x4 / 2);
    return {x, y};
}

int h264Chroma4x4Index(int x, int y)
{
    return y * 2 + x;
}

std::array<int, 2> h264Chroma4x4Coord(int index)
{
    return {index % 2, index / 2};
}

int predictedLumaNonZero(const H264SliceDataContext &context, int address, int blockIndex)
{
    const std::array<int, 2> coord = h264Luma4x4Coord(blockIndex);
    const int mbX = address % context.slice.picWidthInMbs;
    const int mbY = address / context.slice.picWidthInMbs;

    int left = 0;
    bool leftAvailable = false;
    if (coord[0] > 0) {
        left = context.coeffStates[address].luma[h264Luma4x4Index(coord[0] - 1, coord[1])];
        leftAvailable = true;
    } else if (mbX > 0) {
        left = context.coeffStates[address - 1].luma[h264Luma4x4Index(3, coord[1])];
        leftAvailable = true;
    }

    int top = 0;
    bool topAvailable = false;
    if (coord[1] > 0) {
        top = context.coeffStates[address].luma[h264Luma4x4Index(coord[0], coord[1] - 1)];
        topAvailable = true;
    } else if (mbY > 0) {
        top = context.coeffStates[address - context.slice.picWidthInMbs].luma[h264Luma4x4Index(coord[0], 3)];
        topAvailable = true;
    }

    if (leftAvailable && topAvailable) {
        return (left + top + 1) / 2;
    }
    return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
}

int predictedIntra16x16DcNonZero(const H264SliceDataContext &context, int address)
{
    const int mbX = address % context.slice.picWidthInMbs;
    const int mbY = address / context.slice.picWidthInMbs;

    int left = 0;
    bool leftAvailable = false;
    if (mbX > 0) {
        left = context.coeffStates[address - 1].luma16x16Dc;
        leftAvailable = true;
    }

    int top = 0;
    bool topAvailable = false;
    if (mbY > 0) {
        top = context.coeffStates[address - context.slice.picWidthInMbs].luma16x16Dc;
        topAvailable = true;
    }

    if (leftAvailable && topAvailable) {
        return (left + top + 1) / 2;
    }
    return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
}

int predictedChromaNonZero(const H264SliceDataContext &context, int address, int component, int blockIndex)
{
    const std::array<int, 2> coord = h264Chroma4x4Coord(blockIndex);
    const int mbX = address % context.slice.picWidthInMbs;
    const int mbY = address / context.slice.picWidthInMbs;

    int left = 0;
    bool leftAvailable = false;
    if (coord[0] > 0) {
        left = context.coeffStates[address].chroma[component][h264Chroma4x4Index(coord[0] - 1, coord[1])];
        leftAvailable = true;
    } else if (mbX > 0) {
        left = context.coeffStates[address - 1].chroma[component][h264Chroma4x4Index(1, coord[1])];
        leftAvailable = true;
    }

    int top = 0;
    bool topAvailable = false;
    if (coord[1] > 0) {
        top = context.coeffStates[address].chroma[component][h264Chroma4x4Index(coord[0], coord[1] - 1)];
        topAvailable = true;
    } else if (mbY > 0) {
        top = context.coeffStates[address - context.slice.picWidthInMbs].chroma[component][h264Chroma4x4Index(coord[0], 1)];
        topAvailable = true;
    }

    if (leftAvailable && topAvailable) {
        return (left + top + 1) / 2;
    }
    return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
}

int parseCavlcResidualBlockSummary(BitReader &reader,
                                   MacroblockInfo &mb,
                                   const QString &kind,
                                   int component,
                                   int blockIndex,
                                   int nC,
                                   int maxNumCoeff)
{
    const H264CavlcResidualBlock parsed = parseCavlcResidualBlock(reader, nC, maxNumCoeff);
    if (!parsed.valid) {
        return -1;
    }
    ResidualBlockInfo block {
        kind,
        component,
        blockIndex,
        nC,
        maxNumCoeff,
        parsed.totalCoeff,
        parsed.trailingOnes,
        parsed.totalZeros,
        parsed.coefficients
    };
    mb.residualBlocks.append(block);
    ++mb.residualBlockCount;
    mb.residualCoefficientCount += parsed.totalCoeff;
    return parsed.totalCoeff;
}
}

bool h264ParseCavlcMacroblockResidual(H264SliceDataContext &context,
                                      MacroblockInfo &mb,
                                      const H264CavlcMacroblockType &type,
                                      bool transform8x8,
                                      H264MacroblockCoeffState &coeffState)
{
    Q_UNUSED(transform8x8);

    if (type.intra16x16) {
        const int totalCoeff = parseCavlcResidualBlockSummary(context.reader,
                                                              mb,
                                                              QStringLiteral("luma16x16_dc"),
                                                              0,
                                                              0,
                                                              predictedIntra16x16DcNonZero(context, mb.address),
                                                              16);
        if (totalCoeff < 0) {
            return false;
        }
        coeffState.luma16x16Dc = totalCoeff;
    }

    if (type.intra16x16) {
        for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
            if (mb.codedBlockPatternLuma > 0) {
                const int totalCoeff = parseCavlcResidualBlockSummary(context.reader,
                                                                      mb,
                                                                      QStringLiteral("luma16x16_ac"),
                                                                      0,
                                                                      blockIndex,
                                                                      predictedLumaNonZero(context, mb.address, blockIndex),
                                                                      15);
                if (totalCoeff < 0) {
                    return false;
                }
                coeffState.luma[blockIndex] = totalCoeff;
            }
        }
    } else {
        for (int i8x8 = 0; i8x8 < 4; ++i8x8) {
            if (((mb.codedBlockPatternLuma >> i8x8) & 0x01) == 0) {
                continue;
            }
            for (int i4x4 = 0; i4x4 < 4; ++i4x4) {
                const int blockIndex = i8x8 * 4 + i4x4;
                const int totalCoeff = parseCavlcResidualBlockSummary(context.reader,
                                                                      mb,
                                                                      QStringLiteral("luma4x4"),
                                                                      0,
                                                                      blockIndex,
                                                                      predictedLumaNonZero(context, mb.address, blockIndex),
                                                                      16);
                if (totalCoeff < 0) {
                    return false;
                }
                coeffState.luma[blockIndex] = totalCoeff;
            }
        }
    }

    if (context.chromaArrayType == 0 || mb.codedBlockPatternChroma == 0) {
        return true;
    }
    if (context.chromaArrayType != 1) {
        context.appendDiagnostic(
            QStringLiteral("chroma_residual_unsupported"),
            QStringLiteral("CAVLC residual parsing currently supports 4:2:0 chroma only."));
        return false;
    }

    if ((mb.codedBlockPatternChroma & 0x03) != 0) {
        for (int component = 0; component < 2; ++component) {
            if (parseCavlcResidualBlockSummary(context.reader,
                                               mb,
                                               QStringLiteral("chroma_dc"),
                                               component,
                                               0,
                                               -1,
                                               4) < 0) {
                return false;
            }
        }
    }

    if ((mb.codedBlockPatternChroma & 0x02) != 0) {
        for (int component = 0; component < 2; ++component) {
            for (int blockIndex = 0; blockIndex < 4; ++blockIndex) {
                const int totalCoeff =
                    parseCavlcResidualBlockSummary(context.reader,
                                                   mb,
                                                   QStringLiteral("chroma_ac"),
                                                   component,
                                                   blockIndex,
                                                   predictedChromaNonZero(context, mb.address, component, blockIndex),
                                                   15);
                if (totalCoeff < 0) {
                    return false;
                }
                coeffState.chroma[component][blockIndex] = totalCoeff;
            }
        }
    }

    return true;
}
