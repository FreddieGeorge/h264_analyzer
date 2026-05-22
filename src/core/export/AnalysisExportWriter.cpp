#include "core/export/AnalysisExportWriter.h"

#include "core/export/H264SyntaxJsonWriter.h"
#include "core/parser/video/h264/H264FrameAnalysisAdapter.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

namespace
{
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
