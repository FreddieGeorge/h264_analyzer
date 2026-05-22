#include "core/analysis/AccessUnitAnalyzer.h"

#include "core/decode/PacketRawDataBuilder.h"

#include <algorithm>

void AccessUnitAnalyzer::clear()
{
    if (m_videoParser != nullptr) {
        m_videoParser->reset();
    }
    m_videoParser.reset();
    m_videoStreamIndex = -1;
    m_videoPacketIndex = 0;
    m_packetParsers.clear();
}

void AccessUnitAnalyzer::addPacketParser(int streamIndex,
                                         MediaKind mediaKind,
                                         std::unique_ptr<IBitstreamParser> parser,
                                         const QByteArray &extraData)
{
    if (parser == nullptr) {
        return;
    }

    parser->reset();
    parser->parseDecoderConfigurationRecord(extraData);

    StreamPacketParser packetParser;
    packetParser.streamIndex = streamIndex;
    packetParser.mediaKind = mediaKind;
    packetParser.parser = std::move(parser);
    m_packetParsers.push_back(std::move(packetParser));
}

void AccessUnitAnalyzer::setVideoParser(int streamIndex,
                                        std::unique_ptr<IBitstreamParser> parser,
                                        const QByteArray &extraData)
{
    m_videoStreamIndex = streamIndex;
    m_videoPacketIndex = 0;
    m_videoParser = std::move(parser);
    if (m_videoParser == nullptr) {
        return;
    }

    m_videoParser->reset();
    m_videoParser->parseDecoderConfigurationRecord(extraData);
}

void AccessUnitAnalyzer::restoreVideoParserState(const FrameSeekCheckpoint &checkpoint)
{
    if (m_videoParser != nullptr && checkpoint.codecKind == m_videoParser->codecKind()) {
        m_videoParser->restoreState(checkpoint.parserState);
    }
}

void AccessUnitAnalyzer::setVideoPacketIndex(int packetIndex)
{
    m_videoPacketIndex = std::max(0, packetIndex);
}

QVector<FrameAnalysis> AccessUnitAnalyzer::parseNonVideoPacket(const AVPacket *packet, int containerPacketIndex)
{
    QVector<FrameAnalysis> analyses;
    if (!packetHasPayload(packet)) {
        return analyses;
    }

    for (StreamPacketParser &packetParser : m_packetParsers) {
        if (packetParser.streamIndex != packet->stream_index || packetParser.parser == nullptr) {
            continue;
        }

        const QByteArray packetData(reinterpret_cast<const char *>(packet->data), packet->size);
        const int packetIndex = packetParser.packetIndex++;
        FrameAnalysis analysis = packetParser.parser->parsePacket(packetData,
                                                                  packet->pts,
                                                                  packet->dts,
                                                                  packetIndex);
        analysis.streamIndex = packetParser.streamIndex;
        analysis.mediaKind = packetParser.mediaKind;
        analysis.accessUnitKind = AccessUnitKind::AudioFrame;
        analysis.packet = packetRawDataFromAvPacket(packet,
                                                    containerPacketIndex,
                                                    packetIndex,
                                                    packetParser.streamIndex,
                                                    packetParser.mediaKind,
                                                    analysis.codecKind);
        analyses.append(analysis);
        break;
    }

    return analyses;
}

std::optional<AccessUnitAnalyzer::ParsedVideoPacket>
AccessUnitAnalyzer::parseVideoPacket(const AVPacket *packet, int containerPacketIndex)
{
    if (m_videoParser == nullptr || !packetHasPayload(packet)) {
        return std::nullopt;
    }

    const QByteArray packetData(reinterpret_cast<const char *>(packet->data), packet->size);
    const int packetIndex = m_videoPacketIndex++;
    FrameAnalysis analysis = m_videoParser->parsePacket(packetData,
                                                        packet->pts,
                                                        packet->dts,
                                                        packetIndex);
    analysis.streamIndex = m_videoStreamIndex;
    analysis.mediaKind = MediaKind::Video;
    analysis.accessUnitKind = AccessUnitKind::VideoFrame;
    analysis.packet = packetRawDataFromAvPacket(packet,
                                                containerPacketIndex,
                                                packetIndex,
                                                m_videoStreamIndex,
                                                MediaKind::Video,
                                                analysis.codecKind);

    ParsedVideoPacket parsed;
    parsed.analysis = analysis;
    parsed.hasFrame = analysis.hasFrame;
    if (analysis.hasFrame) {
        parsed.checkpoint.packetIndex = packetIndex;
        parsed.checkpoint.containerPacketIndex = containerPacketIndex;
        parsed.checkpoint.packetPosition = packet->pos;
        parsed.checkpoint.packetPts = packet->pts;
        parsed.checkpoint.packetDts = packet->dts;
        parsed.checkpoint.keyframe = (packet->flags & AV_PKT_FLAG_KEY) != 0;
        parsed.checkpoint.codecKind = m_videoParser->codecKind();
        parsed.checkpoint.parserState = m_videoParser->snapshotState();
        parsed.checkpoint.idr = isIdrAccessUnit(analysis);
    }

    return parsed;
}

bool AccessUnitAnalyzer::packetHasPayload(const AVPacket *packet)
{
    return packet != nullptr && packet->data != nullptr && packet->size > 0;
}

bool AccessUnitAnalyzer::isIdrAccessUnit(const FrameAnalysis &analysis)
{
    if (analysis.codecKind != CodecKind::H264) {
        return false;
    }

    for (const AnalysisUnit &unit : analysis.units) {
        if (unit.kind == AnalysisUnitKind::Nalu && unit.type == 5) {
            return true;
        }
    }
    return false;
}
