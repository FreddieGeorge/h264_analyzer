#include "core/parser/video/h264/cabac/H264CabacMacroblockParser.h"

#include "core/parser/video/h264/H264SliceDataContext.h"
#include "core/parser/video/h264/cabac/H264CabacDecoder.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxReader.h"

#include <QStringList>
#include <QVector>

namespace
{
QString h264CabacPSubMbTypeName(int subMbType)
{
    switch (subMbType) {
    case 0:
        return QStringLiteral("P_L0_8x8");
    case 1:
        return QStringLiteral("P_L0_8x4");
    case 2:
        return QStringLiteral("P_L0_4x8");
    default:
        return QStringLiteral("P_L0_4x4");
    }
}

QString h264CabacPSubMbTypeSummary(const QVector<int> &subMbTypes)
{
    QStringList parts;
    for (int i = 0; i < subMbTypes.size(); ++i) {
        parts.append(QStringLiteral("[%1]=%2 (%3)")
                         .arg(i)
                         .arg(h264CabacPSubMbTypeName(subMbTypes.at(i)))
                         .arg(subMbTypes.at(i)));
    }
    return parts.join(QStringLiteral(", "));
}

QString h264CabacMbTypeName(const H264SliceDataContext &context, int mbType)
{
    if (context.isPSlice) {
        if (mbType == 0) {
            return QStringLiteral("P_L0_16x16");
        }
        if (mbType == 1) {
            return QStringLiteral("P_L0_L0_16x8");
        }
        if (mbType == 2) {
            return QStringLiteral("P_L0_L0_8x16");
        }
        return QStringLiteral("P_8x8");
    }

    if (mbType == 0) {
        return QStringLiteral("I_NxN");
    }
    if (mbType == 25) {
        return QStringLiteral("I_PCM");
    }
    return QStringLiteral("I_16x16");
}

void copyDiagnostic(H264CabacMacroblockSyntaxResult &result,
                    const QString &code,
                    const QString &message)
{
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
}

int h264CabacPSubMbPartitionCount(int subMbType)
{
    switch (subMbType) {
    case 0:
        return 1;
    case 1:
    case 2:
        return 2;
    case 3:
        return 4;
    default:
        return 0;
    }
}

void appendCabacP8x8ZeroMvdMotionVectors(H264SliceDataContext &context,
                                         MacroblockInfo &mb,
                                         const H264CabacMacroblockSyntaxResult &syntax)
{
    int mvdIndex = 0;
    for (int subIndex = 0; subIndex < syntax.subMbTypes.size(); ++subIndex) {
        const int refIdx = subIndex < syntax.refIdxL0.size() ? syntax.refIdxL0.at(subIndex) : 0;
        const int partitionCount = h264CabacPSubMbPartitionCount(syntax.subMbTypes.at(subIndex));
        for (int partition = 0; partition < partitionCount && mvdIndex < syntax.mvdL0.size(); ++partition) {
            const H264CabacMvdPair mvd = syntax.mvdL0.at(mvdIndex++);
            const H264MacroblockMvState predicted =
                h264PredictMv(context.mvStatesL0, context.slice.picWidthInMbs, mb.address, refIdx);
            h264AddMotionVector(mb, 0, refIdx, predicted.mvX + mvd.x, predicted.mvY + mvd.y);
        }
    }
    h264SetMvState(mb, context.mvStatesL0, context.mvStatesL1);
}

}

H264CabacUnsupportedResult h264CabacUnsupportedResult()
{
    return {
        QStringLiteral("cabac_unsupported"),
        QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header.")
    };
}

H264CabacMacroblockSyntaxResult h264ReadCabacMacroblockSyntax(H264SliceDataContext &context,
                                                              H264CabacDecoder &decoder,
                                                              H264CabacContextModelSet &contexts)
{
    H264CabacMacroblockSyntaxResult result;
    H264CabacSyntaxResult firstSyntax;
    H264CabacMbTypeResult mbType;
    if (context.isISlice || context.isPSlice) {
        mbType = h264ReadCabacMbType(context.reader, decoder, contexts, context);
        firstSyntax.ok = mbType.ok;
        firstSyntax.value = mbType.prefixBin;
        firstSyntax.ctxIdx = mbType.ctxIdx;
        firstSyntax.diagnosticCode = mbType.diagnosticCode;
        firstSyntax.diagnosticMessage = mbType.diagnosticMessage;
        result.mbType = mbType.mbType;
        result.firstSyntaxName = QStringLiteral("mb_type");
    } else {
        firstSyntax = h264ReadCabacMbSkipFlag(context.reader, decoder, contexts, context);
        result.firstSyntaxName = QStringLiteral("mb_skip_flag");
    }
    result.ok = firstSyntax.ok;
    result.firstSyntaxCtxIdx = firstSyntax.ctxIdx;
    if (!firstSyntax.ok) {
        copyDiagnostic(result, firstSyntax.diagnosticCode, firstSyntax.diagnosticMessage);
        return result;
    }

    if (context.isPSlice && mbType.needsSubMacroblockTypes) {
        const H264CabacSubMbTypesResult subMbTypes =
            h264ReadCabacPSubMbTypes(context.reader, decoder, contexts, context, 4);
        if (!subMbTypes.ok) {
            result.ok = false;
            copyDiagnostic(result, subMbTypes.diagnosticCode, subMbTypes.diagnosticMessage);
            return result;
        }
        if (subMbTypes.complete) {
            result.subMbTypes = subMbTypes.subMbTypes;
            const H264CabacRefIdxListResult refIdx =
                h264ReadCabacPSubMbRefIdxL0(context.reader, decoder, contexts, context, subMbTypes.subMbTypes.size());
            if (!refIdx.ok) {
                result.ok = false;
                copyDiagnostic(result, refIdx.diagnosticCode, refIdx.diagnosticMessage);
                return result;
            }
            result.refIdxL0Present = refIdx.present;
            result.refIdxL0 = refIdx.refIdx;
            if (!refIdx.diagnosticCode.isEmpty()) {
                copyDiagnostic(result, refIdx.diagnosticCode, refIdx.diagnosticMessage);
                return result;
            }
            const H264CabacMvdListResult mvd =
                h264ReadCabacPSubMbMvdL0(context.reader, decoder, contexts, subMbTypes.subMbTypes);
            if (!mvd.ok) {
                result.ok = false;
                copyDiagnostic(result, mvd.diagnosticCode, mvd.diagnosticMessage);
                return result;
            }
            result.mvdL0 = mvd.mvd;
            if (!mvd.complete) {
                copyDiagnostic(result, mvd.diagnosticCode, mvd.diagnosticMessage);
                return result;
            }
            result.parsedSubMacroblockSyntax = true;
            const H264CabacCodedBlockPatternResult cbp =
                h264ReadCabacCodedBlockPatternZero(context.reader, decoder, contexts, context);
            if (!cbp.ok) {
                result.ok = false;
                copyDiagnostic(result, cbp.diagnosticCode, cbp.diagnosticMessage);
                return result;
            }
            result.codedBlockPattern = cbp.codedBlockPattern;
            result.codedBlockPatternLuma = cbp.codedBlockPatternLuma;
            result.codedBlockPatternChroma = cbp.codedBlockPatternChroma;
            if (!cbp.complete) {
                copyDiagnostic(result, cbp.diagnosticCode, cbp.diagnosticMessage);
                return result;
            }
            result.parsedCodedBlockPatternZero = true;
            result.complete = true;
            return result;
        }

        copyDiagnostic(result, subMbTypes.diagnosticCode, subMbTypes.diagnosticMessage);
        return result;
    }

    if ((context.isISlice || context.isPSlice) && mbType.complete) {
        result.complete = true;
        return result;
    }

    copyDiagnostic(
        result,
        QStringLiteral("cabac_macroblock_syntax_incomplete"),
        QStringLiteral("CABAC %1 prefix was decoded using context %2, but full CABAC macroblock parsing is not implemented.")
            .arg(result.firstSyntaxName)
            .arg(result.firstSyntaxCtxIdx));
    return result;
}

bool h264AppendCabacMacroblockSyntaxSkeleton(H264SliceDataContext &context,
                                             const H264CabacMacroblockSyntaxResult &syntax)
{
    if (!syntax.parsedSubMacroblockSyntax || !syntax.parsedCodedBlockPatternZero || syntax.mbType != 3 || !context.isPSlice) {
        return false;
    }

    MacroblockInfo mb;
    mb.address = context.currentAddress;
    mb.mbType = h264CabacMbTypeName(context, syntax.mbType);
    mb.predictionMode = QStringLiteral("Pred_L0 sub-macroblock");
    mb.codedBlockPattern = syntax.codedBlockPattern;
    mb.codedBlockPatternLuma = syntax.codedBlockPatternLuma;
    mb.codedBlockPatternChroma = syntax.codedBlockPatternChroma;
    mb.qp = context.currentQp;
    mb.residualParsed = true;
    mb.parsed = true;
    appendCabacP8x8ZeroMvdMotionVectors(context, mb, syntax);
    mb.note = QStringLiteral("CABAC P_8x8 motion syntax parsed with zero-MVD L0 vectors; coded_block_pattern is zero, so no residual blocks are present.");

    context.coeffStates[mb.address] = H264MacroblockCoeffState {};
    context.currentAddress = mb.address + 1;
    context.slice.macroblocks.append(mb);
    return true;
}

void h264AppendUnsupportedCabacMacroblocks(H264SliceDataContext &context)
{
    const H264CabacUnsupportedResult cabac = h264CabacUnsupportedResult();
    H264CabacDecoder decoder;
    if (!decoder.initialize(context.reader)) {
        context.appendDiagnostic(
            QStringLiteral("cabac_decoder_init_failed"),
            QStringLiteral("CABAC arithmetic decoder could not be initialized from slice_data."));
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(
            context.isISlice,
            context.slice.cabacInitIdc,
            context.currentQp,
            77);

    const H264CabacMacroblockSyntaxResult syntax =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    if (!syntax.ok) {
        context.appendDiagnostic(syntax.diagnosticCode, syntax.diagnosticMessage);
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    if (syntax.parsedSubMacroblockSyntax) {
        if (!syntax.parsedCodedBlockPatternZero) {
            context.appendDiagnostic(syntax.diagnosticCode, syntax.diagnosticMessage);
            context.appendEstimatedRemainder(cabac.code, cabac.message);
            return;
        }
        h264AppendCabacMacroblockSyntaxSkeleton(context, syntax);
        const QString refIdxNote = syntax.refIdxL0Present
            ? QStringLiteral(" ref_idx_l0 values were also decoded.")
            : QStringLiteral(" ref_idx_l0 syntax was not present because only one L0 reference is active.");
        context.appendDiagnostic(
            QStringLiteral("cabac_sub_mb_type_parsed"),
            QStringLiteral("CABAC mb_type decoded as P_8x8 and four sub_mb_type values were decoded: %1.%2 Zero mvd_l0 decoded %3 partition delta pairs, and coded_block_pattern is zero, so no residual blocks are present. Subsequent CABAC macroblocks in this slice are not implemented.")
                .arg(h264CabacPSubMbTypeSummary(syntax.subMbTypes))
                .arg(refIdxNote)
                .arg(syntax.mvdL0.size()));
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    if ((context.isISlice || context.isPSlice) && syntax.complete) {
        const QString unsupportedPart = context.isPSlice
            ? QStringLiteral("motion/residual")
            : QStringLiteral("intra/residual");
        context.appendDiagnostic(
            QStringLiteral("cabac_mb_type_parsed"),
            QStringLiteral("CABAC mb_type decoded as %1 (%2), but subsequent CABAC %3 parsing is not implemented.")
                .arg(h264CabacMbTypeName(context, syntax.mbType))
                .arg(syntax.mbType)
                .arg(unsupportedPart));
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    context.appendDiagnostic(syntax.diagnosticCode, syntax.diagnosticMessage);
    context.appendEstimatedRemainder(cabac.code, cabac.message);
}
