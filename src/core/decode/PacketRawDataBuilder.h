#pragma once

#include "core/model/FrameAnalysis.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

PacketRawData packetRawDataFromAvPacket(const AVPacket *packet,
                                        int containerPacketIndex,
                                        int streamPacketIndex,
                                        int streamIndex,
                                        MediaKind mediaKind,
                                        CodecKind codecKind);
