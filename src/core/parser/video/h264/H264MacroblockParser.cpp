#include "core/parser/video/h264/H264Parser.h"

#include "core/parser/video/h264/H264CabacMacroblockParser.h"
#include "core/parser/video/h264/H264CavlcResidualParser.h"
#include "core/parser/video/h264/H264MotionVectorParser.h"

#include <algorithm>
#include <array>

void H264Parser::parseSliceData(BitReader &reader, SliceInfo &slice, const PpsInfo &pps, const SpsInfo &sps) const
{
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
    QVector<H264MacroblockMvState> mvStatesL0(totalMacroblocks);
    QVector<H264MacroblockMvState> mvStatesL1(totalMacroblocks);
    QVector<MacroblockCoeffState> coeffStates(totalMacroblocks);

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

    auto parseResidualBlock = [&](MacroblockInfo &mb,
                                  const QString &kind,
                                  int component,
                                  int blockIndex,
                                  int nC,
                                  int maxNumCoeff) {
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
    };

    auto parseResidual = [&](MacroblockInfo &mb,
                             bool intra16x16,
                             bool transform8x8,
                             MacroblockCoeffState &coeffState) {
        Q_UNUSED(transform8x8);

        if (intra16x16) {
            const int totalCoeff = parseResidualBlock(mb,
                                                      QStringLiteral("luma16x16_dc"),
                                                      0,
                                                      0,
                                                      predictedIntra16x16DcNonZero(mb.address),
                                                      16);
            if (totalCoeff < 0) {
                return false;
            }
            coeffState.luma16x16Dc = totalCoeff;
        }

        if (intra16x16) {
            for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
                if (mb.codedBlockPatternLuma > 0) {
                    const int totalCoeff = parseResidualBlock(mb,
                                                              QStringLiteral("luma16x16_ac"),
                                                              0,
                                                              blockIndex,
                                                              predictedLumaNonZero(mb.address, blockIndex),
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
                    const int totalCoeff = parseResidualBlock(mb,
                                                              QStringLiteral("luma4x4"),
                                                              0,
                                                              blockIndex,
                                                              predictedLumaNonZero(mb.address, blockIndex),
                                                              16);
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
                if (parseResidualBlock(mb,
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
                        parseResidualBlock(mb,
                                           QStringLiteral("chroma_ac"),
                                           component,
                                           blockIndex,
                                           predictedChromaNonZero(mb.address, component, blockIndex),
                                           15);
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
        const H264CabacUnsupportedResult cabac = h264CabacUnsupportedResult();
        appendEstimatedRemainder(
            cabac.code,
            cabac.message);
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
                    const H264MacroblockMvState predicted =
                        h264PredictMv(mvStatesL0, slice.picWidthInMbs, mb.address, 0);
                    h264AddMotionVector(mb, 0, 0, predicted.mvX, predicted.mvY);
                    h264SetMvState(mb, mvStatesL0, mvStatesL1);
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
            std::array<H264PartitionMv, 2> partitions;
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
                        const H264MacroblockMvState predicted =
                            h264PredictMv(mvStatesL0, slice.picWidthInMbs, mb.address, subMb.refIndex);
                        h264AddMotionVector(mb, 0, subMb.refIndex, predicted.mvX + mvdX, predicted.mvY + mvdY);
                    }
                }
                h264SetMvState(mb, mvStatesL0, mvStatesL1);
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
                    const H264MacroblockMvState predicted =
                        h264PredictMv(mvStatesL0, slice.picWidthInMbs, mb.address, partitions[i].refIndex);
                    partitions[i].mvX = predicted.mvX + partitions[i].mvdX;
                    partitions[i].mvY = predicted.mvY + partitions[i].mvdY;
                    h264AddMotionVector(mb, 0, partitions[i].refIndex, partitions[i].mvX, partitions[i].mvY);
                }
                h264SetMvState(mb, mvStatesL0, mvStatesL1);
            }
        } else if (isBSlice) {
            const H264BPartitionModes modes = h264BPartitionModes(localMbType);
            if (!modes.supported) {
                mb.parsed = false;
                mb.note = modes.unsupportedMessage;
                currentAddress = mb.address + 1;
                slice.macroblocks.append(mb);
                appendEstimatedRemainder(modes.unsupportedCode, mb.note);
                return;
            }

            std::array<H264PartitionMv, 2> l0Partitions;
            std::array<H264PartitionMv, 2> l1Partitions;
            if (slice.numRefIdxL0ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == H264PredictionList::L0 || modes.modes[i] == H264PredictionList::Bi) {
                        l0Partitions[i].refIndex = readTEMbField(
                            mb,
                            QStringLiteral("ref_idx_l0[%1]").arg(i),
                            slice.numRefIdxL0ActiveMinus1);
                    }
                }
            }
            if (slice.numRefIdxL1ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == H264PredictionList::L1 || modes.modes[i] == H264PredictionList::Bi) {
                        l1Partitions[i].refIndex = readTEMbField(
                            mb,
                            QStringLiteral("ref_idx_l1[%1]").arg(i),
                            slice.numRefIdxL1ActiveMinus1);
                    }
                }
            }

            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == H264PredictionList::L0 || modes.modes[i] == H264PredictionList::Bi) {
                    l0Partitions[i].mvdX = readSEMBField(mb, QStringLiteral("mvd_l0[%1][0]").arg(i));
                    l0Partitions[i].mvdY = readSEMBField(mb, QStringLiteral("mvd_l0[%1][1]").arg(i));
                    const H264MacroblockMvState predicted =
                        h264PredictMv(mvStatesL0, slice.picWidthInMbs, mb.address, l0Partitions[i].refIndex);
                    l0Partitions[i].mvX = predicted.mvX + l0Partitions[i].mvdX;
                    l0Partitions[i].mvY = predicted.mvY + l0Partitions[i].mvdY;
                    h264AddMotionVector(mb, 0, l0Partitions[i].refIndex, l0Partitions[i].mvX, l0Partitions[i].mvY);
                }
            }
            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == H264PredictionList::L1 || modes.modes[i] == H264PredictionList::Bi) {
                    l1Partitions[i].mvdX = readSEMBField(mb, QStringLiteral("mvd_l1[%1][0]").arg(i));
                    l1Partitions[i].mvdY = readSEMBField(mb, QStringLiteral("mvd_l1[%1][1]").arg(i));
                    const H264MacroblockMvState predicted =
                        h264PredictMv(mvStatesL1, slice.picWidthInMbs, mb.address, l1Partitions[i].refIndex);
                    l1Partitions[i].mvX = predicted.mvX + l1Partitions[i].mvdX;
                    l1Partitions[i].mvY = predicted.mvY + l1Partitions[i].mvdY;
                    h264AddMotionVector(mb, 1, l1Partitions[i].refIndex, l1Partitions[i].mvX, l1Partitions[i].mvY);
                }
            }
            h264SetMvState(mb, mvStatesL0, mvStatesL1);
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
