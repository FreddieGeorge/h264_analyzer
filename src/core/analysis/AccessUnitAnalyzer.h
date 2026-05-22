#pragma once

#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/parser/BitstreamParser.h"

#include <QByteArray>
#include <QVector>

#include <memory>
#include <optional>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

class AccessUnitAnalyzer
{
public:
    struct ParsedVideoPacket
    {
        FrameAnalysis analysis;
        FrameSeekCheckpoint checkpoint;
        bool hasFrame = false;
    };

    void clear();

    void addPacketParser(int streamIndex,
                         MediaKind mediaKind,
                         std::unique_ptr<IBitstreamParser> parser,
                         const QByteArray &extraData);
    void setVideoParser(int streamIndex,
                        std::unique_ptr<IBitstreamParser> parser,
                        const QByteArray &extraData);
    void restoreVideoParserState(const FrameSeekCheckpoint &checkpoint);
    void setVideoPacketIndex(int packetIndex);

    QVector<FrameAnalysis> parseNonVideoPacket(const AVPacket *packet, int containerPacketIndex);
    std::optional<ParsedVideoPacket> parseVideoPacket(const AVPacket *packet, int containerPacketIndex);

private:
    struct StreamPacketParser
    {
        int streamIndex = -1;
        MediaKind mediaKind = MediaKind::Unknown;
        int packetIndex = 0;
        std::unique_ptr<IBitstreamParser> parser;
    };

    static bool packetHasPayload(const AVPacket *packet);
    static bool isIdrAccessUnit(const FrameAnalysis &analysis);

    int m_videoStreamIndex = -1;
    int m_videoPacketIndex = 0;
    std::unique_ptr<IBitstreamParser> m_videoParser;
    std::vector<StreamPacketParser> m_packetParsers;
};
