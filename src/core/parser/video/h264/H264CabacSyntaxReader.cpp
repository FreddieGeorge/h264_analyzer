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

H264CabacSubMbTypeResult failedSubMbTypeResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacSubMbTypeResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacRefIdxResult failedRefIdxResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacRefIdxResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacMvdResult failedMvdResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacMvdResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

int pSubMbPartitionCount(int subMbType)
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

H264CabacSubMbTypeResult h264ReadCabacPSubMbType(BitReader &reader,
                                                 H264CabacDecoder &decoder,
                                                 H264CabacContextModelSet &contexts,
                                                 const H264SliceDataContext &sliceContext)
{
    if (!sliceContext.isPSlice) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_slice_type_unsupported"),
            QStringLiteral("CABAC P sub_mb_type reader was used for a non-P slice."));
    }

    int bin = 0;
    const int ctxIdx = 21;
    if (!contexts.isInitialized(ctxIdx)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 21 for P sub_mb_type is not initialized in the covered context table."),
            ctxIdx);
    }
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type."),
            ctxIdx);
    }

    H264CabacSubMbTypeResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    if (bin == 0) {
        result.complete = true;
        result.subMbType = 0;
        return result;
    }

    if (!contexts.isInitialized(22)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 22 for P sub_mb_type is not initialized in the covered context table."),
            22);
    }
    if (!decoder.decodeBin(reader, contexts, 22, &bin)) {
        result.ok = false;
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type partition branch.");
        return result;
    }
    if (bin == 0) {
        result.complete = true;
        result.subMbType = 1;
        return result;
    }

    if (!contexts.isInitialized(23)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 23 for P sub_mb_type is not initialized in the covered context table."),
            23);
    }
    if (!decoder.decodeBin(reader, contexts, 23, &bin)) {
        result.ok = false;
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type 4x8/4x4 selector.");
        return result;
    }
    result.complete = true;
    result.subMbType = bin == 0 ? 2 : 3;
    return result;
}

H264CabacSubMbTypesResult h264ReadCabacPSubMbTypes(BitReader &reader,
                                                   H264CabacDecoder &decoder,
                                                   H264CabacContextModelSet &contexts,
                                                   const H264SliceDataContext &sliceContext,
                                                   int subMacroblockCount)
{
    H264CabacSubMbTypesResult result;
    if (subMacroblockCount <= 0) {
        result.ok = true;
        result.complete = true;
        return result;
    }

    for (int subIndex = 0; subIndex < subMacroblockCount; ++subIndex) {
        const H264CabacSubMbTypeResult subMbType =
            h264ReadCabacPSubMbType(reader, decoder, contexts, sliceContext);
        if (!subMbType.ok) {
            result.diagnosticCode = subMbType.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC P sub_mb_type[%1] failed: %2")
                                           .arg(subIndex)
                                           .arg(subMbType.diagnosticMessage);
            return result;
        }
        if (!subMbType.complete) {
            result.ok = true;
            result.diagnosticCode = subMbType.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC P sub_mb_type[%1] is incomplete: %2")
                                           .arg(subIndex)
                                           .arg(subMbType.diagnosticMessage);
            return result;
        }
        result.subMbTypes.append(subMbType.subMbType);
    }

    result.ok = true;
    result.complete = true;
    return result;
}

H264CabacRefIdxResult h264ReadCabacRefIdxL0(BitReader &reader,
                                            H264CabacDecoder &decoder,
                                            H264CabacContextModelSet &contexts,
                                            const H264SliceDataContext &sliceContext)
{
    H264CabacRefIdxResult result;
    if (sliceContext.slice.numRefIdxL0ActiveMinus1 <= 0) {
        result.ok = true;
        result.present = false;
        result.refIdx = 0;
        return result;
    }

    int bin = 0;
    const int firstCtxIdx = 54;
    if (!contexts.isInitialized(firstCtxIdx)) {
        return failedRefIdxResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 54 for ref_idx_l0 is not initialized in the covered context table."),
            firstCtxIdx);
    }
    if (!decoder.decodeBin(reader, contexts, firstCtxIdx, &bin)) {
        return failedRefIdxResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading ref_idx_l0."),
            firstCtxIdx);
    }

    result.ok = true;
    result.present = true;
    result.ctxIdx = firstCtxIdx;
    if (bin == 0) {
        result.refIdx = 0;
        return result;
    }

    result.diagnosticCode = QStringLiteral("cabac_ref_idx_incomplete");
    result.diagnosticMessage =
        QStringLiteral("CABAC ref_idx_l0 greater than zero is not implemented.");
    return result;
}

H264CabacRefIdxListResult h264ReadCabacPSubMbRefIdxL0(BitReader &reader,
                                                      H264CabacDecoder &decoder,
                                                      H264CabacContextModelSet &contexts,
                                                      const H264SliceDataContext &sliceContext,
                                                      int subMacroblockCount)
{
    H264CabacRefIdxListResult result;
    if (subMacroblockCount <= 0) {
        result.ok = true;
        return result;
    }
    if (sliceContext.slice.numRefIdxL0ActiveMinus1 <= 0) {
        result.ok = true;
        result.present = false;
        result.refIdx.fill(0, subMacroblockCount);
        return result;
    }

    result.present = true;
    for (int subIndex = 0; subIndex < subMacroblockCount; ++subIndex) {
        const H264CabacRefIdxResult refIdx =
            h264ReadCabacRefIdxL0(reader, decoder, contexts, sliceContext);
        if (!refIdx.ok) {
            result.diagnosticCode = refIdx.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC ref_idx_l0[%1] failed: %2")
                                           .arg(subIndex)
                                           .arg(refIdx.diagnosticMessage);
            return result;
        }
        if (!refIdx.diagnosticCode.isEmpty()) {
            result.ok = true;
            result.diagnosticCode = refIdx.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC ref_idx_l0[%1] is incomplete: %2")
                                           .arg(subIndex)
                                           .arg(refIdx.diagnosticMessage);
            return result;
        }
        result.refIdx.append(refIdx.refIdx);
    }

    result.ok = true;
    return result;
}

H264CabacMvdResult h264ReadCabacMvdL0Component(BitReader &reader,
                                               H264CabacDecoder &decoder,
                                               H264CabacContextModelSet &contexts,
                                               int component)
{
    const int ctxIdx = component == 0 ? 40 : 47;
    if (!contexts.isInitialized(ctxIdx)) {
        return failedMvdResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context %1 for mvd_l0 is not initialized in the covered context table.")
                .arg(ctxIdx),
            ctxIdx);
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedMvdResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading mvd_l0."),
            ctxIdx);
    }

    H264CabacMvdResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    if (bin == 0) {
        result.complete = true;
        result.value = 0;
        return result;
    }

    result.diagnosticCode = QStringLiteral("cabac_mvd_incomplete");
    result.diagnosticMessage =
        QStringLiteral("CABAC non-zero mvd_l0 is not implemented.");
    return result;
}

H264CabacMvdListResult h264ReadCabacPSubMbMvdL0(BitReader &reader,
                                                H264CabacDecoder &decoder,
                                                H264CabacContextModelSet &contexts,
                                                const QVector<int> &subMbTypes)
{
    H264CabacMvdListResult result;
    for (int subIndex = 0; subIndex < subMbTypes.size(); ++subIndex) {
        const int partitionCount = pSubMbPartitionCount(subMbTypes.at(subIndex));
        if (partitionCount <= 0) {
            result.diagnosticCode = QStringLiteral("cabac_sub_mb_type_invalid");
            result.diagnosticMessage =
                QStringLiteral("CABAC sub_mb_type[%1] has unsupported value %2.")
                    .arg(subIndex)
                    .arg(subMbTypes.at(subIndex));
            return result;
        }

        for (int partition = 0; partition < partitionCount; ++partition) {
            const H264CabacMvdResult mvdX =
                h264ReadCabacMvdL0Component(reader, decoder, contexts, 0);
            if (!mvdX.ok) {
                result.diagnosticCode = mvdX.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][0] failed: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdX.diagnosticMessage);
                return result;
            }
            if (!mvdX.complete) {
                result.ok = true;
                result.diagnosticCode = mvdX.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][0] is incomplete: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdX.diagnosticMessage);
                return result;
            }

            const H264CabacMvdResult mvdY =
                h264ReadCabacMvdL0Component(reader, decoder, contexts, 1);
            if (!mvdY.ok) {
                result.diagnosticCode = mvdY.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][1] failed: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdY.diagnosticMessage);
                return result;
            }
            if (!mvdY.complete) {
                result.ok = true;
                result.diagnosticCode = mvdY.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][1] is incomplete: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdY.diagnosticMessage);
                return result;
            }

            result.mvd.append({mvdX.value, mvdY.value});
        }
    }

    result.ok = true;
    result.complete = true;
    return result;
}
