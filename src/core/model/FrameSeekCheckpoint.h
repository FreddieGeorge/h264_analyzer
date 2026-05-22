#pragma once

#include "core/model/CodecKind.h"
#include "core/parser/BitstreamParser.h"

#include <QMetaType>
#include <QtGlobal>

extern "C" {
#include <libavutil/avutil.h>
}

struct FrameSeekCheckpoint
{
    int frameIndex = -1;
    int packetIndex = -1;
    int containerPacketIndex = -1;
    qint64 packetPosition = -1;
    qint64 packetPts = AV_NOPTS_VALUE;
    qint64 packetDts = AV_NOPTS_VALUE;
    bool keyframe = false;
    bool idr = false;
    CodecKind codecKind = CodecKind::Unknown;
    BitstreamParserStatePtr parserState;
};

Q_DECLARE_METATYPE(FrameSeekCheckpoint)
