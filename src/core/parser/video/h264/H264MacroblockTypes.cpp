#include "core/parser/video/h264/H264Parser.h"

int H264Parser::codedBlockPatternFromCodeNum(quint32 codeNum, bool intra, int chromaArrayType)
{
    static constexpr int codedBlockPatternChromaTable[48][2] = {
        {0, 47}, {16, 31}, {1, 15}, {2, 0}, {4, 23}, {8, 27}, {32, 29}, {3, 30},
        {5, 7}, {10, 11}, {12, 13}, {15, 14}, {47, 39}, {7, 43}, {11, 45}, {13, 46},
        {14, 16}, {6, 3}, {9, 5}, {31, 10}, {35, 12}, {37, 19}, {42, 21}, {44, 26},
        {33, 28}, {34, 35}, {36, 37}, {40, 42}, {39, 44}, {43, 33}, {45, 34}, {46, 36},
        {17, 40}, {18, 8}, {20, 17}, {24, 18}, {19, 20}, {21, 24}, {26, 6}, {28, 9},
        {23, 22}, {27, 25}, {29, 32}, {30, 38}, {22, 41}, {25, 4}, {38, 1}, {41, 2}
    };

    static constexpr int codedBlockPatternMonoTable[16][2] = {
        {0, 15}, {1, 0}, {2, 7}, {4, 11}, {8, 13}, {3, 14}, {5, 3}, {10, 5},
        {12, 10}, {15, 12}, {7, 1}, {11, 2}, {13, 4}, {14, 8}, {6, 6}, {9, 9}
    };

    if (chromaArrayType == 1 || chromaArrayType == 2) {
        if (codeNum >= 48) {
            return 0;
        }
        return codedBlockPatternChromaTable[codeNum][intra ? 1 : 0];
    }

    if (codeNum >= 16) {
        return 0;
    }
    return codedBlockPatternMonoTable[codeNum][intra ? 1 : 0];
}

QString H264Parser::intraMbTypeName(int mbType)
{
    if (mbType == 0) {
        return QStringLiteral("I_NxN");
    }
    if (mbType == 25) {
        return QStringLiteral("I_PCM");
    }
    if (mbType >= 1 && mbType <= 24) {
        return QStringLiteral("I_16x16");
    }
    return QStringLiteral("I_Unknown(%1)").arg(mbType);
}

QString H264Parser::pMbTypeName(int mbType)
{
    switch (mbType) {
    case 0: return QStringLiteral("P_L0_16x16");
    case 1: return QStringLiteral("P_L0_L0_16x8");
    case 2: return QStringLiteral("P_L0_L0_8x16");
    case 3: return QStringLiteral("P_8x8");
    case 4: return QStringLiteral("P_8x8ref0");
    default: return QStringLiteral("P_Unknown(%1)").arg(mbType);
    }
}

QString H264Parser::bMbTypeName(int mbType)
{
    switch (mbType) {
    case 0: return QStringLiteral("B_Direct_16x16");
    case 1: return QStringLiteral("B_L0_16x16");
    case 2: return QStringLiteral("B_L1_16x16");
    case 3: return QStringLiteral("B_Bi_16x16");
    case 4: return QStringLiteral("B_L0_L0_16x8");
    case 5: return QStringLiteral("B_L0_L0_8x16");
    case 6: return QStringLiteral("B_L1_L1_16x8");
    case 7: return QStringLiteral("B_L1_L1_8x16");
    case 8: return QStringLiteral("B_L0_L1_16x8");
    case 9: return QStringLiteral("B_L0_L1_8x16");
    case 10: return QStringLiteral("B_L1_L0_16x8");
    case 11: return QStringLiteral("B_L1_L0_8x16");
    case 12: return QStringLiteral("B_L0_Bi_16x8");
    case 13: return QStringLiteral("B_L0_Bi_8x16");
    case 14: return QStringLiteral("B_L1_Bi_16x8");
    case 15: return QStringLiteral("B_L1_Bi_8x16");
    case 16: return QStringLiteral("B_Bi_L0_16x8");
    case 17: return QStringLiteral("B_Bi_L0_8x16");
    case 18: return QStringLiteral("B_Bi_L1_16x8");
    case 19: return QStringLiteral("B_Bi_L1_8x16");
    case 20: return QStringLiteral("B_Bi_Bi_16x8");
    case 21: return QStringLiteral("B_Bi_Bi_8x16");
    case 22: return QStringLiteral("B_8x8");
    default: return QStringLiteral("B_Unknown(%1)").arg(mbType);
    }
}
