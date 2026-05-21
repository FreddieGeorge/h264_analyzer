#include "core/export/AnalysisExportWriter.h"

#include "core/parser/video/h264/H264FrameAnalysisAdapter.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

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

QJsonObject bitFieldToJson(const AnalysisBitField &field)
{
    QJsonArray packetBitRanges;
    for (const AnalysisBitRange &range : field.packetBitRanges) {
        packetBitRanges.append(QJsonObject {
            {QStringLiteral("bit_offset"), static_cast<double>(range.bitOffset)},
            {QStringLiteral("bit_length"), static_cast<double>(range.bitLength)},
            {QStringLiteral("offset_basis"), range.offsetBasis}
        });
    }

    return {
        {QStringLiteral("path"), field.path},
        {QStringLiteral("name"), field.name},
        {QStringLiteral("bit_offset"), static_cast<double>(field.bitOffset)},
        {QStringLiteral("bit_length"), static_cast<double>(field.bitLength)},
        {QStringLiteral("value"), field.value},
        {QStringLiteral("offset_basis"), field.offsetBasis},
        {QStringLiteral("packet_bit_ranges"), packetBitRanges}
    };
}
}

QJsonObject streamInfoToJson(const StreamInfo &stream)
{
    QJsonArray streams;
    for (const MediaStreamInfo &mediaStream : stream.streams) {
        streams.append(mediaStreamInfoToJson(mediaStream));
    }

    return {
        {QStringLiteral("media_kind"), mediaKindName(stream.mediaKind)},
        {QStringLiteral("stream_index"), stream.streamIndex},
        {QStringLiteral("file_name"), stream.fileName},
        {QStringLiteral("absolute_file_path"), QDir::toNativeSeparators(stream.absoluteFilePath)},
        {QStringLiteral("size_bytes"), static_cast<double>(stream.sizeBytes)},
        {QStringLiteral("codec"), codecKindName(stream.codecKind)},
        {QStringLiteral("codec_name"), stream.codecName},
        {QStringLiteral("pixel_format"), stream.pixelFormatName},
        {QStringLiteral("sample_format"), stream.sampleFormatName},
        {QStringLiteral("channel_layout"), stream.channelLayoutName},
        {QStringLiteral("width"), stream.width},
        {QStringLiteral("height"), stream.height},
        {QStringLiteral("frame_rate"), stream.frameRate},
        {QStringLiteral("sample_rate"), stream.sampleRate},
        {QStringLiteral("channels"), stream.channels},
        {QStringLiteral("streams"), streams},
        {QStringLiteral("duration_us"), static_cast<double>(stream.durationUs)},
        {QStringLiteral("bit_rate"), static_cast<double>(stream.bitRate)}
    };
}

QJsonObject mediaStreamInfoToJson(const MediaStreamInfo &stream)
{
    return {
        {QStringLiteral("stream_index"), stream.streamIndex},
        {QStringLiteral("selected"), stream.selected},
        {QStringLiteral("media_kind"), mediaKindName(stream.mediaKind)},
        {QStringLiteral("codec"), codecKindName(stream.codecKind)},
        {QStringLiteral("codec_name"), stream.codecName},
        {QStringLiteral("pixel_format"), stream.pixelFormatName},
        {QStringLiteral("sample_format"), stream.sampleFormatName},
        {QStringLiteral("channel_layout"), stream.channelLayoutName},
        {QStringLiteral("bit_rate"), static_cast<double>(stream.bitRate)},
        {QStringLiteral("duration_us"), static_cast<double>(stream.durationUs)},
        {QStringLiteral("width"), stream.width},
        {QStringLiteral("height"), stream.height},
        {QStringLiteral("frame_rate"), stream.frameRate},
        {QStringLiteral("sample_rate"), stream.sampleRate},
        {QStringLiteral("channels"), stream.channels}
    };
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

QJsonObject frameAnalysisToJson(const FrameAnalysis &analysis)
{
    QJsonArray units;
    for (const AnalysisUnit &unit : analysis.units) {
        units.append(QJsonObject {
            {QStringLiteral("kind"), analysisUnitKindName(unit.kind)},
            {QStringLiteral("offset"), static_cast<double>(unit.offset)},
            {QStringLiteral("size"), static_cast<double>(unit.size)},
            {QStringLiteral("type"), unit.type},
            {QStringLiteral("type_name"), unit.typeName}
        });
    }

    QJsonArray parameterSets;
    for (const AnalysisParameterSet &parameterSet : analysis.parameterSets) {
        QJsonArray fields;
        for (const AnalysisBitField &field : parameterSet.bitFields) {
            fields.append(bitFieldToJson(field));
        }
        parameterSets.append(QJsonObject {
            {QStringLiteral("kind"), parameterSet.kind},
            {QStringLiteral("id"), parameterSet.id},
            {QStringLiteral("summary"), parameterSet.summary},
            {QStringLiteral("bit_fields"), fields}
        });
    }

    QJsonArray regions;
    for (const AnalysisRegion &region : analysis.regions) {
        regions.append(QJsonObject {
            {QStringLiteral("kind"), analysisRegionKindName(region.kind)},
            {QStringLiteral("address"), region.address},
            {QStringLiteral("x"), region.x},
            {QStringLiteral("y"), region.y},
            {QStringLiteral("width"), region.width},
            {QStringLiteral("height"), region.height},
            {QStringLiteral("qp"), region.qp},
            {QStringLiteral("type"), region.type},
            {QStringLiteral("prediction_mode"), region.predictionMode},
            {QStringLiteral("parsed"), region.parsed},
            {QStringLiteral("skipped"), region.skipped},
            {QStringLiteral("note"), region.note}
        });
    }

    QJsonArray motionVectors;
    for (const AnalysisMotionVector &mv : analysis.motionVectors) {
        motionVectors.append(QJsonObject {
            {QStringLiteral("region_address"), mv.regionAddress},
            {QStringLiteral("list"), mv.list},
            {QStringLiteral("reference_index"), mv.referenceIndex},
            {QStringLiteral("source_x"), mv.sourceX},
            {QStringLiteral("source_y"), mv.sourceY},
            {QStringLiteral("reference_x"), mv.referenceX},
            {QStringLiteral("reference_y"), mv.referenceY},
            {QStringLiteral("mv_x_quarter_pel"), mv.mvXQuarterPel},
            {QStringLiteral("mv_y_quarter_pel"), mv.mvYQuarterPel}
        });
    }

    QJsonArray diagnostics;
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        diagnostics.append(QJsonObject {
            {QStringLiteral("path"), diagnostic.path},
            {QStringLiteral("code"), diagnostic.code},
            {QStringLiteral("message"), diagnostic.message},
            {QStringLiteral("severity"), diagnostic.severity}
        });
    }

    QJsonArray bitFields;
    for (const AnalysisBitField &field : analysis.bitFields) {
        bitFields.append(bitFieldToJson(field));
    }

    const QJsonObject packet {
        {QStringLiteral("packet_index"), analysis.packet.streamPacketIndex},
        {QStringLiteral("stream_packet_index"), analysis.packet.streamPacketIndex},
        {QStringLiteral("container_packet_index"), analysis.packet.containerPacketIndex},
        {QStringLiteral("stream_index"), analysis.packet.streamIndex},
        {QStringLiteral("media_kind"), mediaKindName(analysis.packet.mediaKind)},
        {QStringLiteral("codec"), codecKindName(analysis.packet.codecKind)},
        {QStringLiteral("pts"), static_cast<double>(analysis.packet.pts)},
        {QStringLiteral("dts"), static_cast<double>(analysis.packet.dts)},
        {QStringLiteral("duration"), static_cast<double>(analysis.packet.duration)},
        {QStringLiteral("pos"), static_cast<double>(analysis.packet.position)},
        {QStringLiteral("size"), static_cast<double>(analysis.packet.size)},
        {QStringLiteral("keyframe"), analysis.packet.keyframe},
        {QStringLiteral("raw_bytes_available"), !analysis.packet.bytes.isEmpty()},
        {QStringLiteral("raw_bytes_size"), analysis.packet.bytes.size()}
    };

    return {
        {QStringLiteral("index"), analysis.frameIndex},
        {QStringLiteral("stream_index"), analysis.streamIndex},
        {QStringLiteral("media_kind"), mediaKindName(analysis.mediaKind)},
        {QStringLiteral("access_unit_kind"), accessUnitKindName(analysis.accessUnitKind)},
        {QStringLiteral("codec"), codecKindName(analysis.codecKind)},
        {QStringLiteral("codec_name"), analysis.codecName},
        {QStringLiteral("pts"), static_cast<double>(analysis.pts)},
        {QStringLiteral("dts"), static_cast<double>(analysis.dts)},
        {QStringLiteral("poc"), analysis.poc},
        {QStringLiteral("frame_num"), analysis.frameNum},
        {QStringLiteral("frame_type"), analysis.frameType},
        {QStringLiteral("has_frame"), analysis.hasFrame},
        {QStringLiteral("units"), units},
        {QStringLiteral("parameter_sets"), parameterSets},
        {QStringLiteral("regions"), regions},
        {QStringLiteral("motion_vectors"), motionVectors},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("bit_fields"), bitFields},
        {QStringLiteral("packet"), packet}
    };
}

QJsonObject selectedFrameExportToJson(const StreamInfo &stream,
                                      const FrameAnalysis &analysis,
                                      const QString &generator,
                                      const QString &generatorVersion)
{
    QJsonObject result {
        {QStringLiteral("schema_version"), 3},
        {QStringLiteral("generator"), generator},
        {QStringLiteral("generator_version"), generatorVersion},
        {QStringLiteral("stream"), streamInfoToJson(stream)},
        {QStringLiteral("frame_analysis"), frameAnalysisToJson(analysis)}
    };
    if (analysis.codecKind == CodecKind::H264) {
        result.insert(QStringLiteral("frame"), h264FrameSyntaxToJson(h264SyntaxFromFrameAnalysis(analysis)));
    }
    return result;
}

QJsonObject allFramesExportToJson(const StreamInfo &stream,
                                  const QVector<FrameAnalysis> &frames,
                                  const QString &generator,
                                  const QString &generatorVersion)
{
    QJsonArray frameArray;
    for (const FrameAnalysis &frame : frames) {
        if (frame.frameIndex >= 0) {
            QJsonObject frameObject = frameAnalysisToJson(frame);
            if (frame.codecKind == CodecKind::H264) {
                frameObject.insert(QStringLiteral("h264"), h264FrameSyntaxToJson(h264SyntaxFromFrameAnalysis(frame)));
            }
            frameArray.append(frameObject);
        }
    }

    return {
        {QStringLiteral("schema_version"), 3},
        {QStringLiteral("generator"), generator},
        {QStringLiteral("generator_version"), generatorVersion},
        {QStringLiteral("stream"), streamInfoToJson(stream)},
        {QStringLiteral("frames"), frameArray}
    };
}
