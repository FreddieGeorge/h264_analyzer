#include "core/parser/video/h264/cavlc/H264CavlcMacroblockParser.h"

#include "core/parser/video/h264/cavlc/H264CavlcMacroblockResidualParser.h"
#include "core/parser/video/h264/H264MacroblockTypes.h"

#include <array>

namespace
{
H264MacroblockParseAction readCavlcMacroblockType(H264SliceDataContext &context,
                                                  MacroblockInfo &mb,
                                                  H264CavlcMacroblockType &type)
{
    type.rawMbType = context.readUEMacroblockField(mb, QStringLiteral("mb_type"));
    if (context.reader.hasError()) {
        return H264MacroblockParseAction::ReaderError;
    }

    type.intra = context.isISlice;
    type.localMbType = static_cast<int>(type.rawMbType);
    if (context.isPSlice && type.localMbType >= 5) {
        type.intra = true;
        type.localMbType -= 5;
    } else if (context.isBSlice && type.localMbType >= 23) {
        type.intra = true;
        type.localMbType -= 23;
    }

    type.intra16x16 = type.intra && type.localMbType >= 1 && type.localMbType <= 24;
    type.iPcm = type.intra && type.localMbType == 25;
    type.p8x8 = context.isPSlice && !type.intra && (type.localMbType == 3 || type.localMbType == 4);

    if (type.intra) {
        mb.mbType = h264IntraMbTypeName(type.localMbType);
        mb.predictionMode = type.intra16x16
            ? QStringLiteral("Intra_16x16")
            : (type.iPcm ? QStringLiteral("I_PCM") : QStringLiteral("Intra_4x4/8x8"));
    } else if (context.isPSlice) {
        mb.mbType = h264PMbTypeName(type.localMbType);
        mb.predictionMode = QStringLiteral("Pred_L0");
    } else if (context.isBSlice) {
        mb.mbType = h264BMbTypeName(type.localMbType);
        mb.predictionMode = QStringLiteral("BiPred");
    } else {
        mb.mbType = QStringLiteral("Unsupported");
        mb.predictionMode = QStringLiteral("unsupported");
        mb.note = QStringLiteral("This slice type is not supported for macroblock parsing.");
        context.currentAddress = mb.address + 1;
        context.slice.macroblocks.append(mb);
        context.appendEstimatedRemainder(QStringLiteral("slice_macroblock_unsupported"), mb.note);
        return H264MacroblockParseAction::Stop;
    }

    if (type.iPcm) {
        mb.note = QStringLiteral("I_PCM macroblock payload is byte aligned raw samples; skipping exact payload is not implemented.");
        context.currentAddress = mb.address + 1;
        context.slice.macroblocks.append(mb);
        context.appendEstimatedRemainder(QStringLiteral("i_pcm_unsupported"), mb.note);
        return H264MacroblockParseAction::Stop;
    }

    return H264MacroblockParseAction::Continue;
}

H264MacroblockParseAction parseCavlcIntraSyntax(H264SliceDataContext &context,
                                                MacroblockInfo &mb,
                                                bool intra16x16,
                                                bool &transform8x8)
{
    if (!intra16x16) {
        transform8x8 = context.pps.transform8x8ModeFlag
            && context.readBitMacroblockField(mb, QStringLiteral("transform_size_8x8_flag"));
        const int predictionBlocks = transform8x8 ? 4 : 16;
        for (int i = 0; i < predictionBlocks; ++i) {
            if (!context.reader.readBit()) {
                context.reader.readBits(3);
            }
        }
    }

    context.readUEMacroblockField(mb, QStringLiteral("intra_chroma_pred_mode"));
    return context.reader.hasError()
        ? H264MacroblockParseAction::ReaderError
        : H264MacroblockParseAction::Continue;
}

H264MacroblockParseAction parseCavlcPSliceMotion(H264SliceDataContext &context,
                                                 MacroblockInfo &mb,
                                                 int localMbType,
                                                 bool p8x8)
{
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
            subMb.type = static_cast<int>(context.readUEMacroblockField(
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
                context.currentAddress = mb.address + 1;
                context.slice.macroblocks.append(mb);
                context.appendEstimatedRemainder(QStringLiteral("p8x8_sub_macroblock_type_unsupported"), mb.note);
                return H264MacroblockParseAction::Stop;
            }
        }

        if (context.slice.numRefIdxL0ActiveMinus1 > 0 && localMbType != 4) {
            for (int subIndex = 0; subIndex < static_cast<int>(subMacroblocks.size()); ++subIndex) {
                SubMacroblock &subMb = subMacroblocks[subIndex];
                subMb.refIndex = context.readTEMacroblockField(
                    mb,
                    QStringLiteral("ref_idx_l0[%1]").arg(subIndex),
                    context.slice.numRefIdxL0ActiveMinus1);
            }
        }

        for (int subIndex = 0; subIndex < static_cast<int>(subMacroblocks.size()); ++subIndex) {
            const SubMacroblock &subMb = subMacroblocks[subIndex];
            for (int part = 0; part < subMb.partitionCount; ++part) {
                const int mvdX = context.readSEMacroblockField(
                    mb,
                    QStringLiteral("mvd_l0[%1][%2][0]").arg(subIndex).arg(part));
                const int mvdY = context.readSEMacroblockField(
                    mb,
                    QStringLiteral("mvd_l0[%1][%2][1]").arg(subIndex).arg(part));
                const H264MacroblockMvState predicted =
                    h264PredictMv(context.mvStatesL0, context.slice.picWidthInMbs, mb.address, subMb.refIndex);
                h264AddMotionVector(mb, 0, subMb.refIndex, predicted.mvX + mvdX, predicted.mvY + mvdY);
            }
        }
        h264SetMvState(mb, context.mvStatesL0, context.mvStatesL1);
        mb.predictionMode = QStringLiteral("Pred_L0 sub-macroblock");
    }

    if (!p8x8 && context.slice.numRefIdxL0ActiveMinus1 > 0) {
        for (int i = 0; i < partitionCount; ++i) {
            partitions[i].refIndex = context.readTEMacroblockField(
                mb,
                QStringLiteral("ref_idx_l0[%1]").arg(i),
                context.slice.numRefIdxL0ActiveMinus1);
        }
    }
    if (!p8x8) {
        for (int i = 0; i < partitionCount; ++i) {
            partitions[i].mvdX = context.readSEMacroblockField(mb, QStringLiteral("mvd_l0[%1][0]").arg(i));
            partitions[i].mvdY = context.readSEMacroblockField(mb, QStringLiteral("mvd_l0[%1][1]").arg(i));
            const H264MacroblockMvState predicted =
                h264PredictMv(context.mvStatesL0, context.slice.picWidthInMbs, mb.address, partitions[i].refIndex);
            partitions[i].mvX = predicted.mvX + partitions[i].mvdX;
            partitions[i].mvY = predicted.mvY + partitions[i].mvdY;
            h264AddMotionVector(mb, 0, partitions[i].refIndex, partitions[i].mvX, partitions[i].mvY);
        }
        h264SetMvState(mb, context.mvStatesL0, context.mvStatesL1);
    }

    return context.reader.hasError()
        ? H264MacroblockParseAction::ReaderError
        : H264MacroblockParseAction::Continue;
}

H264MacroblockParseAction parseCavlcBSliceMotion(H264SliceDataContext &context,
                                                 MacroblockInfo &mb,
                                                 int localMbType)
{
    const H264BPartitionModes modes = h264BPartitionModes(localMbType);
    if (!modes.supported) {
        mb.parsed = false;
        mb.note = modes.unsupportedMessage;
        context.currentAddress = mb.address + 1;
        context.slice.macroblocks.append(mb);
        context.appendEstimatedRemainder(modes.unsupportedCode, mb.note);
        return H264MacroblockParseAction::Stop;
    }

    std::array<H264PartitionMv, 2> l0Partitions;
    std::array<H264PartitionMv, 2> l1Partitions;
    if (context.slice.numRefIdxL0ActiveMinus1 > 0) {
        for (int i = 0; i < modes.partitionCount; ++i) {
            if (modes.modes[i] == H264PredictionList::L0 || modes.modes[i] == H264PredictionList::Bi) {
                l0Partitions[i].refIndex = context.readTEMacroblockField(
                    mb,
                    QStringLiteral("ref_idx_l0[%1]").arg(i),
                    context.slice.numRefIdxL0ActiveMinus1);
            }
        }
    }
    if (context.slice.numRefIdxL1ActiveMinus1 > 0) {
        for (int i = 0; i < modes.partitionCount; ++i) {
            if (modes.modes[i] == H264PredictionList::L1 || modes.modes[i] == H264PredictionList::Bi) {
                l1Partitions[i].refIndex = context.readTEMacroblockField(
                    mb,
                    QStringLiteral("ref_idx_l1[%1]").arg(i),
                    context.slice.numRefIdxL1ActiveMinus1);
            }
        }
    }

    for (int i = 0; i < modes.partitionCount; ++i) {
        if (modes.modes[i] == H264PredictionList::L0 || modes.modes[i] == H264PredictionList::Bi) {
            l0Partitions[i].mvdX = context.readSEMacroblockField(mb, QStringLiteral("mvd_l0[%1][0]").arg(i));
            l0Partitions[i].mvdY = context.readSEMacroblockField(mb, QStringLiteral("mvd_l0[%1][1]").arg(i));
            const H264MacroblockMvState predicted =
                h264PredictMv(context.mvStatesL0, context.slice.picWidthInMbs, mb.address, l0Partitions[i].refIndex);
            l0Partitions[i].mvX = predicted.mvX + l0Partitions[i].mvdX;
            l0Partitions[i].mvY = predicted.mvY + l0Partitions[i].mvdY;
            h264AddMotionVector(mb, 0, l0Partitions[i].refIndex, l0Partitions[i].mvX, l0Partitions[i].mvY);
        }
    }
    for (int i = 0; i < modes.partitionCount; ++i) {
        if (modes.modes[i] == H264PredictionList::L1 || modes.modes[i] == H264PredictionList::Bi) {
            l1Partitions[i].mvdX = context.readSEMacroblockField(mb, QStringLiteral("mvd_l1[%1][0]").arg(i));
            l1Partitions[i].mvdY = context.readSEMacroblockField(mb, QStringLiteral("mvd_l1[%1][1]").arg(i));
            const H264MacroblockMvState predicted =
                h264PredictMv(context.mvStatesL1, context.slice.picWidthInMbs, mb.address, l1Partitions[i].refIndex);
            l1Partitions[i].mvX = predicted.mvX + l1Partitions[i].mvdX;
            l1Partitions[i].mvY = predicted.mvY + l1Partitions[i].mvdY;
            h264AddMotionVector(mb, 1, l1Partitions[i].refIndex, l1Partitions[i].mvX, l1Partitions[i].mvY);
        }
    }
    h264SetMvState(mb, context.mvStatesL0, context.mvStatesL1);

    return context.reader.hasError()
        ? H264MacroblockParseAction::ReaderError
        : H264MacroblockParseAction::Continue;
}

H264MacroblockParseAction parseCavlcCodedBlockPatternAndResidual(H264SliceDataContext &context,
                                                                 MacroblockInfo &mb,
                                                                 const H264CavlcMacroblockType &type,
                                                                 bool transform8x8)
{
    BitReader &reader = context.reader;
    SliceInfo &slice = context.slice;
    const PpsInfo &pps = context.pps;
    int &currentAddress = context.currentAddress;
    int &currentQp = context.currentQp;
    QVector<H264MacroblockCoeffState> &coeffStates = context.coeffStates;

    if (reader.hasError()) {
        return H264MacroblockParseAction::ReaderError;
    }

    if (type.intra16x16) {
        const int typeOffset = type.localMbType - 1;
        mb.codedBlockPatternChroma = (typeOffset / 4) % 3;
        mb.codedBlockPatternLuma = (typeOffset / 12) != 0 ? 15 : 0;
        mb.codedBlockPattern = mb.codedBlockPatternLuma | (mb.codedBlockPatternChroma << 4);
    } else {
        const quint32 codedBlockPatternCodeNum =
            context.readUEMacroblockField(mb, QStringLiteral("coded_block_pattern"));
        mb.codedBlockPattern =
            h264CodedBlockPatternFromCodeNum(codedBlockPatternCodeNum, type.intra, context.chromaArrayType);
        mb.codedBlockPatternLuma = mb.codedBlockPattern & 0x0f;
        mb.codedBlockPatternChroma = (mb.codedBlockPattern >> 4) & 0x03;
        if (mb.codedBlockPatternLuma > 0 && pps.transform8x8ModeFlag && !type.intra) {
            transform8x8 = context.readBitMacroblockField(mb, QStringLiteral("transform_size_8x8_flag"));
        }
    }

    const bool hasResidual = type.intra16x16 || mb.codedBlockPatternLuma > 0 || mb.codedBlockPatternChroma > 0;
    H264MacroblockCoeffState coeffState;
    if (hasResidual) {
        mb.mbQpDelta = context.readSEMacroblockField(mb, QStringLiteral("mb_qp_delta"));
        currentQp = (currentQp + mb.mbQpDelta + 52) % 52;
        mb.qp = currentQp;
        if (!h264ParseCavlcMacroblockResidual(context, mb, type, transform8x8, coeffState) || reader.hasError()) {
            mb.note = QStringLiteral("Parsed macroblock header and mb_qp_delta, but residual CAVLC parsing stopped on malformed or unsupported residual data.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            context.appendEstimatedRemainder(
                QStringLiteral("cavlc_residual_parse_failed"),
                QStringLiteral("CAVLC residual data could not be fully parsed; remaining macroblocks are estimated."));
            slice.macroblocksParsed = !slice.macroblocks.isEmpty();
            return H264MacroblockParseAction::Stop;
        }
        mb.residualParsed = true;
        coeffStates[mb.address] = coeffState;
        mb.note = QStringLiteral("Parsed macroblock header, mb_qp_delta, and CAVLC residual data.");
        currentAddress = mb.address + 1;
        slice.macroblocks.append(mb);
        return H264MacroblockParseAction::Continue;
    }

    mb.qp = currentQp;
    coeffStates[mb.address] = coeffState;
    mb.note = QStringLiteral("Parsed macroblock header; no residual data present.");
    currentAddress = mb.address + 1;
    slice.macroblocks.append(mb);
    return H264MacroblockParseAction::Continue;
}
}

H264MacroblockParseAction h264ParseCavlcMacroblock(H264SliceDataContext &context)
{
    int &currentAddress = context.currentAddress;
    int &currentQp = context.currentQp;

    MacroblockInfo mb;
    mb.address = currentAddress;
    mb.qp = currentQp;
    mb.parsed = true;

    H264CavlcMacroblockType type;
    H264MacroblockParseAction action = readCavlcMacroblockType(context, mb, type);
    if (action != H264MacroblockParseAction::Continue) {
        return action;
    }

    bool transform8x8 = false;

    if (type.intra) {
        action = parseCavlcIntraSyntax(context, mb, type.intra16x16, transform8x8);
        if (action != H264MacroblockParseAction::Continue) {
            return action;
        }
    } else if (context.isPSlice) {
        action = parseCavlcPSliceMotion(context, mb, type.localMbType, type.p8x8);
        if (action != H264MacroblockParseAction::Continue) {
            return action;
        }
    } else if (context.isBSlice) {
        action = parseCavlcBSliceMotion(context, mb, type.localMbType);
        if (action != H264MacroblockParseAction::Continue) {
            return action;
        }
    }

    return parseCavlcCodedBlockPatternAndResidual(context, mb, type, transform8x8);
}
