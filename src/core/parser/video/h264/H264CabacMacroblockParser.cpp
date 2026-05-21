#include "core/parser/video/h264/H264CabacMacroblockParser.h"

#include "core/parser/video/h264/H264CabacDecoder.h"
#include "core/parser/video/h264/H264CabacSyntaxReader.h"
#include "core/parser/video/h264/H264SliceDataContext.h"

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
}

H264CabacUnsupportedResult h264CabacUnsupportedResult()
{
    return {
        QStringLiteral("cabac_unsupported"),
        QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header.")
    };
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
            39);

    H264CabacSyntaxResult firstSyntax;
    H264CabacMbTypeResult mbType;
    if (context.isISlice || context.isPSlice) {
        mbType = h264ReadCabacMbType(context.reader, decoder, contexts, context);
        firstSyntax.ok = mbType.ok;
        firstSyntax.value = mbType.prefixBin;
        firstSyntax.ctxIdx = mbType.ctxIdx;
        firstSyntax.diagnosticCode = mbType.diagnosticCode;
        firstSyntax.diagnosticMessage = mbType.diagnosticMessage;
    } else {
        firstSyntax = h264ReadCabacMbSkipFlag(context.reader, decoder, contexts, context);
    }
    if (!firstSyntax.ok) {
        context.appendDiagnostic(firstSyntax.diagnosticCode, firstSyntax.diagnosticMessage);
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    if (context.isPSlice && mbType.needsSubMacroblockTypes) {
        const H264CabacSubMbTypesResult subMbTypes =
            h264ReadCabacPSubMbTypes(context.reader, decoder, contexts, context, 4);
        if (!subMbTypes.ok) {
            context.appendDiagnostic(subMbTypes.diagnosticCode, subMbTypes.diagnosticMessage);
            context.appendEstimatedRemainder(cabac.code, cabac.message);
            return;
        }
        if (subMbTypes.complete) {
            const H264CabacRefIdxListResult refIdx =
                h264ReadCabacPSubMbRefIdxL0(context.reader, decoder, contexts, context, subMbTypes.subMbTypes.size());
            if (!refIdx.ok) {
                context.appendDiagnostic(refIdx.diagnosticCode, refIdx.diagnosticMessage);
                context.appendEstimatedRemainder(cabac.code, cabac.message);
                return;
            }
            const QString refIdxNote = refIdx.present
                ? QStringLiteral(" ref_idx_l0 values were also decoded.")
                : QStringLiteral(" ref_idx_l0 syntax was not present because only one L0 reference is active.");
            const H264CabacMvdListResult mvd =
                h264ReadCabacPSubMbMvdL0(context.reader, decoder, contexts, subMbTypes.subMbTypes);
            if (!mvd.ok) {
                context.appendDiagnostic(mvd.diagnosticCode, mvd.diagnosticMessage);
                context.appendEstimatedRemainder(cabac.code, cabac.message);
                return;
            }
            if (!mvd.complete) {
                context.appendDiagnostic(mvd.diagnosticCode, mvd.diagnosticMessage);
                context.appendEstimatedRemainder(cabac.code, cabac.message);
                return;
            }
            context.appendDiagnostic(
                QStringLiteral("cabac_sub_mb_type_parsed"),
                QStringLiteral("CABAC mb_type decoded as P_8x8 and four sub_mb_type values were decoded: %1.%2 Zero mvd_l0 scaffolding decoded %3 partition delta pairs. Subsequent CABAC sub-macroblock motion/residual parsing is not implemented.")
                    .arg(h264CabacPSubMbTypeSummary(subMbTypes.subMbTypes))
                    .arg(refIdxNote)
                    .arg(mvd.mvd.size()));
            context.appendEstimatedRemainder(cabac.code, cabac.message);
            return;
        }

        context.appendDiagnostic(subMbTypes.diagnosticCode, subMbTypes.diagnosticMessage);
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    if ((context.isISlice || context.isPSlice) && mbType.complete) {
        QString mbTypeName;
        if (context.isPSlice) {
            if (mbType.mbType == 0) {
                mbTypeName = QStringLiteral("P_L0_16x16");
            } else if (mbType.mbType == 1) {
                mbTypeName = QStringLiteral("P_L0_L0_16x8");
            } else {
                mbTypeName = QStringLiteral("P_L0_L0_8x16");
            }
        } else {
            mbTypeName = mbType.mbType == 0
                ? QStringLiteral("I_NxN")
                : (mbType.mbType == 25 ? QStringLiteral("I_PCM") : QStringLiteral("I_16x16"));
        }
        const QString unsupportedPart = context.isPSlice
            ? QStringLiteral("motion/residual")
            : QStringLiteral("intra/residual");
        context.appendDiagnostic(
            QStringLiteral("cabac_mb_type_parsed"),
            QStringLiteral("CABAC mb_type decoded as %1 (%2), but subsequent CABAC %3 parsing is not implemented.")
                .arg(mbTypeName)
                .arg(mbType.mbType)
                .arg(unsupportedPart));
        context.appendEstimatedRemainder(cabac.code, cabac.message);
        return;
    }

    context.appendDiagnostic(
        QStringLiteral("cabac_macroblock_syntax_incomplete"),
        QStringLiteral("CABAC %1 prefix was decoded using context %2, but full CABAC macroblock parsing is not implemented.")
            .arg(context.isISlice ? QStringLiteral("mb_type") : QStringLiteral("mb_skip_flag"))
            .arg(firstSyntax.ctxIdx));
    context.appendEstimatedRemainder(cabac.code, cabac.message);
}
