#include "core/parser/video/h264/H264Parser.h"

#include <algorithm>
#include <array>

void H264Parser::parseSliceData(BitReader &reader, SliceInfo &slice, const PpsInfo &pps, const SpsInfo &sps) const
{
    struct MacroblockMvState
    {
        bool valid = false;
        int refIndex = 0;
        int mvX = 0;
        int mvY = 0;
    };

    struct PartitionMv
    {
        int refIndex = 0;
        int mvdX = 0;
        int mvdY = 0;
        int mvX = 0;
        int mvY = 0;
    };

    enum class PredictionList
    {
        None,
        L0,
        L1,
        Bi
    };

    struct BPartitionModes
    {
        int partitionCount = 1;
        std::array<PredictionList, 2> modes {PredictionList::None, PredictionList::None};
        bool supported = false;
        QString unsupportedCode;
        QString unsupportedMessage;
    };

    struct CoeffToken
    {
        int totalCoeff = 0;
        int trailingOnes = 0;
        bool valid = false;
    };

    struct MacroblockCoeffState
    {
        int luma16x16Dc = 0;
        std::array<int, 16> luma {};
        std::array<std::array<int, 4>, 2> chroma {};
    };

    const int totalMacroblocks = std::max(1, slice.picWidthInMbs * slice.picHeightInMbs);
    int currentAddress = std::clamp(slice.firstMbInSlice, 0, totalMacroblocks - 1);
    int currentQp = slice.derivedQp;
    const int normalizedSliceType = slice.sliceType % 5;
    const bool isISlice = normalizedSliceType == 2 || normalizedSliceType == 4;
    const bool isPSlice = normalizedSliceType == 0 || normalizedSliceType == 3;
    const bool isBSlice = normalizedSliceType == 1;
    const int chromaArrayType = sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc;
    QVector<MacroblockMvState> mvStatesL0(totalMacroblocks);
    QVector<MacroblockMvState> mvStatesL1(totalMacroblocks);
    QVector<MacroblockCoeffState> coeffStates(totalMacroblocks);

    auto median = [](int a, int b, int c) {
        return a + b + c - std::min({a, b, c}) - std::max({a, b, c});
    };

    auto neighborMv = [&](const QVector<MacroblockMvState> &states, int address) {
        if (address < 0 || address >= states.size()) {
            return MacroblockMvState {};
        }
        return states[address];
    };

    auto predictMv = [&](const QVector<MacroblockMvState> &states, int address, int refIndex) {
        const int mbX = address % slice.picWidthInMbs;
        const int leftAddress = mbX > 0 ? address - 1 : -1;
        const int topAddress = address >= slice.picWidthInMbs ? address - slice.picWidthInMbs : -1;
        const int topRightAddress = (topAddress >= 0 && mbX + 1 < slice.picWidthInMbs)
            ? topAddress + 1
            : -1;
        const int topLeftAddress = (topAddress >= 0 && mbX > 0)
            ? topAddress - 1
            : -1;

        const MacroblockMvState a = neighborMv(states, leftAddress);
        const MacroblockMvState b = neighborMv(states, topAddress);
        MacroblockMvState c = neighborMv(states, topRightAddress);
        if (!c.valid) {
            c = neighborMv(states, topLeftAddress);
        }

        QVector<MacroblockMvState> matching;
        for (const MacroblockMvState &candidate : {a, b, c}) {
            if (candidate.valid && candidate.refIndex == refIndex) {
                matching.append(candidate);
            }
        }
        if (matching.size() == 1) {
            return matching.first();
        }

        MacroblockMvState result;
        result.valid = a.valid || b.valid || c.valid;
        result.refIndex = refIndex;
        result.mvX = median(a.valid ? a.mvX : 0, b.valid ? b.mvX : 0, c.valid ? c.mvX : 0);
        result.mvY = median(a.valid ? a.mvY : 0, b.valid ? b.mvY : 0, c.valid ? c.mvY : 0);
        return result;
    };

    auto addMotionVector = [&](MacroblockInfo &mb, int list, int refIndex, int mvX, int mvY) {
        MotionVectorInfo mv;
        mv.list = list;
        mv.referenceIndex = refIndex;
        mv.mvXQuarterPel = mvX;
        mv.mvYQuarterPel = mvY;
        mb.motionVectors.append(mv);
    };

    auto setMvState = [&](const MacroblockInfo &mb) {
        if (mb.address < 0 || mb.motionVectors.isEmpty()) {
            return;
        }
        for (const MotionVectorInfo &mv : mb.motionVectors) {
            QVector<MacroblockMvState> &states = mv.list == 1 ? mvStatesL1 : mvStatesL0;
            if (mb.address < states.size()) {
                states[mb.address] = {true, mv.referenceIndex, mv.mvXQuarterPel, mv.mvYQuarterPel};
            }
        }
    };

    auto readTE = [&](int range) {
        if (range <= 0) {
            return 0;
        }
        if (range == 1) {
            return reader.readBit() ? 0 : 1;
        }
        return static_cast<int>(reader.readUE());
    };

    auto addMbField = [](MacroblockInfo &mb,
                         const QString &name,
                         qsizetype start,
                         qsizetype end,
                         const QString &value) {
        mb.fields.append({name, start, end - start, value});
    };

    auto readUEMbField = [&](MacroblockInfo &mb, const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addMbField(mb, name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    auto readSEMBField = [&](MacroblockInfo &mb, const QString &name) {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addMbField(mb, name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    auto readBitMbField = [&](MacroblockInfo &mb, const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addMbField(mb, name, start, reader.bitOffset(), value ? QStringLiteral("1") : QStringLiteral("0"));
        return value;
    };

    auto readTEMbField = [&](MacroblockInfo &mb, const QString &name, int range) {
        if (range <= 0) {
            addMbField(mb, name, reader.bitOffset(), reader.bitOffset(), QStringLiteral("0"));
            return 0;
        }
        if (range == 1) {
            const qsizetype start = reader.bitOffset();
            const int value = reader.readBit() ? 0 : 1;
            addMbField(mb, name, start, reader.bitOffset(), QString::number(value));
            return value;
        }
        return static_cast<int>(readUEMbField(mb, name));
    };

    auto bPartitionModes = [](int mbType) {
        BPartitionModes result;
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
            result.modes[0] = mbType == 1 ? PredictionList::L0
                : (mbType == 2 ? PredictionList::L1 : PredictionList::Bi);
            return result;
        }

        result.partitionCount = 2;
        switch (mbType) {
        case 4:
        case 5:
            result.modes = {PredictionList::L0, PredictionList::L0};
            break;
        case 6:
        case 7:
            result.modes = {PredictionList::L1, PredictionList::L1};
            break;
        case 8:
        case 9:
            result.modes = {PredictionList::L0, PredictionList::L1};
            break;
        case 10:
        case 11:
            result.modes = {PredictionList::L1, PredictionList::L0};
            break;
        case 12:
        case 13:
            result.modes = {PredictionList::L0, PredictionList::Bi};
            break;
        case 14:
        case 15:
            result.modes = {PredictionList::L1, PredictionList::Bi};
            break;
        case 16:
        case 17:
            result.modes = {PredictionList::Bi, PredictionList::L0};
            break;
        case 18:
        case 19:
            result.modes = {PredictionList::Bi, PredictionList::L1};
            break;
        case 20:
        case 21:
            result.modes = {PredictionList::Bi, PredictionList::Bi};
            break;
        }
        return result;
    };

    auto appendDiagnostic = [&](const QString &code, const QString &message) {
        if (!message.isEmpty()) {
            slice.diagnostics.append({code, message});
            slice.macroblockParseWarnings.append(message);
        }
    };

    auto appendEstimatedRemainder = [&](const QString &code, const QString &message) {
        if (!message.isEmpty()) {
            appendDiagnostic(code, message);
        }
        for (int address = currentAddress; address < totalMacroblocks; ++address) {
            MacroblockInfo mb;
            mb.address = address;
            mb.mbType = QStringLiteral("Estimated");
            mb.predictionMode = QStringLiteral("unknown");
            mb.qp = currentQp;
            mb.note = message.isEmpty()
                ? QStringLiteral("QP carried forward after slice_data parsing stopped.")
                : message;
            slice.macroblocks.append(mb);
        }
    };

    auto luma4x4Index = [](int x, int y) {
        const int i8x8 = (y / 2) * 2 + (x / 2);
        const int i4x4 = (y % 2) * 2 + (x % 2);
        return i8x8 * 4 + i4x4;
    };

    auto luma4x4Coord = [](int index) {
        const int i8x8 = index / 4;
        const int i4x4 = index % 4;
        const int x = (i8x8 % 2) * 2 + (i4x4 % 2);
        const int y = (i8x8 / 2) * 2 + (i4x4 / 2);
        return std::array<int, 2> {x, y};
    };

    auto predictedLumaNonZero = [&](int address, int blockIndex) {
        const std::array<int, 2> coord = luma4x4Coord(blockIndex);
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (coord[0] > 0) {
            left = coeffStates[address].luma[luma4x4Index(coord[0] - 1, coord[1])];
            leftAvailable = true;
        } else if (mbX > 0) {
            left = coeffStates[address - 1].luma[luma4x4Index(3, coord[1])];
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (coord[1] > 0) {
            top = coeffStates[address].luma[luma4x4Index(coord[0], coord[1] - 1)];
            topAvailable = true;
        } else if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].luma[luma4x4Index(coord[0], 3)];
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto predictedIntra16x16DcNonZero = [&](int address) {
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (mbX > 0) {
            left = coeffStates[address - 1].luma16x16Dc;
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].luma16x16Dc;
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto chroma4x4Index = [](int x, int y) {
        return y * 2 + x;
    };

    auto chroma4x4Coord = [](int index) {
        return std::array<int, 2> {index % 2, index / 2};
    };

    auto predictedChromaNonZero = [&](int address, int component, int blockIndex) {
        const std::array<int, 2> coord = chroma4x4Coord(blockIndex);
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (coord[0] > 0) {
            left = coeffStates[address].chroma[component][chroma4x4Index(coord[0] - 1, coord[1])];
            leftAvailable = true;
        } else if (mbX > 0) {
            left = coeffStates[address - 1].chroma[component][chroma4x4Index(1, coord[1])];
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (coord[1] > 0) {
            top = coeffStates[address].chroma[component][chroma4x4Index(coord[0], coord[1] - 1)];
            topAvailable = true;
        } else if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].chroma[component][chroma4x4Index(coord[0], 1)];
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto readVlcValue = [&](const uint8_t *lengths, const uint8_t *bits, int entryCount) {
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
    };

    auto readCoeffToken = [&](int nC, int maxNumCoeff) {
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

        const int value = readVlcValue(lengths, bits, entryCount);
        CoeffToken token;
        if (value < 0) {
            return token;
        }

        token.totalCoeff = value / 4;
        token.trailingOnes = value % 4;
        token.valid = token.totalCoeff <= maxNumCoeff
            && token.trailingOnes <= std::min(3, token.totalCoeff);
        return token;
    };

    auto readTotalZeros = [&](int totalCoeff, int maxNumCoeff, bool chromaDc) {
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
        return readVlcValue(lengths, bits, maxZeros + 1);
    };

    auto readRunBefore = [&](int zerosLeft) {
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
        return readVlcValue(runLen[row], runBits[row], zerosLeft + 1);
    };

    auto parseResidualBlockCavlc = [&](int nC, int maxNumCoeff) {
        const bool chromaDc = nC < 0;
        const CoeffToken token = readCoeffToken(nC, maxNumCoeff);
        if (!token.valid) {
            return -1;
        }

        int suffixLength = token.totalCoeff > 10 && token.trailingOnes < 3 ? 1 : 0;
        for (int i = 0; i < token.totalCoeff; ++i) {
            if (i < token.trailingOnes) {
                reader.readBit();
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
                if (suffixLength == 0) {
                    suffixLength = 1;
                }
                if (absLevel > (3 << (suffixLength - 1)) && suffixLength < 6) {
                    ++suffixLength;
                }
            }
        }

        const int totalZeros = readTotalZeros(token.totalCoeff, maxNumCoeff, chromaDc);
        if (totalZeros < 0 || reader.hasError()) {
            return -1;
        }

        int zerosLeft = totalZeros;
        for (int i = 0; i < token.totalCoeff - 1 && zerosLeft > 0; ++i) {
            const int runBefore = readRunBefore(zerosLeft);
            if (runBefore < 0 || reader.hasError()) {
                return -1;
            }
            zerosLeft -= runBefore;
        }
        return token.totalCoeff;
    };

    auto parseResidualBlock = [&](MacroblockInfo &mb, int nC, int maxNumCoeff) {
        const int totalCoeff = parseResidualBlockCavlc(nC, maxNumCoeff);
        if (totalCoeff < 0) {
            return -1;
        }
        ++mb.residualBlockCount;
        mb.residualCoefficientCount += totalCoeff;
        return totalCoeff;
    };

    auto parseResidual = [&](MacroblockInfo &mb,
                             bool intra16x16,
                             bool transform8x8,
                             MacroblockCoeffState &coeffState) {
        Q_UNUSED(transform8x8);

        if (intra16x16) {
            const int totalCoeff = parseResidualBlock(mb, predictedIntra16x16DcNonZero(mb.address), 16);
            if (totalCoeff < 0) {
                return false;
            }
            coeffState.luma16x16Dc = totalCoeff;
        }

        if (intra16x16) {
            for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
                if (mb.codedBlockPatternLuma > 0) {
                    const int totalCoeff = parseResidualBlock(mb, predictedLumaNonZero(mb.address, blockIndex), 15);
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
                    const int totalCoeff = parseResidualBlock(mb, predictedLumaNonZero(mb.address, blockIndex), 16);
                    if (totalCoeff < 0) {
                        return false;
                    }
                    coeffState.luma[blockIndex] = totalCoeff;
                }
            }
        }

        if (chromaArrayType == 0 || mb.codedBlockPatternChroma == 0) {
            return true;
        }
        if (chromaArrayType != 1) {
            appendDiagnostic(
                QStringLiteral("chroma_residual_unsupported"),
                QStringLiteral("CAVLC residual parsing currently supports 4:2:0 chroma only."));
            return false;
        }

        if ((mb.codedBlockPatternChroma & 0x03) != 0) {
            for (int component = 0; component < 2; ++component) {
                if (parseResidualBlock(mb, -1, 4) < 0) {
                    return false;
                }
            }
        }

        if ((mb.codedBlockPatternChroma & 0x02) != 0) {
            for (int component = 0; component < 2; ++component) {
                for (int blockIndex = 0; blockIndex < 4; ++blockIndex) {
                    const int totalCoeff =
                        parseResidualBlock(mb, predictedChromaNonZero(mb.address, component, blockIndex), 15);
                    if (totalCoeff < 0) {
                        return false;
                    }
                    coeffState.chroma[component][blockIndex] = totalCoeff;
                }
            }
        }

        return true;
    };

    if (pps.entropyCodingModeFlag) {
        appendEstimatedRemainder(
            QStringLiteral("cabac_unsupported"),
            QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header."));
        return;
    }

    if (!sps.frameMbsOnlyFlag || pps.numSliceGroupsMinus1 > 0) {
        appendEstimatedRemainder(
            QStringLiteral("interlaced_or_fmo_unsupported"),
            QStringLiteral("Interlaced/MBAFF or FMO slice_data parsing is not implemented; macroblock QP is carried forward from the slice header."));
        return;
    }

    while (currentAddress < totalMacroblocks && reader.moreRbspData() && !reader.hasError()) {
        if (!isISlice) {
            const quint32 mbSkipRun = reader.readUE();
            if (reader.hasError()) {
                break;
            }
            for (quint32 i = 0; i < mbSkipRun && currentAddress < totalMacroblocks; ++i) {
                MacroblockInfo mb;
                mb.address = currentAddress++;
                mb.mbType = isPSlice ? QStringLiteral("P_Skip") : QStringLiteral("B_Skip/Direct");
                mb.predictionMode = isPSlice ? QStringLiteral("Pred_L0") : QStringLiteral("Direct");
                mb.codedBlockPattern = 0;
                mb.codedBlockPatternLuma = 0;
                mb.codedBlockPatternChroma = 0;
                mb.qp = currentQp;
                mb.skipped = true;
                mb.parsed = true;
                if (isPSlice) {
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, 0);
                    addMotionVector(mb, 0, 0, predicted.mvX, predicted.mvY);
                    setMvState(mb);
                    mb.note = QStringLiteral("Parsed from mb_skip_run; P_Skip motion vector uses neighboring median prediction.");
                } else {
                    mb.note = QStringLiteral("Parsed from mb_skip_run; B direct motion vector parsing is not implemented.");
                }
                coeffStates[mb.address] = MacroblockCoeffState {};
                slice.macroblocks.append(mb);
            }
            if (mbSkipRun > 0 && !reader.moreRbspData()) {
                break;
            }
            if (currentAddress >= totalMacroblocks) {
                break;
            }
        }

        MacroblockInfo mb;
        mb.address = currentAddress;
        mb.qp = currentQp;
        mb.parsed = true;

        const quint32 rawMbType = readUEMbField(mb, QStringLiteral("mb_type"));
        if (reader.hasError()) {
            break;
        }

        bool intra = isISlice;
        int localMbType = static_cast<int>(rawMbType);
        if (isPSlice && localMbType >= 5) {
            intra = true;
            localMbType -= 5;
        } else if (isBSlice && localMbType >= 23) {
            intra = true;
            localMbType -= 23;
        }

        const bool intra16x16 = intra && localMbType >= 1 && localMbType <= 24;
        const bool iPcm = intra && localMbType == 25;
        const bool p8x8 = isPSlice && !intra && (localMbType == 3 || localMbType == 4);
        bool transform8x8 = false;

        if (intra) {
            mb.mbType = intraMbTypeName(localMbType);
            mb.predictionMode = intra16x16 ? QStringLiteral("Intra_16x16")
                                           : (iPcm ? QStringLiteral("I_PCM") : QStringLiteral("Intra_4x4/8x8"));
        } else if (isPSlice) {
            mb.mbType = pMbTypeName(localMbType);
            mb.predictionMode = QStringLiteral("Pred_L0");
        } else if (isBSlice) {
            mb.mbType = bMbTypeName(localMbType);
            mb.predictionMode = QStringLiteral("BiPred");
        } else {
            mb.mbType = QStringLiteral("Unsupported");
            mb.predictionMode = QStringLiteral("unsupported");
            mb.note = QStringLiteral("This slice type is not supported for macroblock parsing.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            appendEstimatedRemainder(QStringLiteral("slice_macroblock_unsupported"), mb.note);
            return;
        }

        if (iPcm) {
            mb.note = QStringLiteral("I_PCM macroblock payload is byte aligned raw samples; skipping exact payload is not implemented.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            appendEstimatedRemainder(QStringLiteral("i_pcm_unsupported"), mb.note);
            return;
        }

        if (intra && !intra16x16) {
            transform8x8 = pps.transform8x8ModeFlag
                && readBitMbField(mb, QStringLiteral("transform_size_8x8_flag"));
            const int predictionBlocks = transform8x8 ? 4 : 16;
            for (int i = 0; i < predictionBlocks; ++i) {
                if (!reader.readBit()) {
                    reader.readBits(3);
                }
            }
            readUEMbField(mb, QStringLiteral("intra_chroma_pred_mode"));
        } else if (intra16x16) {
            readUEMbField(mb, QStringLiteral("intra_chroma_pred_mode"));
        } else if (isPSlice) {
            int partitionCount = 1;
            std::array<PartitionMv, 2> partitions;
            if (localMbType == 1 || localMbType == 2) {
                partitionCount = 2;
            } else if (p8x8) {
                struct SubMacroblock
                {
                    int type = 0;
                    int refIndex = 0;
                    int partitionCount = 1;
                };

                std::array<SubMacroblock, 4> subMacroblocks;
                for (int subIndex = 0; subIndex < static_cast<int>(subMacroblocks.size()); ++subIndex) {
                    SubMacroblock &subMb = subMacroblocks[subIndex];
                    subMb.type = static_cast<int>(readUEMbField(
                        mb,
                        QStringLiteral("sub_mb_type_l0[%1]").arg(subIndex)));
                    switch (subMb.type) {
                    case 0:
                        subMb.partitionCount = 1;
                        break;
                    case 1:
                    case 2:
                        subMb.partitionCount = 2;
                        break;
                    case 3:
                        subMb.partitionCount = 4;
                        break;
                    default:
                        mb.note = QStringLiteral("Unsupported P sub_mb_type %1; remaining macroblocks are estimated.")
                                      .arg(subMb.type);
                        currentAddress = mb.address + 1;
                        slice.macroblocks.append(mb);
                        appendEstimatedRemainder(QStringLiteral("p8x8_sub_macroblock_type_unsupported"), mb.note);
                        return;
                    }
                }

                if (slice.numRefIdxL0ActiveMinus1 > 0 && localMbType != 4) {
                    for (int subIndex = 0; subIndex < static_cast<int>(subMacroblocks.size()); ++subIndex) {
                        SubMacroblock &subMb = subMacroblocks[subIndex];
                        subMb.refIndex = readTEMbField(
                            mb,
                            QStringLiteral("ref_idx_l0[%1]").arg(subIndex),
                            slice.numRefIdxL0ActiveMinus1);
                    }
                }

                for (int subIndex = 0; subIndex < static_cast<int>(subMacroblocks.size()); ++subIndex) {
                    const SubMacroblock &subMb = subMacroblocks[subIndex];
                    for (int part = 0; part < subMb.partitionCount; ++part) {
                        const int mvdX = readSEMBField(
                            mb,
                            QStringLiteral("mvd_l0[%1][%2][0]").arg(subIndex).arg(part));
                        const int mvdY = readSEMBField(
                            mb,
                            QStringLiteral("mvd_l0[%1][%2][1]").arg(subIndex).arg(part));
                        const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, subMb.refIndex);
                        addMotionVector(mb, 0, subMb.refIndex, predicted.mvX + mvdX, predicted.mvY + mvdY);
                    }
                }
                setMvState(mb);
                mb.predictionMode = QStringLiteral("Pred_L0 sub-macroblock");
            }

            if (!p8x8 && slice.numRefIdxL0ActiveMinus1 > 0) {
                for (int i = 0; i < partitionCount; ++i) {
                    partitions[i].refIndex = readTEMbField(
                        mb,
                        QStringLiteral("ref_idx_l0[%1]").arg(i),
                        slice.numRefIdxL0ActiveMinus1);
                }
            }
            if (!p8x8) {
                for (int i = 0; i < partitionCount; ++i) {
                    partitions[i].mvdX = readSEMBField(mb, QStringLiteral("mvd_l0[%1][0]").arg(i));
                    partitions[i].mvdY = readSEMBField(mb, QStringLiteral("mvd_l0[%1][1]").arg(i));
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, partitions[i].refIndex);
                    partitions[i].mvX = predicted.mvX + partitions[i].mvdX;
                    partitions[i].mvY = predicted.mvY + partitions[i].mvdY;
                    addMotionVector(mb, 0, partitions[i].refIndex, partitions[i].mvX, partitions[i].mvY);
                }
                setMvState(mb);
            }
        } else if (isBSlice) {
            const BPartitionModes modes = bPartitionModes(localMbType);
            if (!modes.supported) {
                mb.note = modes.unsupportedMessage;
                currentAddress = mb.address + 1;
                slice.macroblocks.append(mb);
                appendEstimatedRemainder(modes.unsupportedCode, mb.note);
                return;
            }

            std::array<PartitionMv, 2> l0Partitions;
            std::array<PartitionMv, 2> l1Partitions;
            if (slice.numRefIdxL0ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == PredictionList::L0 || modes.modes[i] == PredictionList::Bi) {
                        l0Partitions[i].refIndex = readTEMbField(
                            mb,
                            QStringLiteral("ref_idx_l0[%1]").arg(i),
                            slice.numRefIdxL0ActiveMinus1);
                    }
                }
            }
            if (slice.numRefIdxL1ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == PredictionList::L1 || modes.modes[i] == PredictionList::Bi) {
                        l1Partitions[i].refIndex = readTEMbField(
                            mb,
                            QStringLiteral("ref_idx_l1[%1]").arg(i),
                            slice.numRefIdxL1ActiveMinus1);
                    }
                }
            }

            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == PredictionList::L0 || modes.modes[i] == PredictionList::Bi) {
                    l0Partitions[i].mvdX = readSEMBField(mb, QStringLiteral("mvd_l0[%1][0]").arg(i));
                    l0Partitions[i].mvdY = readSEMBField(mb, QStringLiteral("mvd_l0[%1][1]").arg(i));
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, l0Partitions[i].refIndex);
                    l0Partitions[i].mvX = predicted.mvX + l0Partitions[i].mvdX;
                    l0Partitions[i].mvY = predicted.mvY + l0Partitions[i].mvdY;
                    addMotionVector(mb, 0, l0Partitions[i].refIndex, l0Partitions[i].mvX, l0Partitions[i].mvY);
                }
            }
            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == PredictionList::L1 || modes.modes[i] == PredictionList::Bi) {
                    l1Partitions[i].mvdX = readSEMBField(mb, QStringLiteral("mvd_l1[%1][0]").arg(i));
                    l1Partitions[i].mvdY = readSEMBField(mb, QStringLiteral("mvd_l1[%1][1]").arg(i));
                    const MacroblockMvState predicted = predictMv(mvStatesL1, mb.address, l1Partitions[i].refIndex);
                    l1Partitions[i].mvX = predicted.mvX + l1Partitions[i].mvdX;
                    l1Partitions[i].mvY = predicted.mvY + l1Partitions[i].mvdY;
                    addMotionVector(mb, 1, l1Partitions[i].refIndex, l1Partitions[i].mvX, l1Partitions[i].mvY);
                }
            }
            setMvState(mb);
        }

        if (reader.hasError()) {
            break;
        }

        if (intra16x16) {
            const int typeOffset = localMbType - 1;
            mb.codedBlockPatternChroma = (typeOffset / 4) % 3;
            mb.codedBlockPatternLuma = (typeOffset / 12) != 0 ? 15 : 0;
            mb.codedBlockPattern = mb.codedBlockPatternLuma | (mb.codedBlockPatternChroma << 4);
        } else {
            const quint32 codedBlockPatternCodeNum = readUEMbField(mb, QStringLiteral("coded_block_pattern"));
            mb.codedBlockPattern = codedBlockPatternFromCodeNum(codedBlockPatternCodeNum, intra, chromaArrayType);
            mb.codedBlockPatternLuma = mb.codedBlockPattern & 0x0f;
            mb.codedBlockPatternChroma = (mb.codedBlockPattern >> 4) & 0x03;
            if (mb.codedBlockPatternLuma > 0 && pps.transform8x8ModeFlag && !intra) {
                transform8x8 = readBitMbField(mb, QStringLiteral("transform_size_8x8_flag"));
            }
        }

        const bool hasResidual = intra16x16 || mb.codedBlockPatternLuma > 0 || mb.codedBlockPatternChroma > 0;
        MacroblockCoeffState coeffState;
        if (hasResidual) {
            mb.mbQpDelta = readSEMBField(mb, QStringLiteral("mb_qp_delta"));
            currentQp = (currentQp + mb.mbQpDelta + 52) % 52;
            mb.qp = currentQp;
            if (!parseResidual(mb, intra16x16, transform8x8, coeffState) || reader.hasError()) {
                mb.note = QStringLiteral("Parsed macroblock header and mb_qp_delta, but residual CAVLC parsing stopped on malformed or unsupported residual data.");
                currentAddress = mb.address + 1;
                slice.macroblocks.append(mb);
                appendEstimatedRemainder(
                    QStringLiteral("cavlc_residual_parse_failed"),
                    QStringLiteral("CAVLC residual data could not be fully parsed; remaining macroblocks are estimated."));
                slice.macroblocksParsed = !slice.macroblocks.isEmpty();
                return;
            }
            mb.residualParsed = true;
            coeffStates[mb.address] = coeffState;
            mb.note = QStringLiteral("Parsed macroblock header, mb_qp_delta, and CAVLC residual data.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            continue;
        }

        mb.qp = currentQp;
        coeffStates[mb.address] = coeffState;
        mb.note = QStringLiteral("Parsed macroblock header; no residual data present.");
        currentAddress = mb.address + 1;
        slice.macroblocks.append(mb);
    }

    if (reader.hasError()) {
        appendEstimatedRemainder(
            QStringLiteral("slice_data_truncated"),
            QStringLiteral("slice_data ended unexpectedly; remaining macroblocks are estimated."));
    }

    if (slice.macroblocks.isEmpty()) {
        appendEstimatedRemainder(
            QStringLiteral("slice_data_missing"),
            QStringLiteral("No macroblock data could be parsed; QP is carried forward from the slice header."));
    }

    slice.macroblocksParsed = !slice.macroblocks.isEmpty();
}
