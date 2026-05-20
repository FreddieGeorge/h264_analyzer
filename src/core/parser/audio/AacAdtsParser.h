#pragma once

#include "core/parser/BitstreamParser.h"

#include <QByteArray>

class AacAdtsParser : public IBitstreamParser
{
public:
    AacAdtsParser();

    CodecKind codecKind() const override;
    void reset() override;
    void parseDecoderConfigurationRecord(const QByteArray &extraData) override;
    FrameAnalysis parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex) override;
    BitstreamParserStatePtr snapshotState() const override;
    void restoreState(const BitstreamParserStatePtr &state) override;

    static QString profileName(int profileObjectTypeMinus1);
    static int sampleRateForIndex(int samplingFrequencyIndex);

private:
    static int readBits(const QByteArray &data, int bitOffset, int bitCount, bool *ok);
    static void appendField(FrameAnalysis &analysis,
                            const QString &name,
                            int bitOffset,
                            int bitLength,
                            const QString &value);
};
