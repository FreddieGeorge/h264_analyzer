#include "core/parser/video/h264/cabac/H264CabacMacroblockSyntaxReader.h"

#include "core/parser/video/h264/H264SliceDataContext.h"
#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

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

bool lumaCbpBitIsSet(const H264SliceDataContext &context,
                     int address,
                     int luma8x8,
                     int currentCodedBlockPatternLuma)
{
    if (address == context.currentAddress) {
        return ((currentCodedBlockPatternLuma >> luma8x8) & 0x01) != 0;
    }

    const MacroblockInfo *mb = macroblockAtAddress(context.slice, address);
    return mb != nullptr && ((mb->codedBlockPatternLuma >> luma8x8) & 0x01) != 0;
}

int cbpLumaCondTermFlag(const H264SliceDataContext &context,
                        int address,
                        int luma8x8,
                        int currentCodedBlockPatternLuma)
{
    if (address < 0) {
        return 0;
    }
    const MacroblockInfo *mb = address == context.currentAddress
        ? nullptr
        : macroblockAtAddress(context.slice, address);
    if (address != context.currentAddress && mb == nullptr) {
        return 0;
    }
    if (mb != nullptr && mb->mbType == QStringLiteral("I_PCM")) {
        return 0;
    }
    if (mb == nullptr || !mb->skipped) {
        return lumaCbpBitIsSet(context, address, luma8x8, currentCodedBlockPatternLuma) ? 0 : 1;
    }
    return 1;
}

int cabacCbpLumaCtxIdx(const H264SliceDataContext &context,
                       int luma8x8,
                       int currentCodedBlockPatternLuma)
{
    const int mbX = context.currentAddress % context.slice.picWidthInMbs;
    const int mbY = context.currentAddress / context.slice.picWidthInMbs;
    const int x8 = luma8x8 % 2;
    const int y8 = luma8x8 / 2;

    int leftAddress = -1;
    int leftLuma8x8 = -1;
    if (x8 > 0) {
        leftAddress = context.currentAddress;
        leftLuma8x8 = luma8x8 - 1;
    } else if (mbX > 0) {
        leftAddress = context.currentAddress - 1;
        leftLuma8x8 = luma8x8 + 1;
    }

    int topAddress = -1;
    int topLuma8x8 = -1;
    if (y8 > 0) {
        topAddress = context.currentAddress;
        topLuma8x8 = luma8x8 - 2;
    } else if (mbY > 0) {
        topAddress = context.currentAddress - context.slice.picWidthInMbs;
        topLuma8x8 = luma8x8 + 2;
    }

    const int condTermFlagA =
        cbpLumaCondTermFlag(context, leftAddress, leftLuma8x8, currentCodedBlockPatternLuma);
    const int condTermFlagB =
        cbpLumaCondTermFlag(context, topAddress, topLuma8x8, currentCodedBlockPatternLuma);
    return 73 + condTermFlagA + 2 * condTermFlagB;
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

    if (sliceContext.isPSlice) {
        if (prefix.value != 0) {
            result.diagnosticCode = QStringLiteral("cabac_mb_type_incomplete");
            result.diagnosticMessage =
                QStringLiteral("CABAC P-slice intra mb_type fallback is not implemented.");
            return result;
        }

        int bin = 0;
        if (!decoder.decodeBin(reader, contexts, 15, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading P-slice mb_type partition branch.");
            return result;
        }
        if (bin == 0) {
            if (!decoder.decodeBin(reader, contexts, 16, &bin)) {
                result.ok = false;
                result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                result.diagnosticMessage =
                    QStringLiteral("CABAC bin decoding failed while reading P_L0_16x16/P_8x8 selector.");
                return result;
            }
            if (bin != 0) {
                result.complete = true;
                result.needsSubMacroblockTypes = true;
                result.mbType = 3;
                return result;
            }
            result.complete = true;
            result.mbType = 0;
            return result;
        }

        if (!decoder.decodeBin(reader, contexts, 17, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading P_L0_L0_16x8/P_L0_L0_8x16 selector.");
            return result;
        }
        result.complete = true;
        result.mbType = bin == 0 ? 2 : 1;
        return result;
    }

    if (sliceContext.isISlice) {
        int terminateBin = 0;
        if (!decoder.decodeTerminate(reader, &terminateBin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC terminate bin decoding failed while reading I-slice mb_type.");
            return result;
        }
        if (terminateBin != 0) {
            result.complete = true;
            result.mbType = 25;
            return result;
        }

        int bin = 0;
        result.mbType = 1;
        if (!decoder.decodeBin(reader, contexts, 6, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading I_16x16 coded-block-pattern luma.");
            return result;
        }
        result.mbType += 12 * bin;

        if (!decoder.decodeBin(reader, contexts, 7, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading I_16x16 coded-block-pattern chroma.");
            return result;
        }
        if (bin != 0) {
            result.mbType += 4;
            if (!decoder.decodeBin(reader, contexts, 8, &bin)) {
                result.ok = false;
                result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                result.diagnosticMessage =
                    QStringLiteral("CABAC bin decoding failed while reading I_16x16 coded-block-pattern chroma detail.");
                return result;
            }
            result.mbType += 4 * bin;
        }

        if (!decoder.decodeBin(reader, contexts, 9, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading I_16x16 intra prediction mode.");
            return result;
        }
        result.mbType += 2 * bin;

        if (!decoder.decodeBin(reader, contexts, 10, &bin)) {
            result.ok = false;
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading I_16x16 intra prediction mode.");
            return result;
        }
        result.mbType += bin;
        result.complete = true;
        return result;
    }

    result.diagnosticCode = QStringLiteral("cabac_mb_type_incomplete");
    result.diagnosticMessage =
        sliceContext.isISlice
        ? QStringLiteral("CABAC mb_type prefix was decoded, but the I-slice decision tree is not implemented.")
        : QStringLiteral("CABAC mb_type prefix was decoded, but this slice-type decision tree is not implemented.");
    return result;
}

H264CabacCodedBlockPatternResult h264ReadCabacCodedBlockPatternZero(BitReader &reader,
                                                                    H264CabacDecoder &decoder,
                                                                    H264CabacContextModelSet &contexts,
                                                                    const H264SliceDataContext &sliceContext)
{
    H264CabacCodedBlockPatternResult result;
    result.firstCtxIdx = 73;
    result.codedBlockPatternLuma = 0;
    result.codedBlockPatternChroma = 0;
    result.codedBlockPattern = 0;

    if (!sliceContext.isPSlice) {
        result.diagnosticCode = QStringLiteral("cabac_slice_type_unsupported");
        result.diagnosticMessage =
            QStringLiteral("CABAC coded_block_pattern zero reader currently only supports P-slice inter macroblocks.");
        return result;
    }

    for (int luma8x8 = 0; luma8x8 < 4; ++luma8x8) {
        const int ctxIdx =
            cabacCbpLumaCtxIdx(sliceContext, luma8x8, result.codedBlockPatternLuma);
        const H264CabacSyntaxResult bin = decodeContextBin(
            reader,
            decoder,
            contexts,
            ctxIdx,
            QStringLiteral("coded_block_pattern_luma"));
        if (!bin.ok) {
            result.diagnosticCode = bin.diagnosticCode;
            result.diagnosticMessage = bin.diagnosticMessage;
            return result;
        }
        result.ok = true;
        if (bin.value != 0) {
            result.codedBlockPatternLuma |= (1 << luma8x8);
            result.codedBlockPattern = result.codedBlockPatternLuma;
            result.diagnosticCode = QStringLiteral("cabac_cbp_incomplete");
            result.diagnosticMessage =
                QStringLiteral("CABAC non-zero coded_block_pattern luma is not implemented.");
            return result;
        }
    }

    if (sliceContext.chromaArrayType == 0) {
        result.complete = true;
        return result;
    }

    const H264CabacSyntaxResult chromaPresent = decodeContextBin(
        reader,
        decoder,
        contexts,
        77,
        QStringLiteral("coded_block_pattern_chroma"));
    if (!chromaPresent.ok) {
        result.ok = false;
        result.diagnosticCode = chromaPresent.diagnosticCode;
        result.diagnosticMessage = chromaPresent.diagnosticMessage;
        return result;
    }
    result.ok = true;
    if (chromaPresent.value != 0) {
        result.codedBlockPatternChroma = 1;
        result.codedBlockPattern = result.codedBlockPatternLuma | (result.codedBlockPatternChroma << 4);
        result.diagnosticCode = QStringLiteral("cabac_cbp_incomplete");
        result.diagnosticMessage =
            QStringLiteral("CABAC non-zero coded_block_pattern chroma is not implemented.");
        return result;
    }

    result.complete = true;
    return result;
}

H264CabacMbQpDeltaResult h264ReadCabacMbQpDeltaZero(BitReader &reader,
                                                    H264CabacDecoder &decoder,
                                                    H264CabacContextModelSet &contexts)
{
    H264CabacMbQpDeltaResult result;
    result.firstCtxIdx = 60;

    const H264CabacSyntaxResult firstBin = decodeContextBin(
        reader,
        decoder,
        contexts,
        result.firstCtxIdx,
        QStringLiteral("mb_qp_delta"));
    if (!firstBin.ok) {
        result.diagnosticCode = firstBin.diagnosticCode;
        result.diagnosticMessage = firstBin.diagnosticMessage;
        return result;
    }

    result.ok = true;
    if (firstBin.value != 0) {
        result.diagnosticCode = QStringLiteral("cabac_mb_qp_delta_incomplete");
        result.diagnosticMessage =
            QStringLiteral("CABAC non-zero mb_qp_delta is not implemented.");
        return result;
    }

    result.complete = true;
    result.mbQpDelta = 0;
    return result;
}
