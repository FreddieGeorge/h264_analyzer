#include "core/decode/PacketRawDataBuilder.h"

#include <QByteArray>

PacketRawData packetRawDataFromAvPacket(const AVPacket *packet,
                                        int containerPacketIndex,
                                        int streamPacketIndex,
                                        int streamIndex,
                                        MediaKind mediaKind,
                                        CodecKind codecKind)
{
    PacketRawData rawData;
    rawData.containerPacketIndex = containerPacketIndex;
    rawData.streamPacketIndex = streamPacketIndex;
    rawData.streamIndex = streamIndex;
    rawData.mediaKind = mediaKind;
    rawData.codecKind = codecKind;
    if (packet == nullptr) {
        return rawData;
    }

    rawData.pts = packet->pts;
    rawData.dts = packet->dts;
    rawData.duration = packet->duration;
    rawData.position = packet->pos;
    rawData.size = packet->size;
    rawData.keyframe = (packet->flags & AV_PKT_FLAG_KEY) != 0;
    if (packet->data != nullptr && packet->size > 0) {
        rawData.bytes = QByteArray(reinterpret_cast<const char *>(packet->data), packet->size);
    }
    return rawData;
}
