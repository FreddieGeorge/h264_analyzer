#include "core/parser/audio/AacAdtsParser.h"

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

bool hasDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

QString fieldValue(const FrameAnalysis &analysis, const QString &name)
{
    for (const AnalysisBitField &field : analysis.bitFields) {
        if (field.name == name) {
            return field.value;
        }
    }
    return {};
}

QByteArray makeAacLc44100StereoHeaderOnlyFrame()
{
    QByteArray packet;
    packet.append(char(0xff));
    packet.append(char(0xf1));
    packet.append(char(0x50));
    packet.append(char(0x80));
    packet.append(char(0x00));
    packet.append(char(0xff));
    packet.append(char(0xfc));
    return packet;
}

void testValidAdtsHeaderSkeleton()
{
    AacAdtsParser parser;
    const FrameAnalysis analysis = parser.parsePacket(makeAacLc44100StereoHeaderOnlyFrame(), 12, 10, 4);

    require(analysis.codecKind == CodecKind::AAC, "AAC codec kind");
    require(analysis.codecName == QStringLiteral("aac"), "AAC codec name");
    require(analysis.mediaKind == MediaKind::Audio, "AAC media kind");
    require(analysis.accessUnitKind == AccessUnitKind::AudioFrame, "AAC access unit kind");
    require(analysis.frameIndex == 4, "AAC packet index");
    require(analysis.pts == 12, "AAC PTS");
    require(analysis.dts == 10, "AAC DTS");
    require(analysis.hasFrame, "AAC skeleton marks valid ADTS packet as frame");
    require(analysis.frameType == QStringLiteral("AAC ADTS"), "AAC frame type");
    require(analysis.units.size() == 1, "AAC ADTS unit count");
    require(analysis.units.first().kind == AnalysisUnitKind::AdtsFrame, "AAC ADTS unit kind");
    require(analysis.units.first().size == 7, "AAC ADTS unit size");
    require(fieldValue(analysis, QStringLiteral("profile")) == QStringLiteral("LC"), "AAC profile field");
    require(fieldValue(analysis, QStringLiteral("sample_rate")) == QStringLiteral("44100"), "AAC sample rate field");
    require(fieldValue(analysis, QStringLiteral("channel_configuration")) == QStringLiteral("2"),
            "AAC channel configuration field");
    require(fieldValue(analysis, QStringLiteral("aac_frame_length")) == QStringLiteral("7"),
            "AAC frame length field");
    require(!hasDiagnosticCode(analysis, QStringLiteral("aac_adts_syncword_missing")),
            "AAC valid header has syncword");
}

void testInvalidSyncwordDiagnostic()
{
    AacAdtsParser parser;
    QByteArray packet = makeAacLc44100StereoHeaderOnlyFrame();
    packet[0] = char(0x00);
    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);

    require(!analysis.hasFrame, "AAC invalid syncword is not a frame");
    require(hasDiagnosticCode(analysis, QStringLiteral("aac_adts_syncword_missing")),
            "AAC invalid syncword diagnostic");
}

void testTruncatedHeaderDiagnostic()
{
    AacAdtsParser parser;
    const FrameAnalysis analysis = parser.parsePacket(QByteArray::fromHex("fff150"), 0, 0, 0);

    require(!analysis.hasFrame, "AAC truncated header is not a frame");
    require(hasDiagnosticCode(analysis, QStringLiteral("aac_adts_header_truncated")),
            "AAC truncated header diagnostic");
}
}

int main()
{
    testValidAdtsHeaderSkeleton();
    testInvalidSyncwordDiagnostic();
    testTruncatedHeaderDiagnostic();

    std::cout << "AacAdtsParser tests passed\n";
    return 0;
}
