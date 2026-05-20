#pragma once

#include "core/parser/BitstreamParser.h"

class Mp3FrameParser : public IBitstreamParser
{
public:
    Mp3FrameParser();

    CodecKind codecKind() const override;
    void reset() override;
    void parseDecoderConfigurationRecord(const QByteArray &extraData) override;
    FrameAnalysis parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex) override;
    BitstreamParserStatePtr snapshotState() const override;
    void restoreState(const BitstreamParserStatePtr &state) override;

    static QString versionName(int versionId);
    static QString layerName(int layer);
    static QString channelModeName(int channelMode);
    static int sampleRateForIndex(int versionId, int samplingFrequencyIndex);
    static int bitrateKbpsForIndex(int versionId, int layer, int bitrateIndex);

private:
    static void appendField(FrameAnalysis &analysis,
                            const QString &name,
                            int bitOffset,
                            int bitLength,
                            const QString &value);
};
