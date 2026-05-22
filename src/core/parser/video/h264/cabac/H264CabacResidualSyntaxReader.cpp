#include "core/parser/video/h264/cabac/H264CabacResidualSyntaxReader.h"

#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

namespace
{
constexpr int Luma4x4SignificantCoeffFlagCtxIdxBase = 134;
constexpr int Luma4x4SignificantCoeffFlagSkeletonCount = 15;
constexpr int Luma4x4LastSignificantCoeffFlagCtxIdxBase = 166;
constexpr int Luma4x4CoeffAbsLevelMinus1FirstCtxIdx = 248;
constexpr int Luma4x4CoeffAbsLevelMinus1NextCtxIdx = 252;

int codedBlockFlagCtxIdx(H264CabacResidualBlockCategory category)
{
    switch (category) {
    case H264CabacResidualBlockCategory::Luma4x4:
        return 85;
    case H264CabacResidualBlockCategory::ChromaDc:
        return 97;
    }
    return -1;
}

QString residualBlockCategoryName(H264CabacResidualBlockCategory category)
{
    switch (category) {
    case H264CabacResidualBlockCategory::Luma4x4:
        return QStringLiteral("luma4x4");
    case H264CabacResidualBlockCategory::ChromaDc:
        return QStringLiteral("chroma_dc");
    }
    return QStringLiteral("unknown");
}

H264CabacResidualBlockResult failedResidualBlockResult(const QString &code,
                                                       const QString &message,
                                                       int ctxIdx = -1)
{
    H264CabacResidualBlockResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacResidualLuma4x4Result failedResidualLuma4x4Result(const QString &code,
                                                           const QString &message,
                                                           int ctxIdx = -1)
{
    H264CabacResidualLuma4x4Result result;
    result.firstCtxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacResidualChromaDcResult failedResidualChromaDcResult(const QString &code,
                                                             const QString &message,
                                                             int ctxIdx = -1)
{
    H264CabacResidualChromaDcResult result;
    result.firstCtxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

void appendLuma4x4CoeffReverseScanOrder(H264CabacResidualLuma4x4Result &result,
                                        int terminalScanIndex)
{
    result.coeffReverseScanIndices.append(terminalScanIndex);
    for (int i = result.significantScanIndices.size() - 1; i >= 0; --i) {
        const int scanIndex = result.significantScanIndices.at(i);
        if (scanIndex == terminalScanIndex || result.significantCoeffFlags.at(i) == 0) {
            continue;
        }
        result.coeffReverseScanIndices.append(scanIndex);
    }
}

bool readLuma4x4CoeffSignFlagSkeleton(BitReader &reader,
                                       H264CabacDecoder &decoder,
                                       int blockIndex,
                                       int scanIndex,
                                       H264CabacResidualLuma4x4Result &result)
{
    int sign = 0;
    if (!decoder.decodeBypassBin(reader, &sign)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bypass decoding failed while reading luma4x4 coeff_sign_flag[%1][%2].")
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    result.coeffSignFlags.append(sign);
    result.incompleteStage = QStringLiteral("residual_coefficients");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_sign_flag[%1][%2] was decoded; completing non-zero residual coefficients is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex);
    return true;
}

bool readLuma4x4CoeffAbsLevelMinus1FirstBinSkeleton(BitReader &reader,
                                                    H264CabacDecoder &decoder,
                                                    H264CabacContextModelSet &contexts,
                                                    int blockIndex,
                                                    int scanIndex,
                                                    bool inferredFinalScan,
                                                    H264CabacResidualLuma4x4Result &result)
{
    if (!contexts.isInitialized(Luma4x4CoeffAbsLevelMinus1FirstCtxIdx)) {
        result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
        result.diagnosticMessage =
            QStringLiteral("CABAC context %1 for luma4x4 coeff_abs_level_minus1[%2][%3] first prefix bin is not initialized in the covered context table.")
                .arg(Luma4x4CoeffAbsLevelMinus1FirstCtxIdx)
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, Luma4x4CoeffAbsLevelMinus1FirstCtxIdx, &bin)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading luma4x4 coeff_abs_level_minus1[%1][%2] first prefix bin.")
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    result.coeffAbsLevelScanIndices.append(scanIndex);
    result.coeffAbsLevelInferredFinalFlags.append(inferredFinalScan ? 1 : 0);
    result.coeffAbsLevelPrefixFirstBins.append(bin);
    result.incompleteBlockIndex = blockIndex;
    result.incompleteScanIndex = scanIndex;
    result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
    if (bin == 0) {
        return readLuma4x4CoeffSignFlagSkeleton(reader, decoder, blockIndex, scanIndex, result);
    }

    if (!contexts.isInitialized(Luma4x4CoeffAbsLevelMinus1NextCtxIdx)) {
        result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
        result.diagnosticMessage =
            QStringLiteral("CABAC context %1 for luma4x4 coeff_abs_level_minus1[%2][%3] next prefix bin is not initialized in the covered context table.")
                .arg(Luma4x4CoeffAbsLevelMinus1NextCtxIdx)
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    int nextBin = 0;
    if (!decoder.decodeBin(reader, contexts, Luma4x4CoeffAbsLevelMinus1NextCtxIdx, &nextBin)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading luma4x4 coeff_abs_level_minus1[%1][%2] next prefix bin.")
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    result.coeffAbsLevelPrefixNextBins.append(nextBin);
    if (nextBin == 0) {
        return readLuma4x4CoeffSignFlagSkeleton(reader, decoder, blockIndex, scanIndex, result);
    }

    result.incompleteStage = QStringLiteral("coeff_abs_level_minus1");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_abs_level_minus1[%1][%2] next prefix bin is 1; remaining coefficient level prefix parsing is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex);
    return true;
}

bool readLuma4x4CoeffReverseOrderSkeleton(BitReader &reader,
                                          H264CabacDecoder &decoder,
                                          H264CabacContextModelSet &contexts,
                                          int blockIndex,
                                          H264CabacResidualLuma4x4Result &result)
{
    const int coefficientLimit = result.coeffReverseScanIndices.size() < 2
        ? result.coeffReverseScanIndices.size()
        : 2;
    for (int i = 0; i < coefficientLimit; ++i) {
        const int scanIndex = result.coeffReverseScanIndices.at(i);
        const int signCountBefore = result.coeffSignFlags.size();
        if (!readLuma4x4CoeffAbsLevelMinus1FirstBinSkeleton(
                reader,
                decoder,
                contexts,
                blockIndex,
                scanIndex,
                scanIndex == Luma4x4SignificantCoeffFlagSkeletonCount,
                result)) {
            return false;
        }
        if (result.incompleteStage != QStringLiteral("residual_coefficients")
            || result.coeffSignFlags.size() == signCountBefore) {
            return true;
        }
    }
    return true;
}

bool readLuma4x4SignificantCoeffFlagsSkeleton(BitReader &reader,
                                              H264CabacDecoder &decoder,
                                              H264CabacContextModelSet &contexts,
                                              int blockIndex,
                                              H264CabacResidualLuma4x4Result &result)
{
    for (int scanIndex = 0; scanIndex < Luma4x4SignificantCoeffFlagSkeletonCount; ++scanIndex) {
        const int ctxIdx = Luma4x4SignificantCoeffFlagCtxIdxBase + scanIndex;
        if (!contexts.isInitialized(ctxIdx)) {
            result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
            result.diagnosticMessage =
                QStringLiteral("CABAC context %1 for luma4x4 significant_coeff_flag[%2][%3] is not initialized in the covered context table.")
                    .arg(ctxIdx)
                    .arg(blockIndex)
                    .arg(scanIndex);
            return false;
        }

        int bin = 0;
        if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading luma4x4 significant_coeff_flag[%1][%2].")
                    .arg(blockIndex)
                    .arg(scanIndex);
            return false;
        }

        result.significantScanIndices.append(scanIndex);
        result.significantCoeffFlags.append(bin);
        if (bin != 0) {
            const int lastCtxIdx = Luma4x4LastSignificantCoeffFlagCtxIdxBase + scanIndex;
            if (!contexts.isInitialized(lastCtxIdx)) {
                result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
                result.diagnosticMessage =
                    QStringLiteral("CABAC context %1 for luma4x4 last_significant_coeff_flag[%2][%3] is not initialized in the covered context table.")
                        .arg(lastCtxIdx)
                        .arg(blockIndex)
                        .arg(scanIndex);
                return false;
            }

            int lastBin = 0;
            if (!decoder.decodeBin(reader, contexts, lastCtxIdx, &lastBin)) {
                result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                result.diagnosticMessage =
                    QStringLiteral("CABAC bin decoding failed while reading luma4x4 last_significant_coeff_flag[%1][%2].")
                        .arg(blockIndex)
                        .arg(scanIndex);
                return false;
            }

            result.lastSignificantScanIndices.append(scanIndex);
            result.lastSignificantCoeffFlags.append(lastBin);
            if (lastBin != 0) {
                appendLuma4x4CoeffReverseScanOrder(result, scanIndex);
                return readLuma4x4CoeffReverseOrderSkeleton(reader, decoder, contexts, blockIndex, result);
            }

            continue;
        }
    }

    appendLuma4x4CoeffReverseScanOrder(result, Luma4x4SignificantCoeffFlagSkeletonCount);
    return readLuma4x4CoeffReverseOrderSkeleton(reader, decoder, contexts, blockIndex, result);
}
}

H264CabacResidualBlockResult h264ReadCabacResidualCodedBlockFlagZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    H264CabacResidualBlockCategory category)
{
    const int ctxIdx = codedBlockFlagCtxIdx(category);
    if (ctxIdx < 0) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_residual_block_category_unsupported"),
            QStringLiteral("CABAC residual block category is not supported by the coded_block_flag zero reader."),
            ctxIdx);
    }
    if (!contexts.isInitialized(ctxIdx)) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context %1 for residual coded_block_flag is not initialized in the covered context table.")
                .arg(ctxIdx),
            ctxIdx);
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading residual coded_block_flag."),
            ctxIdx);
    }

    H264CabacResidualBlockResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    result.codedBlockFlag = bin;
    if (bin != 0) {
        result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
        result.diagnosticMessage =
            QStringLiteral("CABAC %1 coded_block_flag is 1; significant_coeff_flag parsing is not implemented.")
                .arg(residualBlockCategoryName(category));
        return result;
    }

    result.complete = true;
    return result;
}

H264CabacResidualChromaDcResult h264ReadCabacResidualChromaDcCodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int chromaArrayType,
    int codedBlockPatternChroma)
{
    H264CabacResidualChromaDcResult result;
    result.firstCtxIdx = 97;

    if (codedBlockPatternChroma == 0) {
        result.ok = true;
        result.complete = true;
        return result;
    }
    if (chromaArrayType != 1) {
        return failedResidualChromaDcResult(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC chroma DC coded_block_flag zero reader currently only supports 4:2:0 chroma."),
            result.firstCtxIdx);
    }
    if (codedBlockPatternChroma != 1 && codedBlockPatternChroma != 2) {
        return failedResidualChromaDcResult(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC chroma DC coded_block_flag zero reader only supports coded_block_pattern_chroma 1 or 2."),
            result.firstCtxIdx);
    }

    for (int component = 0; component < 2; ++component) {
        const H264CabacResidualBlockResult block =
            h264ReadCabacResidualCodedBlockFlagZero(
                reader,
                decoder,
                contexts,
                H264CabacResidualBlockCategory::ChromaDc);
        if (!block.ok) {
            result.diagnosticCode = block.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC chroma_dc coded_block_flag[%1] failed: %2")
                                           .arg(component)
                                           .arg(block.diagnosticMessage);
            return result;
        }
        result.ok = true;
        result.components.append(component);
        result.codedBlockFlags.append(block.codedBlockFlag);
        if (!block.complete) {
            result.incompleteComponent = component;
            result.incompleteStage = QStringLiteral("significant_coeff_flag");
            result.diagnosticCode = block.diagnosticCode;
            result.diagnosticMessage =
                QStringLiteral("CABAC chroma_dc coded_block_flag[%1] is 1; significant_coeff_flag parsing is not implemented.")
                    .arg(component);
            return result;
        }
    }

    result.complete = true;
    return result;
}

H264CabacResidualLuma4x4Result h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int codedBlockPatternLuma)
{
    H264CabacResidualLuma4x4Result result;
    result.firstCtxIdx = 85;

    if (codedBlockPatternLuma <= 0 || codedBlockPatternLuma > 0x0f) {
        return failedResidualLuma4x4Result(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC narrow residual path only supports luma coded_block_pattern bits."),
            result.firstCtxIdx);
    }

    for (int luma8x8 = 0; luma8x8 < 4; ++luma8x8) {
        if (((codedBlockPatternLuma >> luma8x8) & 0x01) == 0) {
            continue;
        }
        for (int i4x4 = 0; i4x4 < 4; ++i4x4) {
            const int blockIndex = luma8x8 * 4 + i4x4;
            const H264CabacResidualBlockResult block =
                h264ReadCabacResidualCodedBlockFlagZero(
                    reader,
                    decoder,
                    contexts,
                    H264CabacResidualBlockCategory::Luma4x4);
            if (!block.ok) {
                result.diagnosticCode = block.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC luma4x4 coded_block_flag[%1] failed: %2")
                                               .arg(blockIndex)
                                               .arg(block.diagnosticMessage);
                return result;
            }
            result.ok = true;
            result.blockIndices.append(blockIndex);
            result.codedBlockFlags.append(block.codedBlockFlag);
            if (!block.complete) {
                result.incompleteBlockIndex = blockIndex;
                result.incompleteStage = QStringLiteral("significant_coeff_flag");
                if (!readLuma4x4SignificantCoeffFlagsSkeleton(
                        reader,
                        decoder,
                        contexts,
                        blockIndex,
                        result)) {
                    result.ok = false;
                }
                return result;
            }
        }
    }

    result.complete = true;
    return result;
}
