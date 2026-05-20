#include "core/parser/video/h264/H264Parser.h"

#include "core/parser/video/h264/H264FrameAnalysisAdapter.h"
#include "core/util/ByteStream.h"
#include "core/util/Rbsp.h"

#include <QtEndian>

namespace
{
void normalizeRbspFieldsToPacket(QVector<SyntaxFieldInfo> &fields,
                                 const QVector<qsizetype> &rbspByteToPacketByte)
{
    for (SyntaxFieldInfo &field : fields) {
        field.packetBitRanges = packetBitRangesForRbspField(field.bitOffset,
                                                            field.bitLength,
                                                            rbspByteToPacketByte);
    }
}
}

H264Parser::H264Parser() = default;

CodecKind H264Parser::codecKind() const
{
    return CodecKind::H264;
}

void H264Parser::reset()
{
    m_spsById.clear();
    m_ppsById.clear();
    m_nalLengthSize = 4;
}

void H264Parser::parseDecoderConfigurationRecord(const QByteArray &extraData)
{
    if (extraData.size() < 7 || static_cast<uint8_t>(extraData[0]) != 1) {
        return;
    }

    const auto *data = reinterpret_cast<const uint8_t *>(extraData.constData());
    m_nalLengthSize = (data[4] & 0x03) + 1;

    qsizetype offset = 5;
    const int spsCount = data[offset++] & 0x1F;
    for (int i = 0; i < spsCount && offset + 2 <= extraData.size(); ++i) {
        const int length = qFromBigEndian<quint16>(data + offset);
        offset += 2;
        if (offset + length > extraData.size()) {
            return;
        }

        const QByteArray nalu(reinterpret_cast<const char *>(data + offset), length);
        const NaluInfo parsed = parseNaluPayload(NaluInfo {}, nalu);
        if (parsed.sps.valid) {
            m_spsById.insert(parsed.sps.seqParameterSetId, parsed.sps);
        }
        offset += length;
    }

    if (offset >= extraData.size()) {
        return;
    }

    const int ppsCount = data[offset++];
    for (int i = 0; i < ppsCount && offset + 2 <= extraData.size(); ++i) {
        const int length = qFromBigEndian<quint16>(data + offset);
        offset += 2;
        if (offset + length > extraData.size()) {
            return;
        }

        const QByteArray nalu(reinterpret_cast<const char *>(data + offset), length);
        const NaluInfo parsed = parseNaluPayload(NaluInfo {}, nalu);
        if (parsed.pps.valid) {
            m_ppsById.insert(parsed.pps.picParameterSetId, parsed.pps);
        }
        offset += length;
    }
}

void H264Parser::setParameterSets(const QHash<int, SpsInfo> &spsById, const QHash<int, PpsInfo> &ppsById)
{
    m_spsById = spsById;
    m_ppsById = ppsById;
}

BitstreamParserStatePtr H264Parser::snapshotState() const
{
    auto state = std::make_shared<H264ParserState>();
    state->spsById = m_spsById;
    state->ppsById = m_ppsById;
    state->nalLengthSize = m_nalLengthSize;
    return state;
}

void H264Parser::restoreState(const BitstreamParserStatePtr &state)
{
    const auto h264State = std::dynamic_pointer_cast<const H264ParserState>(state);
    if (!h264State) {
        reset();
        return;
    }

    m_spsById = h264State->spsById;
    m_ppsById = h264State->ppsById;
    m_nalLengthSize = h264State->nalLengthSize;
}

FrameAnalysis H264Parser::parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    return frameAnalysisFromH264Syntax(parsePacketSyntax(packetData, pts, dts, packetIndex));
}

FrameSyntaxInfo H264Parser::parsePacketSyntax(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    FrameSyntaxInfo frame;
    frame.codecKind = codecKind();
    frame.codecName = codecKindName(frame.codecKind);
    frame.index = packetIndex;
    frame.pts = pts;
    frame.dts = dts;

    frame.nalus = splitNalus(packetData, &frame.diagnostics);
    for (NaluInfo &nalu : frame.nalus) {
        if (nalu.sps.valid) {
            m_spsById.insert(nalu.sps.seqParameterSetId, nalu.sps);
        }
        if (nalu.pps.valid) {
            m_ppsById.insert(nalu.pps.picParameterSetId, nalu.pps);
        }
        if (nalu.slice.valid || !nalu.slice.diagnostics.isEmpty()) {
            frame.slices.append(nalu.slice);
            if (nalu.slice.valid && frame.frameNum < 0) {
                frame.frameNum = nalu.slice.frameNum;
                frame.poc = nalu.slice.picOrderCntLsb;
                frame.frameType = nalu.slice.sliceTypeName;
            }
        }
    }

    if (frame.frameType.isEmpty()) {
        frame.frameType = QStringLiteral("-");
    }
    return frame;
}

const QHash<int, SpsInfo> &H264Parser::spsMap() const
{
    return m_spsById;
}

const QHash<int, PpsInfo> &H264Parser::ppsMap() const
{
    return m_ppsById;
}

int H264Parser::nalLengthSize() const
{
    return m_nalLengthSize;
}

QString H264Parser::naluTypeName(int nalUnitType)
{
    switch (nalUnitType) {
    case 1: return QStringLiteral("Coded slice");
    case 5: return QStringLiteral("IDR slice");
    case 6: return QStringLiteral("SEI");
    case 7: return QStringLiteral("SPS");
    case 8: return QStringLiteral("PPS");
    case 9: return QStringLiteral("AUD");
    default: return QStringLiteral("NALU %1").arg(nalUnitType);
    }
}

QString H264Parser::sliceTypeName(int sliceType)
{
    switch (sliceType % 5) {
    case 0: return QStringLiteral("P");
    case 1: return QStringLiteral("B");
    case 2: return QStringLiteral("I");
    case 3: return QStringLiteral("SP");
    case 4: return QStringLiteral("SI");
    default: return QStringLiteral("Unknown");
    }
}

QVector<NaluInfo> H264Parser::splitNalus(const QByteArray &packetData, QVector<ParserDiagnosticInfo> *diagnostics)
{
    return hasAnnexBStartCode(packetData)
        ? splitAnnexBNalus(packetData)
        : splitLengthPrefixedNalus(packetData, diagnostics);
}

QVector<NaluInfo> H264Parser::splitAnnexBNalus(const QByteArray &packetData)
{
    QVector<NaluInfo> nalus;
    qsizetype offset = 0;

    while (offset < packetData.size()) {
        const qsizetype startSize = annexBStartCodeSizeAt(packetData, offset);
        if (startSize == 0) {
            ++offset;
            continue;
        }

        const qsizetype naluStart = offset + startSize;
        qsizetype nextStart = naluStart;
        while (nextStart < packetData.size() && annexBStartCodeSizeAt(packetData, nextStart) == 0) {
            ++nextStart;
        }

        if (nextStart > naluStart) {
            NaluInfo base;
            base.offset = naluStart;
            base.size = nextStart - naluStart;
            const QByteArray nalu = packetData.mid(naluStart, base.size);
            nalus.append(parseNaluPayload(base, nalu));
        }
        offset = nextStart;
    }

    return nalus;
}

QVector<NaluInfo> H264Parser::splitLengthPrefixedNalus(const QByteArray &packetData, QVector<ParserDiagnosticInfo> *diagnostics)
{
    QVector<NaluInfo> nalus;
    qsizetype offset = 0;
    auto appendDiagnostic = [diagnostics](const QString &code, const QString &message) {
        if (diagnostics != nullptr) {
            diagnostics->append({code, message});
        }
    };

    while (offset + m_nalLengthSize <= packetData.size()) {
        const qsizetype lengthOffset = offset;
        const int naluLength = readBigEndianLength(
            reinterpret_cast<const uint8_t *>(packetData.constData() + offset),
            m_nalLengthSize);
        offset += m_nalLengthSize;

        if (naluLength <= 0) {
            appendDiagnostic(
                QStringLiteral("avcc_invalid_nalu_length"),
                QStringLiteral("Length-prefixed packet contains a non-positive NALU length at byte %1.")
                    .arg(lengthOffset));
            break;
        }

        if (offset + naluLength > packetData.size()) {
            appendDiagnostic(
                QStringLiteral("avcc_nalu_length_exceeds_packet"),
                QStringLiteral("Length-prefixed NALU at byte %1 declares %2 bytes, but only %3 bytes remain.")
                    .arg(lengthOffset)
                    .arg(naluLength)
                    .arg(packetData.size() - offset));
            break;
        }

        NaluInfo base;
        base.offset = offset;
        base.size = naluLength;
        const QByteArray nalu = packetData.mid(offset, naluLength);
        nalus.append(parseNaluPayload(base, nalu));
        offset += naluLength;
    }

    if (offset < packetData.size() && packetData.size() - offset < m_nalLengthSize) {
        appendDiagnostic(
            QStringLiteral("avcc_length_prefix_truncated"),
            QStringLiteral("Length-prefixed packet ends with %1 trailing byte(s), shorter than the configured %2-byte NALU length field.")
                .arg(packetData.size() - offset)
                .arg(m_nalLengthSize));
    }

    return nalus;
}

NaluInfo H264Parser::parseNaluPayload(const NaluInfo &base, const QByteArray &nalu)
{
    NaluInfo info = base;
    if (nalu.isEmpty()) {
        return info;
    }

    const uint8_t header = static_cast<uint8_t>(nalu[0]);
    info.forbiddenZeroBit = (header >> 7) & 0x01;
    info.nalRefIdc = (header >> 5) & 0x03;
    info.nalUnitType = header & 0x1F;
    info.nalUnitTypeName = naluTypeName(info.nalUnitType);

    const QByteArray rbsp = rbspFromEbsp(reinterpret_cast<const uint8_t *>(nalu.constData() + 1),
                                        nalu.size() - 1);
    const QVector<qsizetype> rbspByteToPacketByte = rbspByteToPacketByteOffsets(nalu, info.offset + 1);

    if (info.nalUnitType == 7) {
        info.sps = parseSps(rbsp);
        normalizeRbspFieldsToPacket(info.sps.fields, rbspByteToPacketByte);
        if (info.sps.valid) {
            m_spsById.insert(info.sps.seqParameterSetId, info.sps);
        } else {
            info.diagnostics.append({
                QStringLiteral("sps_truncated"),
                QStringLiteral("SPS NALU ended unexpectedly or could not be fully parsed; parameter set was not cached.")
            });
        }
    } else if (info.nalUnitType == 8) {
        info.pps = parsePps(rbsp);
        normalizeRbspFieldsToPacket(info.pps.fields, rbspByteToPacketByte);
        if (info.pps.valid) {
            m_ppsById.insert(info.pps.picParameterSetId, info.pps);
        } else {
            info.diagnostics.append({
                QStringLiteral("pps_truncated"),
                QStringLiteral("PPS NALU ended unexpectedly or could not be fully parsed; parameter set was not cached.")
            });
        }
    } else if (info.nalUnitType == 1 || info.nalUnitType == 5) {
        info.slice = parseSliceHeader(rbsp, info.nalUnitType, info.nalRefIdc);
        normalizeRbspFieldsToPacket(info.slice.fields, rbspByteToPacketByte);
        for (MacroblockInfo &mb : info.slice.macroblocks) {
            normalizeRbspFieldsToPacket(mb.fields, rbspByteToPacketByte);
        }
    }

    return info;
}
