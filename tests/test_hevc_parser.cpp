#include "core/HevcParser.h"

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

QByteArray makeHevcNalu(int nalUnitType)
{
    QByteArray nalu;
    nalu.append(char((nalUnitType & 0x3f) << 1));
    nalu.append(char(0x01));
    nalu.append(char(0x80));
    return nalu;
}

QByteArray makeAnnexBPacket(std::initializer_list<QByteArray> nalus)
{
    QByteArray packet;
    for (const QByteArray &nalu : nalus) {
        packet.append(char(0x00));
        packet.append(char(0x00));
        packet.append(char(0x01));
        packet.append(nalu);
    }
    return packet;
}

QByteArray makeLengthPrefixedPacket(std::initializer_list<QByteArray> nalus)
{
    QByteArray packet;
    for (const QByteArray &nalu : nalus) {
        const int size = nalu.size();
        packet.append(char((size >> 24) & 0xff));
        packet.append(char((size >> 16) & 0xff));
        packet.append(char((size >> 8) & 0xff));
        packet.append(char(size & 0xff));
        packet.append(nalu);
    }
    return packet;
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

void testAnnexBHevcSkeleton()
{
    HevcParser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeHevcNalu(32),
        makeHevcNalu(33),
        makeHevcNalu(34),
        makeHevcNalu(19)
    });
    const FrameAnalysis analysis = parser.parsePacket(packet, 10, 8, 3);

    require(analysis.codecKind == CodecKind::HEVC, "HEVC codec kind");
    require(analysis.mediaKind == MediaKind::Video, "HEVC media kind");
    require(analysis.accessUnitKind == AccessUnitKind::VideoFrame, "HEVC access unit kind");
    require(analysis.frameIndex == 3, "HEVC packet index");
    require(analysis.hasFrame, "HEVC skeleton marks VCL packet as frame");
    require(analysis.frameType == QStringLiteral("IRAP"), "HEVC IRAP frame type");
    require(analysis.units.size() == 4, "HEVC NAL unit count");
    require(analysis.parameterSets.size() == 3, "HEVC parameter set count");
    require(analysis.units[0].typeName == QStringLiteral("VPS"), "HEVC VPS type name");
    require(hasDiagnosticCode(analysis, QStringLiteral("hevc_slice_header_unsupported")),
            "HEVC skeleton reports slice-header unsupported diagnostic");
}

void testLengthPrefixedHevcSkeleton()
{
    HevcParser parser;
    const QByteArray packet = makeLengthPrefixedPacket({
        makeHevcNalu(33),
        makeHevcNalu(1)
    });
    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);

    require(analysis.units.size() == 2, "HEVC length-prefixed NAL unit count");
    require(analysis.hasFrame, "HEVC length-prefixed VCL detected");
    require(analysis.frameType == QStringLiteral("VCL"), "HEVC non-IRAP VCL frame type");
}
}

int main()
{
    testAnnexBHevcSkeleton();
    testLengthPrefixedHevcSkeleton();

    std::cout << "HevcParser tests passed\n";
    return 0;
}
