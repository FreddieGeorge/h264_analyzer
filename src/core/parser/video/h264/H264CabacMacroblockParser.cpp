#include "core/parser/video/h264/H264CabacMacroblockParser.h"

#include "core/parser/video/h264/H264CabacDecoder.h"
#include "core/parser/video/h264/H264CabacSyntaxReader.h"
#include "core/parser/video/h264/H264SliceDataContext.h"

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
            27);

    H264CabacSyntaxResult firstSyntax;
    H264CabacMbTypeResult mbType;
    if (context.isISlice) {
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

    if (context.isISlice && mbType.complete) {
        context.appendDiagnostic(
            QStringLiteral("cabac_mb_type_i_nxn_parsed"),
            QStringLiteral("CABAC mb_type decoded as I_NxN, but subsequent CABAC intra/residual parsing is not implemented."));
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
