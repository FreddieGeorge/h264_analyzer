#include "core/HevcParser.h"

#include <algorithm>

HevcParser::HevcParser() = default;

CodecKind HevcParser::codecKind() const
{
    return CodecKind::HEVC;
}

void HevcParser::reset()
{
    m_nalLengthSize = 4;
}

void HevcParser::parseDecoderConfigurationRecord(const QByteArray &extraData)
{
    if (extraData.size() >= 22) {
        m_nalLengthSize = (static_cast<unsigned char>(extraData.at(21)) & 0x03) + 1;
    }
}

FrameAnalysis HevcParser::parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    FrameAnalysis analysis;
    analysis.frameIndex = packetIndex;
    analysis.mediaKind = MediaKind::Video;
    analysis.accessUnitKind = AccessUnitKind::VideoFrame;
    analysis.codecKind = CodecKind::HEVC;
    analysis.codecName = QStringLiteral("hevc");
    analysis.pts = pts;
    analysis.dts = dts;

    QVector<AnalysisDiagnostic> splitDiagnostics;
    const QVector<Nalu> nalus = splitNalus(packetData, &splitDiagnostics);
    analysis.diagnostics += splitDiagnostics;

    bool sawVcl = false;
    bool sawIrap = false;
    for (const Nalu &nalu : nalus) {
        AnalysisUnit unit;
        unit.kind = AnalysisUnitKind::Nalu;
        unit.offset = nalu.offset;
        unit.size = nalu.size;
        unit.type = nalu.type;
        unit.typeName = naluTypeName(nalu.type);
        analysis.units.append(unit);

        if (nalu.type == 32 || nalu.type == 33 || nalu.type == 34) {
            AnalysisParameterSet parameterSet;
            parameterSet.kind = nalu.type == 32 ? QStringLiteral("VPS")
                : (nalu.type == 33 ? QStringLiteral("SPS") : QStringLiteral("PPS"));
            parameterSet.summary = QStringLiteral("HEVC %1 NALU, payload parsing not implemented yet.")
                                       .arg(parameterSet.kind);
            analysis.parameterSets.append(parameterSet);
        }

        if (isVclNaluType(nalu.type)) {
            sawVcl = true;
            if (nalu.type >= 16 && nalu.type <= 23) {
                sawIrap = true;
            }
        }
    }

    analysis.hasFrame = sawVcl;
    if (sawVcl) {
        analysis.frameType = sawIrap ? QStringLiteral("IRAP") : QStringLiteral("VCL");
        analysis.diagnostics.append({
            QStringLiteral("frame"),
            QStringLiteral("hevc_slice_header_unsupported"),
            QStringLiteral("HEVC playback is available, but VPS/SPS/PPS and slice syntax parsing are skeleton-only."),
            QStringLiteral("info")
        });
    }

    if (nalus.isEmpty() && analysis.diagnostics.isEmpty()) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("hevc_no_nalus_found"),
            QStringLiteral("No HEVC NAL units were found in this packet."),
            QStringLiteral("warning")
        });
    }

    return analysis;
}

BitstreamParserStatePtr HevcParser::snapshotState() const
{
    auto state = std::make_shared<HevcParserState>();
    state->nalLengthSize = m_nalLengthSize;
    return state;
}

void HevcParser::restoreState(const BitstreamParserStatePtr &state)
{
    const auto hevcState = std::dynamic_pointer_cast<const HevcParserState>(state);
    if (hevcState != nullptr) {
        m_nalLengthSize = hevcState->nalLengthSize;
    }
}

int HevcParser::nalLengthSize() const
{
    return m_nalLengthSize;
}

QString HevcParser::naluTypeName(int nalUnitType)
{
    switch (nalUnitType) {
    case 0: return QStringLiteral("TRAIL_N");
    case 1: return QStringLiteral("TRAIL_R");
    case 16: return QStringLiteral("BLA_W_LP");
    case 17: return QStringLiteral("BLA_W_RADL");
    case 18: return QStringLiteral("BLA_N_LP");
    case 19: return QStringLiteral("IDR_W_RADL");
    case 20: return QStringLiteral("IDR_N_LP");
    case 21: return QStringLiteral("CRA_NUT");
    case 32: return QStringLiteral("VPS");
    case 33: return QStringLiteral("SPS");
    case 34: return QStringLiteral("PPS");
    case 35: return QStringLiteral("AUD");
    case 39: return QStringLiteral("PREFIX_SEI");
    case 40: return QStringLiteral("SUFFIX_SEI");
    default:
        return QStringLiteral("HEVC_NAL_%1").arg(nalUnitType);
    }
}

QVector<HevcParser::Nalu> HevcParser::splitNalus(const QByteArray &packetData,
                                                 QVector<AnalysisDiagnostic> *diagnostics) const
{
    if (hasAnnexBStartCode(packetData)) {
        return splitAnnexBNalus(packetData);
    }
    return splitLengthPrefixedNalus(packetData, diagnostics);
}

QVector<HevcParser::Nalu> HevcParser::splitAnnexBNalus(const QByteArray &packetData) const
{
    QVector<Nalu> nalus;
    qsizetype offset = 0;
    while (offset < packetData.size()) {
        qsizetype startCodeSize = startCodeSizeAt(packetData, offset);
        if (startCodeSize == 0) {
            ++offset;
            continue;
        }

        const qsizetype naluStart = offset + startCodeSize;
        qsizetype nextStart = naluStart;
        while (nextStart < packetData.size() && startCodeSizeAt(packetData, nextStart) == 0) {
            ++nextStart;
        }

        const qsizetype naluSize = nextStart - naluStart;
        if (naluSize >= 2) {
            nalus.append({naluStart, naluSize, naluTypeFromHeader(packetData, naluStart, naluSize)});
        }
        offset = nextStart;
    }
    return nalus;
}

QVector<HevcParser::Nalu> HevcParser::splitLengthPrefixedNalus(
    const QByteArray &packetData,
    QVector<AnalysisDiagnostic> *diagnostics) const
{
    QVector<Nalu> nalus;
    qsizetype offset = 0;
    while (offset + m_nalLengthSize <= packetData.size()) {
        const int naluSize = readBigEndianLength(
            reinterpret_cast<const uint8_t *>(packetData.constData() + offset),
            m_nalLengthSize);
        const qsizetype naluStart = offset + m_nalLengthSize;
        if (naluSize < 0 || naluStart + naluSize > packetData.size()) {
            if (diagnostics != nullptr) {
                diagnostics->append({
                    QStringLiteral("packet"),
                    QStringLiteral("hevc_length_prefixed_nalu_exceeds_packet"),
                    QStringLiteral("HEVC length-prefixed NALU size exceeds the available packet bytes."),
                    QStringLiteral("warning")
                });
            }
            break;
        }
        if (naluSize >= 2) {
            nalus.append({naluStart, naluSize, naluTypeFromHeader(packetData, naluStart, naluSize)});
        }
        offset = naluStart + naluSize;
    }
    return nalus;
}

bool HevcParser::hasAnnexBStartCode(const QByteArray &data)
{
    const qsizetype scanLimit = std::min<qsizetype>(data.size(), 64);
    for (qsizetype i = 0; i < scanLimit; ++i) {
        if (startCodeSizeAt(data, i) != 0) {
            return true;
        }
    }
    return false;
}

qsizetype HevcParser::startCodeSizeAt(const QByteArray &data, qsizetype offset)
{
    if (offset + 3 <= data.size()
        && data[offset] == 0
        && data[offset + 1] == 0
        && data[offset + 2] == 1) {
        return 3;
    }

    if (offset + 4 <= data.size()
        && data[offset] == 0
        && data[offset + 1] == 0
        && data[offset + 2] == 0
        && data[offset + 3] == 1) {
        return 4;
    }
    return 0;
}

int HevcParser::readBigEndianLength(const uint8_t *data, int size)
{
    int value = 0;
    for (int i = 0; i < size; ++i) {
        value = (value << 8) | data[i];
    }
    return value;
}

int HevcParser::naluTypeFromHeader(const QByteArray &data, qsizetype offset, qsizetype size)
{
    if (offset < 0 || offset + 2 > data.size() || size < 2) {
        return -1;
    }
    return (static_cast<unsigned char>(data.at(offset)) >> 1) & 0x3f;
}

bool HevcParser::isVclNaluType(int nalUnitType)
{
    return nalUnitType >= 0 && nalUnitType <= 31;
}
