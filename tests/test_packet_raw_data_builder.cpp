#include "core/decode/PacketRawDataBuilder.h"

#include <QByteArray>

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void nullPacketReturnsOnlyContextMetadata()
{
    const PacketRawData raw = packetRawDataFromAvPacket(nullptr,
                                                        11,
                                                        7,
                                                        3,
                                                        MediaKind::Audio,
                                                        CodecKind::AAC);
    require(raw.containerPacketIndex == 11, "null packet container index");
    require(raw.streamPacketIndex == 7, "null packet stream packet index");
    require(raw.streamIndex == 3, "null packet stream index");
    require(raw.mediaKind == MediaKind::Audio, "null packet media kind");
    require(raw.codecKind == CodecKind::AAC, "null packet codec kind");
    require(raw.bytes.isEmpty(), "null packet has no raw bytes");
    require(raw.position == -1, "null packet keeps default position");
}

void packetPayloadAndTimingAreCopied()
{
    AVPacket *packet = av_packet_alloc();
    require(packet != nullptr, "packet allocation");
    require(av_new_packet(packet, 4) == 0, "packet payload allocation");

    packet->stream_index = 2;
    packet->pts = 100;
    packet->dts = 90;
    packet->duration = 33;
    packet->pos = 1234;
    packet->flags |= AV_PKT_FLAG_KEY;
    packet->data[0] = 0x01;
    packet->data[1] = 0x23;
    packet->data[2] = 0x45;
    packet->data[3] = 0x67;

    const PacketRawData raw = packetRawDataFromAvPacket(packet,
                                                        8,
                                                        5,
                                                        2,
                                                        MediaKind::Video,
                                                        CodecKind::H264);

    require(raw.containerPacketIndex == 8, "packet container index");
    require(raw.streamPacketIndex == 5, "packet stream packet index");
    require(raw.streamIndex == 2, "packet stream index");
    require(raw.mediaKind == MediaKind::Video, "packet media kind");
    require(raw.codecKind == CodecKind::H264, "packet codec kind");
    require(raw.pts == 100, "packet pts");
    require(raw.dts == 90, "packet dts");
    require(raw.duration == 33, "packet duration");
    require(raw.position == 1234, "packet position");
    require(raw.size == 4, "packet size");
    require(raw.keyframe, "packet keyframe flag");
    require(raw.bytes == QByteArray::fromHex("01234567"), "packet raw bytes");

    av_packet_free(&packet);
}
}

int main()
{
    nullPacketReturnsOnlyContextMetadata();
    packetPayloadAndTimingAreCopied();

    std::cout << "Packet raw data builder tests passed\n";
    return 0;
}
