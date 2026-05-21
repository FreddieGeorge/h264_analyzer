#include "core/parser/video/h264/H264Parser.h"

#include "core/parser/video/h264/cabac/H264CabacMacroblockParser.h"
#include "core/parser/video/h264/cavlc/H264CavlcMacroblockParser.h"

void H264Parser::parseSliceData(BitReader &bitReader, SliceInfo &sliceInfo, const PpsInfo &ppsInfo, const SpsInfo &spsInfo) const
{
    H264SliceDataContext context(bitReader, sliceInfo, ppsInfo, spsInfo);
    BitReader &reader = context.reader;
    SliceInfo &slice = context.slice;
    const PpsInfo &pps = context.pps;
    const SpsInfo &sps = context.sps;

    if (pps.entropyCodingModeFlag) {
        h264AppendUnsupportedCabacMacroblocks(context);
        return;
    }

    if (!sps.frameMbsOnlyFlag || pps.numSliceGroupsMinus1 > 0) {
        context.appendEstimatedRemainder(
            QStringLiteral("interlaced_or_fmo_unsupported"),
            QStringLiteral("Interlaced/MBAFF or FMO slice_data parsing is not implemented; macroblock QP is carried forward from the slice header."));
        return;
    }

    while (context.currentAddress < context.totalMacroblocks && reader.moreRbspData() && !reader.hasError()) {
        if (!context.isISlice) {
            const quint32 mbSkipRun = reader.readUE();
            if (reader.hasError()) {
                break;
            }
            for (quint32 i = 0; i < mbSkipRun && context.currentAddress < context.totalMacroblocks; ++i) {
                MacroblockInfo mb;
                mb.address = context.currentAddress++;
                mb.mbType = context.isPSlice ? QStringLiteral("P_Skip") : QStringLiteral("B_Skip/Direct");
                mb.predictionMode = context.isPSlice ? QStringLiteral("Pred_L0") : QStringLiteral("Direct");
                mb.codedBlockPattern = 0;
                mb.codedBlockPatternLuma = 0;
                mb.codedBlockPatternChroma = 0;
                mb.qp = context.currentQp;
                mb.skipped = true;
                mb.parsed = true;
                if (context.isPSlice) {
                    const H264MacroblockMvState predicted =
                        h264PredictMv(context.mvStatesL0, slice.picWidthInMbs, mb.address, 0);
                    h264AddMotionVector(mb, 0, 0, predicted.mvX, predicted.mvY);
                    h264SetMvState(mb, context.mvStatesL0, context.mvStatesL1);
                    mb.note = QStringLiteral("Parsed from mb_skip_run; P_Skip motion vector uses neighboring median prediction.");
                } else {
                    mb.note = QStringLiteral("Parsed from mb_skip_run; B direct motion vector parsing is not implemented.");
                }
                context.coeffStates[mb.address] = H264MacroblockCoeffState {};
                slice.macroblocks.append(mb);
            }
            if (mbSkipRun > 0 && !reader.moreRbspData()) {
                break;
            }
            if (context.currentAddress >= context.totalMacroblocks) {
                break;
            }
        }

        const H264MacroblockParseAction action = h264ParseCavlcMacroblock(context);
        if (action == H264MacroblockParseAction::Stop) {
            return;
        }
        if (action == H264MacroblockParseAction::ReaderError) {
            break;
        }
    }

    if (reader.hasError()) {
        context.appendEstimatedRemainder(
            QStringLiteral("slice_data_truncated"),
            QStringLiteral("slice_data ended unexpectedly; remaining macroblocks are estimated."));
    }

    if (slice.macroblocks.isEmpty()) {
        context.appendEstimatedRemainder(
            QStringLiteral("slice_data_missing"),
            QStringLiteral("No macroblock data could be parsed; QP is carried forward from the slice header."));
    }

    slice.macroblocksParsed = !slice.macroblocks.isEmpty();
}
