#include "core/Mp3FrameParser.h"

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

QByteArray makeMpeg1Layer3HeaderOnlyFrame()
{
    QByteArray packet;
    packet.append(char(0xff));
    packet.append(char(0xfb));
    packet.append(char(0x90));
    packet.append(char(0x64));
    return packet;
}

void testValidMp3HeaderSkeleton()
{
    Mp3FrameParser parser;
    const FrameAnalysis analysis = parser.parsePacket(makeMpeg1Layer3HeaderOnlyFrame(), 12, 10, 4);

    require(analysis.codecKind == CodecKind::MP3, "MP3 codec kind");
    require(analysis.codecName == QStringLiteral("mp3"), "MP3 codec name");
    require(analysis.mediaKind == MediaKind::Audio, "MP3 media kind");
    require(analysis.accessUnitKind == AccessUnitKind::AudioFrame, "MP3 access unit kind");
    require(analysis.frameIndex == 4, "MP3 packet index");
    require(analysis.hasFrame, "MP3 skeleton marks valid header as frame");
    require(analysis.frameType == QStringLiteral("MP3 Frame"), "MP3 frame type");
    require(analysis.units.size() == 1, "MP3 unit count");
    require(analysis.units.first().kind == AnalysisUnitKind::Mp3Frame, "MP3 unit kind");
    require(fieldValue(analysis, QStringLiteral("version")) == QStringLiteral("MPEG 1"), "MP3 version field");
    require(fieldValue(analysis, QStringLiteral("layer")) == QStringLiteral("Layer III"), "MP3 layer field");
    require(fieldValue(analysis, QStringLiteral("bitrate_kbps")) == QStringLiteral("128"), "MP3 bitrate field");
    require(fieldValue(analysis, QStringLiteral("sample_rate")) == QStringLiteral("44100"), "MP3 sample rate field");
    require(fieldValue(analysis, QStringLiteral("channel_mode")) == QStringLiteral("Joint stereo"), "MP3 channel mode field");
    require(!hasDiagnosticCode(analysis, QStringLiteral("mp3_frame_sync_missing")),
            "MP3 valid header has syncword");
}

void testInvalidSyncwordDiagnostic()
{
    Mp3FrameParser parser;
    QByteArray packet = makeMpeg1Layer3HeaderOnlyFrame();
    packet[0] = char(0x00);
    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);

    require(!analysis.hasFrame, "MP3 invalid syncword is not a frame");
    require(hasDiagnosticCode(analysis, QStringLiteral("mp3_frame_sync_missing")),
            "MP3 invalid syncword diagnostic");
}

void testTruncatedHeaderDiagnostic()
{
    Mp3FrameParser parser;
    const FrameAnalysis analysis = parser.parsePacket(QByteArray::fromHex("fffb90"), 0, 0, 0);

    require(!analysis.hasFrame, "MP3 truncated header is not a frame");
    require(hasDiagnosticCode(analysis, QStringLiteral("mp3_frame_header_truncated")),
            "MP3 truncated header diagnostic");
}
}

int main()
{
    testValidMp3HeaderSkeleton();
    testInvalidSyncwordDiagnostic();
    testTruncatedHeaderDiagnostic();

    std::cout << "Mp3FrameParser tests passed\n";
    return 0;
}
