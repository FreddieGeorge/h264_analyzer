#include "core/parser/video/h264/H264CabacSyntaxReader.h"

#include "core/parser/video/h264/H264CabacDecoder.h"
#include "core/parser/video/h264/H264SliceDataContext.h"

namespace
{
const MacroblockInfo *macroblockAtAddress(const SliceInfo &slice, int address)
{
    for (const MacroblockInfo &mb : slice.macroblocks) {
        if (mb.address == address) {
            return &mb;
        }
    }
    return nullptr;
}

int cabacMbSkipFlagCtxIdx(const H264SliceDataContext &context)
{
    if (context.isPSlice) {
        int ctxIdxInc = 0;
        const int mbX = context.currentAddress % context.slice.picWidthInMbs;
        const int mbY = context.currentAddress / context.slice.picWidthInMbs;
        if (mbX > 0) {
            const MacroblockInfo *left = macroblockAtAddress(context.slice, context.currentAddress - 1);
            if (left != nullptr && !left->skipped) {
                ++ctxIdxInc;
            }
        }
        if (mbY > 0) {
            const MacroblockInfo *top =
                macroblockAtAddress(context.slice, context.currentAddress - context.slice.picWidthInMbs);
            if (top != nullptr && !top->skipped) {
                ++ctxIdxInc;
            }
        }
        return 11 + ctxIdxInc;
    }

    if (context.isBSlice) {
        return 24;
    }

    return -1;
}

int cabacMbTypePrefixCtxIdx(const H264SliceDataContext &context)
{
    if (context.isISlice) {
        return 3;
    }
    if (context.isPSlice) {
        return 14;
    }
    if (context.isBSlice) {
        return 27;
    }
    return -1;
}

H264CabacSyntaxResult failedResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacSyntaxResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacSyntaxResult decodeContextBin(BitReader &reader,
                                       H264CabacDecoder &decoder,
                                       H264CabacContextModelSet &contexts,
                                       int ctxIdx,
                                       const QString &syntaxName)
{
    if (ctxIdx < 0) {
        return failedResult(
            QStringLiteral("cabac_slice_type_unsupported"),
            QStringLiteral("CABAC %1 is not supported for this slice type.").arg(syntaxName),
            ctxIdx);
    }
    if (!contexts.isInitialized(ctxIdx)) {
        return failedResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context %1 for %2 is not initialized in the covered context table.")
                .arg(ctxIdx)
                .arg(syntaxName),
            ctxIdx);
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading %1.").arg(syntaxName),
            ctxIdx);
    }

    H264CabacSyntaxResult result;
    result.ok = true;
    result.value = bin;
    result.ctxIdx = ctxIdx;
    return result;
}
}

H264CabacSyntaxResult h264ReadCabacMbSkipFlag(BitReader &reader,
                                              H264CabacDecoder &decoder,
                                              H264CabacContextModelSet &contexts,
                                              const H264SliceDataContext &sliceContext)
{
    return decodeContextBin(
        reader,
        decoder,
        contexts,
        cabacMbSkipFlagCtxIdx(sliceContext),
        QStringLiteral("mb_skip_flag"));
}

H264CabacSyntaxResult h264ReadCabacMbTypePrefix(BitReader &reader,
                                                H264CabacDecoder &decoder,
                                                H264CabacContextModelSet &contexts,
                                                const H264SliceDataContext &sliceContext)
{
    return decodeContextBin(
        reader,
        decoder,
        contexts,
        cabacMbTypePrefixCtxIdx(sliceContext),
        QStringLiteral("mb_type"));
}

H264CabacMbTypeResult h264ReadCabacMbType(BitReader &reader,
                                          H264CabacDecoder &decoder,
                                          H264CabacContextModelSet &contexts,
                                          const H264SliceDataContext &sliceContext)
{
    const H264CabacSyntaxResult prefix =
        h264ReadCabacMbTypePrefix(reader, decoder, contexts, sliceContext);

    H264CabacMbTypeResult result;
    result.ok = prefix.ok;
    result.prefixBin = prefix.value;
    result.ctxIdx = prefix.ctxIdx;
    result.diagnosticCode = prefix.diagnosticCode;
    result.diagnosticMessage = prefix.diagnosticMessage;
    if (!prefix.ok) {
        return result;
    }

    if (sliceContext.isISlice && prefix.value == 0) {
        result.complete = true;
        result.mbType = 0;
        return result;
    }

    result.diagnosticCode = QStringLiteral("cabac_mb_type_incomplete");
    result.diagnosticMessage =
        sliceContext.isISlice
        ? QStringLiteral("CABAC mb_type prefix was decoded, but the I_16x16/I_PCM decision tree is not implemented.")
        : QStringLiteral("CABAC mb_type prefix was decoded, but this slice-type decision tree is not implemented.");
    return result;
}
