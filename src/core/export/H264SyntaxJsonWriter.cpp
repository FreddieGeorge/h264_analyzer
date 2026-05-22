#include "core/export/H264SyntaxJsonWriter.h"

#include <QJsonArray>

namespace
{
QJsonObject motionVectorToJson(const MotionVectorInfo &mv)
{
    return {
        {QStringLiteral("list"), mv.list},
        {QStringLiteral("reference_index"), mv.referenceIndex},
        {QStringLiteral("mv_x_quarter_pel"), mv.mvXQuarterPel},
        {QStringLiteral("mv_y_quarter_pel"), mv.mvYQuarterPel},
        {QStringLiteral("reference_x"), mv.referenceX},
        {QStringLiteral("reference_y"), mv.referenceY}
    };
}

QJsonObject residualBlockToJson(const ResidualBlockInfo &block)
{
    QJsonArray coefficients;
    for (const ResidualBlockInfo::Coefficient &coefficient : block.coefficients) {
        coefficients.append(QJsonObject {
            {QStringLiteral("scan_index"), coefficient.scanIndex},
            {QStringLiteral("level"), coefficient.level},
            {QStringLiteral("run_before"), coefficient.runBefore}
        });
    }

    return {
        {QStringLiteral("kind"), block.kind},
        {QStringLiteral("component"), block.component},
        {QStringLiteral("block_index"), block.blockIndex},
        {QStringLiteral("predicted_non_zero_count"), block.predictedNonZeroCount},
        {QStringLiteral("max_coefficient_count"), block.maxCoefficientCount},
        {QStringLiteral("total_coefficient_count"), block.totalCoefficientCount},
        {QStringLiteral("trailing_ones"), block.trailingOnes},
        {QStringLiteral("total_zeros"), block.totalZeros},
        {QStringLiteral("coefficients"), coefficients}
    };
}

QJsonObject diagnosticToJson(const ParserDiagnosticInfo &diagnostic)
{
    return {
        {QStringLiteral("code"), diagnostic.code},
        {QStringLiteral("message"), diagnostic.message}
    };
}

QJsonObject macroblockToJson(const MacroblockInfo &mb)
{
    QJsonArray motionVectors;
    for (const MotionVectorInfo &mv : mb.motionVectors) {
        motionVectors.append(motionVectorToJson(mv));
    }

    QJsonArray residualBlocks;
    for (const ResidualBlockInfo &block : mb.residualBlocks) {
        residualBlocks.append(residualBlockToJson(block));
    }

    return {
        {QStringLiteral("address"), mb.address},
        {QStringLiteral("mb_type"), mb.mbType},
        {QStringLiteral("prediction_mode"), mb.predictionMode},
        {QStringLiteral("coded_block_pattern"), mb.codedBlockPattern},
        {QStringLiteral("coded_block_pattern_luma"), mb.codedBlockPatternLuma},
        {QStringLiteral("coded_block_pattern_chroma"), mb.codedBlockPatternChroma},
        {QStringLiteral("qp"), mb.qp},
        {QStringLiteral("mb_qp_delta"), mb.mbQpDelta},
        {QStringLiteral("residual_block_count"), mb.residualBlockCount},
        {QStringLiteral("residual_coefficient_count"), mb.residualCoefficientCount},
        {QStringLiteral("residual_parsed"), mb.residualParsed},
        {QStringLiteral("residual_blocks"), residualBlocks},
        {QStringLiteral("skipped"), mb.skipped},
        {QStringLiteral("parsed"), mb.parsed},
        {QStringLiteral("note"), mb.note},
        {QStringLiteral("motion_vectors"), motionVectors}
    };
}

QJsonObject spsToJson(const SpsInfo &sps)
{
    return {
        {QStringLiteral("valid"), sps.valid},
        {QStringLiteral("profile_idc"), sps.profileIdc},
        {QStringLiteral("level_idc"), sps.levelIdc},
        {QStringLiteral("seq_parameter_set_id"), sps.seqParameterSetId},
        {QStringLiteral("width"), sps.width},
        {QStringLiteral("height"), sps.height},
        {QStringLiteral("vui_parameters_present_flag"), sps.vuiParametersPresentFlag},
        {QStringLiteral("aspect_ratio_idc"), sps.aspectRatioIdc},
        {QStringLiteral("timing_info_present_flag"), sps.timingInfoPresentFlag},
        {QStringLiteral("bitstream_restriction_flag"), sps.bitstreamRestrictionFlag}
    };
}

QJsonObject ppsToJson(const PpsInfo &pps)
{
    return {
        {QStringLiteral("valid"), pps.valid},
        {QStringLiteral("pic_parameter_set_id"), pps.picParameterSetId},
        {QStringLiteral("seq_parameter_set_id"), pps.seqParameterSetId},
        {QStringLiteral("entropy_coding_mode_flag"), pps.entropyCodingModeFlag},
        {QStringLiteral("weighted_pred_flag"), pps.weightedPredFlag},
        {QStringLiteral("weighted_bipred_idc"), pps.weightedBipredIdc},
        {QStringLiteral("transform_8x8_mode_flag"), pps.transform8x8ModeFlag},
        {QStringLiteral("pic_init_qp_minus26"), pps.picInitQpMinus26}
    };
}

QJsonObject naluToJson(const NaluInfo &nalu)
{
    QJsonArray diagnostics;
    for (const ParserDiagnosticInfo &diagnostic : nalu.diagnostics) {
        diagnostics.append(diagnosticToJson(diagnostic));
    }

    QJsonObject result {
        {QStringLiteral("offset"), static_cast<double>(nalu.offset)},
        {QStringLiteral("size"), static_cast<double>(nalu.size)},
        {QStringLiteral("nal_ref_idc"), nalu.nalRefIdc},
        {QStringLiteral("nal_unit_type"), nalu.nalUnitType},
        {QStringLiteral("nal_unit_type_name"), nalu.nalUnitTypeName},
        {QStringLiteral("diagnostics"), diagnostics}
    };
    if (nalu.sps.valid) {
        result.insert(QStringLiteral("sps"), spsToJson(nalu.sps));
    }
    if (nalu.pps.valid) {
        result.insert(QStringLiteral("pps"), ppsToJson(nalu.pps));
    }
    return result;
}

QJsonObject sliceToJson(const SliceInfo &slice)
{
    QJsonArray macroblocks;
    for (const MacroblockInfo &mb : slice.macroblocks) {
        macroblocks.append(macroblockToJson(mb));
    }

    QJsonArray diagnostics;
    for (const ParserDiagnosticInfo &diagnostic : slice.diagnostics) {
        diagnostics.append(diagnosticToJson(diagnostic));
    }

    QJsonArray warnings;
    for (const QString &warning : slice.macroblockParseWarnings) {
        warnings.append(warning);
    }

    return {
        {QStringLiteral("slice_type"), slice.sliceType},
        {QStringLiteral("slice_type_name"), slice.sliceTypeName},
        {QStringLiteral("first_mb_in_slice"), slice.firstMbInSlice},
        {QStringLiteral("pic_parameter_set_id"), slice.picParameterSetId},
        {QStringLiteral("frame_num"), slice.frameNum},
        {QStringLiteral("pic_order_cnt_lsb"), slice.picOrderCntLsb},
        {QStringLiteral("cabac_init_idc"), slice.cabacInitIdc},
        {QStringLiteral("slice_qp_delta"), slice.sliceQpDelta},
        {QStringLiteral("derived_qp"), slice.derivedQp},
        {QStringLiteral("pic_width_in_mbs"), slice.picWidthInMbs},
        {QStringLiteral("pic_height_in_mbs"), slice.picHeightInMbs},
        {QStringLiteral("macroblocks_parsed"), slice.macroblocksParsed},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("macroblock_parse_warnings"), warnings},
        {QStringLiteral("macroblocks"), macroblocks}
    };
}
}

QJsonObject h264FrameSyntaxToJson(const FrameSyntaxInfo &syntaxInfo)
{
    QJsonArray diagnostics;
    for (const ParserDiagnosticInfo &diagnostic : syntaxInfo.diagnostics) {
        diagnostics.append(diagnosticToJson(diagnostic));
    }

    QJsonArray nalus;
    for (const NaluInfo &nalu : syntaxInfo.nalus) {
        nalus.append(naluToJson(nalu));
    }

    QJsonArray slices;
    for (const SliceInfo &slice : syntaxInfo.slices) {
        slices.append(sliceToJson(slice));
    }

    return {
        {QStringLiteral("index"), syntaxInfo.index},
        {QStringLiteral("codec"), codecKindName(syntaxInfo.codecKind)},
        {QStringLiteral("codec_name"), syntaxInfo.codecName},
        {QStringLiteral("pts"), static_cast<double>(syntaxInfo.pts)},
        {QStringLiteral("dts"), static_cast<double>(syntaxInfo.dts)},
        {QStringLiteral("poc"), syntaxInfo.poc},
        {QStringLiteral("frame_num"), syntaxInfo.frameNum},
        {QStringLiteral("frame_type"), syntaxInfo.frameType},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("nalus"), nalus},
        {QStringLiteral("slices"), slices}
    };
}
