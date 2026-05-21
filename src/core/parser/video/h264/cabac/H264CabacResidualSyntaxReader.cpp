#include "core/parser/video/h264/cabac/H264CabacResidualSyntaxReader.h"

#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

namespace
{
int codedBlockFlagCtxIdx(H264CabacResidualBlockCategory category)
{
    switch (category) {
    case H264CabacResidualBlockCategory::Luma4x4:
        return 85;
    }
    return -1;
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
            QStringLiteral("CABAC non-zero residual coded_block_flag is not implemented.");
        return result;
    }

    result.complete = true;
    return result;
}
