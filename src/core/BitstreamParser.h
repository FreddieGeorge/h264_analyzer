#pragma once

#include "core/CodecKind.h"
#include "core/FrameAnalysis.h"

#include <QByteArray>

#include <memory>

struct BitstreamParserState
{
    virtual ~BitstreamParserState() = default;
};

using BitstreamParserStatePtr = std::shared_ptr<const BitstreamParserState>;

class IBitstreamParser
{
public:
    virtual ~IBitstreamParser() = default;

    virtual CodecKind codecKind() const = 0;
    virtual void reset() = 0;
    virtual void parseDecoderConfigurationRecord(const QByteArray &extraData) = 0;
    virtual FrameAnalysis parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex) = 0;
    virtual BitstreamParserStatePtr snapshotState() const = 0;
    virtual void restoreState(const BitstreamParserStatePtr &state) = 0;
};
