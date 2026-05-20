#include "core/parser/video/h264/H264CavlcResidualParser.h"

#include <algorithm>

namespace
{
struct CoeffToken
{
    int totalCoeff = 0;
    int trailingOnes = 0;
    bool valid = false;
};

int readVlcValue(BitReader &reader, const uint8_t *lengths, const uint8_t *bits, int entryCount)
{
    int code = 0;
    for (int length = 1; length <= 16 && !reader.hasError(); ++length) {
        code = (code << 1) | (reader.readBit() ? 1 : 0);
        for (int value = 0; value < entryCount; ++value) {
            if (lengths[value] == length && bits[value] == code) {
                return value;
            }
        }
    }
    return -1;
}

CoeffToken readCoeffToken(BitReader &reader, int nC, int maxNumCoeff)
{
    static constexpr uint8_t coeffTokenLen[4][68] = {
        {
            1, 0, 0, 0,
            6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5,
            10, 9, 8, 6, 11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9,
            13, 13, 13, 10, 14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14,
            15, 15, 15, 14, 16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16,
            16, 16, 16, 16
        },
        {
            2, 0, 0, 0,
            6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4,
            8, 6, 6, 4, 8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6,
            11, 11, 11, 7, 12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11,
            13, 13, 13, 12, 13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13,
            14, 14, 14, 14
        },
        {
            4, 0, 0, 0,
            6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4,
            7, 5, 5, 4, 7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4,
            8, 7, 7, 5, 8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8,
            9, 9, 9, 8, 10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10
        },
        {
            6, 0, 0, 0,
            6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6
        }
    };
    static constexpr uint8_t coeffTokenBits[4][68] = {
        {
            1, 0, 0, 0,
            5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3,
            7, 6, 5, 3, 7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4,
            8, 10, 13, 4, 15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12,
            11, 10, 13, 8, 15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12,
            4, 6, 5, 8
        },
        {
            3, 0, 0, 0,
            11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5,
            7, 6, 5, 4, 4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4,
            11, 14, 13, 4, 15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8,
            15, 14, 13, 12, 11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1,
            7, 6, 5, 4
        },
        {
            15, 0, 0, 0,
            15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12,
            15, 10, 11, 11, 11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8,
            15, 14, 13, 13, 11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12,
            8, 10, 13, 8, 13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6,
            1, 4, 3, 2
        },
        {
            3, 0, 0, 0,
            0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11,
            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
            28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
            44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
            60, 61, 62, 63
        }
    };
    static constexpr uint8_t chromaDcCoeffTokenLen[20] = {
        2, 0, 0, 0,
        6, 1, 0, 0,
        6, 6, 3, 0,
        6, 7, 7, 6,
        6, 8, 8, 7
    };
    static constexpr uint8_t chromaDcCoeffTokenBits[20] = {
        1, 0, 0, 0,
        7, 1, 0, 0,
        4, 6, 1, 0,
        3, 3, 2, 5,
        2, 3, 2, 0
    };

    const uint8_t *lengths = nullptr;
    const uint8_t *bits = nullptr;
    int entryCount = 0;
    if (nC < 0) {
        lengths = chromaDcCoeffTokenLen;
        bits = chromaDcCoeffTokenBits;
        entryCount = 20;
    } else {
        const int table = nC < 2 ? 0 : (nC < 4 ? 1 : (nC < 8 ? 2 : 3));
        lengths = coeffTokenLen[table];
        bits = coeffTokenBits[table];
        entryCount = 68;
    }

    const int value = readVlcValue(reader, lengths, bits, entryCount);
    CoeffToken token;
    if (value < 0) {
        return token;
    }

    token.totalCoeff = value / 4;
    token.trailingOnes = value % 4;
    token.valid = token.totalCoeff <= maxNumCoeff
        && token.trailingOnes <= std::min(3, token.totalCoeff);
    return token;
}

int readTotalZeros(BitReader &reader, int totalCoeff, int maxNumCoeff, bool chromaDc)
{
    static constexpr uint8_t totalZerosLen[15][16] = {
        {1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
        {3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6, 0},
        {4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6, 0, 0},
        {5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5, 0, 0, 0},
        {4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5, 0, 0, 0, 0},
        {6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6, 0, 0, 0, 0, 0},
        {6, 5, 3, 3, 3, 2, 3, 4, 3, 6, 0, 0, 0, 0, 0, 0},
        {6, 4, 5, 3, 2, 2, 3, 3, 6, 0, 0, 0, 0, 0, 0, 0},
        {6, 6, 4, 2, 2, 3, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0},
        {5, 5, 3, 2, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {4, 4, 3, 3, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {4, 4, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    static constexpr uint8_t totalZerosBits[15][16] = {
        {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1},
        {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0, 0},
        {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0, 0, 0},
        {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0, 0, 0, 0},
        {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0},
        {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0},
        {1, 1, 5, 4, 3, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 1, 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 1, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    static constexpr uint8_t chromaDcTotalZerosLen[3][4] = {
        {1, 2, 3, 3},
        {1, 2, 2, 0},
        {1, 1, 0, 0}
    };
    static constexpr uint8_t chromaDcTotalZerosBits[3][4] = {
        {1, 1, 1, 0},
        {1, 1, 0, 0},
        {1, 0, 0, 0}
    };

    if (totalCoeff <= 0 || totalCoeff >= maxNumCoeff) {
        return 0;
    }

    const int maxZeros = maxNumCoeff - totalCoeff;
    const uint8_t *lengths = chromaDc ? chromaDcTotalZerosLen[totalCoeff - 1] : totalZerosLen[totalCoeff - 1];
    const uint8_t *bits = chromaDc ? chromaDcTotalZerosBits[totalCoeff - 1] : totalZerosBits[totalCoeff - 1];
    return readVlcValue(reader, lengths, bits, maxZeros + 1);
}

int readRunBefore(BitReader &reader, int zerosLeft)
{
    static constexpr uint8_t runLen[7][16] = {
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 2, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}
    };
    static constexpr uint8_t runBits[7][16] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 2, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 0, 1, 3, 2, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}
    };

    if (zerosLeft <= 0) {
        return 0;
    }
    const int row = std::min(zerosLeft, 7) - 1;
    return readVlcValue(reader, runLen[row], runBits[row], zerosLeft + 1);
}
}

H264CavlcResidualBlock parseCavlcResidualBlock(BitReader &reader, int nC, int maxNumCoeff)
{
    H264CavlcResidualBlock block;
    const bool chromaDc = nC < 0;
    const CoeffToken token = readCoeffToken(reader, nC, maxNumCoeff);
    if (!token.valid) {
        return block;
    }
    block.totalCoeff = token.totalCoeff;
    block.trailingOnes = token.trailingOnes;

    QVector<int> levels;
    int suffixLength = token.totalCoeff > 10 && token.trailingOnes < 3 ? 1 : 0;
    for (int i = 0; i < token.totalCoeff; ++i) {
        if (i < token.trailingOnes) {
            const bool negative = reader.readBit();
            levels.append(negative ? -1 : 1);
        } else {
            int levelPrefix = 0;
            while (!reader.hasError() && !reader.readBit()) {
                ++levelPrefix;
                if (levelPrefix > 15) {
                    break;
                }
            }

            int levelSuffixSize = suffixLength;
            if (levelPrefix == 14 && suffixLength == 0) {
                levelSuffixSize = 4;
            } else if (levelPrefix == 15) {
                levelSuffixSize = 12;
            }

            int levelCode = (std::min(levelPrefix, 15) << suffixLength);
            if (levelSuffixSize > 0) {
                levelCode += static_cast<int>(reader.readBits(levelSuffixSize));
            }
            if (levelPrefix == 15 && suffixLength == 0) {
                levelCode += 15;
            }
            if (token.trailingOnes < 3) {
                levelCode += 2;
            }

            const int absLevel = (levelCode + 2) / 2;
            const int level = (levelCode % 2) == 0
                ? absLevel
                : -((levelCode + 1) / 2);
            levels.append(level);
            if (suffixLength == 0) {
                suffixLength = 1;
            }
            if (absLevel > (3 << (suffixLength - 1)) && suffixLength < 6) {
                ++suffixLength;
            }
        }
    }

    const int totalZeros = readTotalZeros(reader, token.totalCoeff, maxNumCoeff, chromaDc);
    if (totalZeros < 0 || reader.hasError()) {
        return H264CavlcResidualBlock {};
    }
    block.totalZeros = totalZeros;

    QVector<int> runs;
    int zerosLeft = totalZeros;
    for (int i = 0; i < token.totalCoeff - 1 && zerosLeft > 0; ++i) {
        const int runBefore = readRunBefore(reader, zerosLeft);
        if (runBefore < 0 || reader.hasError()) {
            return H264CavlcResidualBlock {};
        }
        runs.append(runBefore);
        zerosLeft -= runBefore;
    }
    if (token.totalCoeff > 0) {
        runs.append(zerosLeft);
    }

    int scanIndex = -1;
    for (int i = token.totalCoeff - 1; i >= 0; --i) {
        const int runBefore = i < runs.size() ? runs[i] : 0;
        scanIndex += runBefore + 1;
        block.coefficients.append({
            scanIndex,
            i < levels.size() ? levels[i] : 0,
            runBefore
        });
    }

    block.valid = !reader.hasError();
    return block;
}
