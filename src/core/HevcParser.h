#pragma once

#include "core/BitstreamParser.h"

#include <QByteArray>
#include <QVector>

struct HevcParserState : public BitstreamParserState
{
    int nalLengthSize = 4;
};

class HevcParser : public IBitstreamParser
{
public:
    HevcParser();

    CodecKind codecKind() const override;
    void reset() override;
    void parseDecoderConfigurationRecord(const QByteArray &extraData) override;
    FrameAnalysis parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex) override;
    BitstreamParserStatePtr snapshotState() const override;
    void restoreState(const BitstreamParserStatePtr &state) override;

    int nalLengthSize() const;

    static QString naluTypeName(int nalUnitType);

private:
    struct Nalu
    {
        qsizetype offset = 0;
        qsizetype size = 0;
        int type = -1;
    };

    QVector<Nalu> splitNalus(const QByteArray &packetData, QVector<AnalysisDiagnostic> *diagnostics) const;
    QVector<Nalu> splitAnnexBNalus(const QByteArray &packetData) const;
    QVector<Nalu> splitLengthPrefixedNalus(const QByteArray &packetData, QVector<AnalysisDiagnostic> *diagnostics) const;

    static bool hasAnnexBStartCode(const QByteArray &data);
    static qsizetype startCodeSizeAt(const QByteArray &data, qsizetype offset);
    static int readBigEndianLength(const uint8_t *data, int size);
    static int naluTypeFromHeader(const QByteArray &data, qsizetype offset, qsizetype size);
    static bool isVclNaluType(int nalUnitType);

    int m_nalLengthSize = 4;
};
