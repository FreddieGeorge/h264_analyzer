#include "core/H264Parser.h"

#include "core/H264FrameAnalysisAdapter.h"

#include <QtEndian>

#include <QStringList>

#include <algorithm>
#include <array>

class H264Parser::BitReader
{
public:
    explicit BitReader(const QByteArray &data)
        : m_data(reinterpret_cast<const uint8_t *>(data.constData()))
        , m_sizeBits(data.size() * 8)
    {
    }

    bool readBit()
    {
        if (m_bitOffset >= m_sizeBits) {
            m_error = true;
            return false;
        }
        const qsizetype byteOffset = m_bitOffset / 8;
        const int bitInByte = 7 - static_cast<int>(m_bitOffset % 8);
        ++m_bitOffset;
        return ((m_data[byteOffset] >> bitInByte) & 0x01) != 0;
    }

    quint32 readBits(int count)
    {
        quint32 value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 1) | (readBit() ? 1U : 0U);
        }
        return value;
    }

    quint32 readUE()
    {
        int leadingZeroBits = 0;
        while (m_bitOffset < m_sizeBits && !readBit()) {
            ++leadingZeroBits;
            if (leadingZeroBits >= 31) {
                m_error = true;
                return 0;
            }
        }

        if (leadingZeroBits == 0) {
            return 0;
        }

        const quint32 suffix = readBits(leadingZeroBits);
        return ((1U << leadingZeroBits) - 1U) + suffix;
    }

    qint32 readSE()
    {
        const quint32 codeNum = readUE();
        const qint32 value = static_cast<qint32>((codeNum + 1U) / 2U);
        return (codeNum % 2U) == 0U ? -value : value;
    }

    bool hasError() const
    {
        return m_error;
    }

    qsizetype bitsRemaining() const
    {
        return m_sizeBits - m_bitOffset;
    }

    qsizetype bitOffset() const
    {
        return m_bitOffset;
    }

    bool moreRbspData() const
    {
        for (qsizetype bit = m_sizeBits - 1; bit >= m_bitOffset; --bit) {
            const qsizetype byteOffset = bit / 8;
            const int bitInByte = 7 - static_cast<int>(bit % 8);
            if (((m_data[byteOffset] >> bitInByte) & 0x01) != 0) {
                return bit > m_bitOffset;
            }
            if (bit == 0) {
                break;
            }
        }
        return false;
    }

private:
    const uint8_t *m_data = nullptr;
    qsizetype m_sizeBits = 0;
    qsizetype m_bitOffset = 0;
    bool m_error = false;
};

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

#ifdef H264_ANALYZER_ENABLE_TESTS
quint32 H264Parser::decodeUnsignedExpGolombForTest(const QByteArray &data, bool *ok)
{
    BitReader reader(data);
    const quint32 value = reader.readUE();
    if (ok != nullptr) {
        *ok = !reader.hasError();
    }
    return value;
}

qint32 H264Parser::decodeSignedExpGolombForTest(const QByteArray &data, bool *ok)
{
    BitReader reader(data);
    const qint32 value = reader.readSE();
    if (ok != nullptr) {
        *ok = !reader.hasError();
    }
    return value;
}
#endif

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
        const qsizetype startSize = startCodeSizeAt(packetData, offset);
        if (startSize == 0) {
            ++offset;
            continue;
        }

        const qsizetype naluStart = offset + startSize;
        qsizetype nextStart = naluStart;
        while (nextStart < packetData.size() && startCodeSizeAt(packetData, nextStart) == 0) {
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

    if (info.nalUnitType == 7) {
        info.sps = parseSps(rbsp);
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
    }

    return info;
}

SpsInfo H264Parser::parseSps(const QByteArray &rbsp) const
{
    BitReader reader(rbsp);
    SpsInfo sps;

    auto addField = [&sps](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        sps.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    sps.profileIdc = static_cast<int>(readBitsField(QStringLiteral("profile_idc"), 8));
    sps.constraintSet0Flag = readBitField(QStringLiteral("constraint_set0_flag"));
    sps.constraintSet1Flag = readBitField(QStringLiteral("constraint_set1_flag"));
    sps.constraintSet2Flag = readBitField(QStringLiteral("constraint_set2_flag"));
    sps.constraintSet3Flag = readBitField(QStringLiteral("constraint_set3_flag"));
    sps.constraintSet4Flag = readBitField(QStringLiteral("constraint_set4_flag"));
    sps.constraintSet5Flag = readBitField(QStringLiteral("constraint_set5_flag"));
    sps.reservedZero2Bits = static_cast<int>(readBitsField(QStringLiteral("reserved_zero_2bits"), 2));
    sps.levelIdc = static_cast<int>(readBitsField(QStringLiteral("level_idc"), 8));
    sps.seqParameterSetId = static_cast<int>(readUEField(QStringLiteral("seq_parameter_set_id")));

    const bool highProfile = sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122
        || sps.profileIdc == 244 || sps.profileIdc == 44 || sps.profileIdc == 83
        || sps.profileIdc == 86 || sps.profileIdc == 118 || sps.profileIdc == 128
        || sps.profileIdc == 138 || sps.profileIdc == 139 || sps.profileIdc == 134 || sps.profileIdc == 135;

    if (highProfile) {
        sps.chromaFormatIdc = static_cast<int>(readUEField(QStringLiteral("chroma_format_idc")));
        if (sps.chromaFormatIdc == 3) {
            readBitField(QStringLiteral("separate_colour_plane_flag"));
        }
        readUEField(QStringLiteral("bit_depth_luma_minus8"));
        readUEField(QStringLiteral("bit_depth_chroma_minus8"));
        readBitField(QStringLiteral("qpprime_y_zero_transform_bypass_flag"));
        if (readBitField(QStringLiteral("seq_scaling_matrix_present_flag"))) {
            const int scalingListCount = sps.chromaFormatIdc != 3 ? 8 : 12;
            for (int i = 0; i < scalingListCount; ++i) {
                if (reader.readBit()) {
                    skipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
        }
    }

    sps.log2MaxFrameNumMinus4 = static_cast<int>(readUEField(QStringLiteral("log2_max_frame_num_minus4")));
    sps.picOrderCntType = static_cast<int>(readUEField(QStringLiteral("pic_order_cnt_type")));
    if (sps.picOrderCntType == 0) {
        sps.log2MaxPicOrderCntLsbMinus4 = static_cast<int>(readUEField(QStringLiteral("log2_max_pic_order_cnt_lsb_minus4")));
    } else if (sps.picOrderCntType == 1) {
        sps.deltaPicOrderAlwaysZeroFlag = readBitField(QStringLiteral("delta_pic_order_always_zero_flag"));
        reader.readSE();
        reader.readSE();
        const quint32 cycleCount = reader.readUE();
        for (quint32 i = 0; i < cycleCount; ++i) {
            reader.readSE();
        }
    }

    readUEField(QStringLiteral("max_num_ref_frames"));
    readBitField(QStringLiteral("gaps_in_frame_num_value_allowed_flag"));
    sps.picWidthInMbsMinus1 = static_cast<int>(readUEField(QStringLiteral("pic_width_in_mbs_minus1")));
    sps.picHeightInMapUnitsMinus1 = static_cast<int>(readUEField(QStringLiteral("pic_height_in_map_units_minus1")));
    sps.frameMbsOnlyFlag = readBitField(QStringLiteral("frame_mbs_only_flag"));
    if (!sps.frameMbsOnlyFlag) {
        readBitField(QStringLiteral("mb_adaptive_frame_field_flag"));
    }
    readBitField(QStringLiteral("direct_8x8_inference_flag"));
    if (readBitField(QStringLiteral("frame_cropping_flag"))) {
        sps.frameCropLeftOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_left_offset")));
        sps.frameCropRightOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_right_offset")));
        sps.frameCropTopOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_top_offset")));
        sps.frameCropBottomOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_bottom_offset")));
    }

    auto skipHrdParameters = [&reader]() {
        const quint32 cpbCntMinus1 = reader.readUE();
        reader.readBits(4);
        reader.readBits(4);
        for (quint32 i = 0; i <= cpbCntMinus1; ++i) {
            reader.readUE();
            reader.readUE();
            reader.readBit();
        }
        reader.readBits(5);
        reader.readBits(5);
        reader.readBits(5);
        reader.readBits(5);
    };

    sps.vuiParametersPresentFlag = readBitField(QStringLiteral("vui_parameters_present_flag"));
    if (sps.vuiParametersPresentFlag) {
        sps.aspectRatioInfoPresentFlag = readBitField(QStringLiteral("aspect_ratio_info_present_flag"));
        if (sps.aspectRatioInfoPresentFlag) {
            sps.aspectRatioIdc = static_cast<int>(readBitsField(QStringLiteral("aspect_ratio_idc"), 8));
            if (sps.aspectRatioIdc == 255) {
                sps.sarWidth = static_cast<int>(readBitsField(QStringLiteral("sar_width"), 16));
                sps.sarHeight = static_cast<int>(readBitsField(QStringLiteral("sar_height"), 16));
            }
        }

        if (reader.readBit()) {
            reader.readBit();
        }
        if (reader.readBit()) {
            reader.readBits(3);
            reader.readBit();
            if (reader.readBit()) {
                reader.readBits(8);
                reader.readBits(8);
                reader.readBits(8);
            }
        }
        if (reader.readBit()) {
            reader.readUE();
            reader.readUE();
        }

        sps.timingInfoPresentFlag = readBitField(QStringLiteral("timing_info_present_flag"));
        if (sps.timingInfoPresentFlag) {
            sps.numUnitsInTick = readBitsField(QStringLiteral("num_units_in_tick"), 32);
            sps.timeScale = readBitsField(QStringLiteral("time_scale"), 32);
            sps.fixedFrameRateFlag = readBitField(QStringLiteral("fixed_frame_rate_flag"));
        }

        const bool nalHrdParametersPresentFlag = reader.readBit();
        if (nalHrdParametersPresentFlag) {
            skipHrdParameters();
        }
        const bool vclHrdParametersPresentFlag = reader.readBit();
        if (vclHrdParametersPresentFlag) {
            skipHrdParameters();
        }
        if (nalHrdParametersPresentFlag || vclHrdParametersPresentFlag) {
            reader.readBit();
        }
        reader.readBit();

        sps.bitstreamRestrictionFlag = readBitField(QStringLiteral("bitstream_restriction_flag"));
        if (sps.bitstreamRestrictionFlag) {
            sps.motionVectorsOverPicBoundariesFlag = readBitField(QStringLiteral("motion_vectors_over_pic_boundaries_flag"));
            sps.maxBytesPerPicDenom = static_cast<int>(readUEField(QStringLiteral("max_bytes_per_pic_denom")));
            sps.maxBitsPerMbDenom = static_cast<int>(readUEField(QStringLiteral("max_bits_per_mb_denom")));
            sps.log2MaxMvLengthHorizontal = static_cast<int>(readUEField(QStringLiteral("log2_max_mv_length_horizontal")));
            sps.log2MaxMvLengthVertical = static_cast<int>(readUEField(QStringLiteral("log2_max_mv_length_vertical")));
            sps.maxNumReorderFrames = static_cast<int>(readUEField(QStringLiteral("max_num_reorder_frames")));
            sps.maxDecFrameBuffering = static_cast<int>(readUEField(QStringLiteral("max_dec_frame_buffering")));
        }
    }

    const int widthMbs = sps.picWidthInMbsMinus1 + 1;
    const int heightMapUnits = sps.picHeightInMapUnitsMinus1 + 1;
    const int frameHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) * heightMapUnits;

    int cropUnitX = 1;
    int cropUnitY = 2 - (sps.frameMbsOnlyFlag ? 1 : 0);
    if (sps.chromaFormatIdc == 1) {
        cropUnitX = 2;
        cropUnitY = 2 * (2 - (sps.frameMbsOnlyFlag ? 1 : 0));
    } else if (sps.chromaFormatIdc == 2) {
        cropUnitX = 2;
        cropUnitY = 2 - (sps.frameMbsOnlyFlag ? 1 : 0);
    }

    sps.width = widthMbs * 16 - cropUnitX * (sps.frameCropLeftOffset + sps.frameCropRightOffset);
    sps.height = frameHeightInMbs * 16 - cropUnitY * (sps.frameCropTopOffset + sps.frameCropBottomOffset);
    sps.valid = !reader.hasError();
    return sps;
}

PpsInfo H264Parser::parsePps(const QByteArray &rbsp) const
{
    BitReader reader(rbsp);
    PpsInfo pps;

    auto addField = [&pps](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        pps.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readSEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    pps.picParameterSetId = static_cast<int>(readUEField(QStringLiteral("pic_parameter_set_id")));
    pps.seqParameterSetId = static_cast<int>(readUEField(QStringLiteral("seq_parameter_set_id")));
    pps.entropyCodingModeFlag = readBitField(QStringLiteral("entropy_coding_mode_flag"));
    pps.bottomFieldPicOrderInFramePresentFlag = readBitField(QStringLiteral("bottom_field_pic_order_in_frame_present_flag"));
    pps.numSliceGroupsMinus1 = static_cast<int>(readUEField(QStringLiteral("num_slice_groups_minus1")));

    if (pps.numSliceGroupsMinus1 > 0) {
        const quint32 sliceGroupMapType = reader.readUE();
        if (sliceGroupMapType == 0) {
            for (int i = 0; i <= pps.numSliceGroupsMinus1; ++i) {
                reader.readUE();
            }
        } else if (sliceGroupMapType == 2) {
            for (int i = 0; i < pps.numSliceGroupsMinus1; ++i) {
                reader.readUE();
                reader.readUE();
                reader.readUE();
            }
        } else if (sliceGroupMapType == 3 || sliceGroupMapType == 4 || sliceGroupMapType == 5) {
            reader.readBit();
            reader.readUE();
        } else if (sliceGroupMapType == 6) {
            const quint32 picSizeInMapUnitsMinus1 = reader.readUE();
            int bits = 0;
            while ((1U << bits) < static_cast<quint32>(pps.numSliceGroupsMinus1 + 1)) {
                ++bits;
            }
            for (quint32 i = 0; i <= picSizeInMapUnitsMinus1; ++i) {
                reader.readBits(bits);
            }
        }
    }

    pps.numRefIdxL0DefaultActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l0_default_active_minus1")));
    pps.numRefIdxL1DefaultActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l1_default_active_minus1")));
    pps.weightedPredFlag = readBitField(QStringLiteral("weighted_pred_flag"));
    pps.weightedBipredIdc = static_cast<int>(readBitsField(QStringLiteral("weighted_bipred_idc"), 2));
    pps.picInitQpMinus26 = readSEField(QStringLiteral("pic_init_qp_minus26"));
    readSEField(QStringLiteral("pic_init_qs_minus26"));
    pps.secondChromaQpIndexOffset = readSEField(QStringLiteral("chroma_qp_index_offset"));
    pps.deblockingFilterControlPresentFlag = readBitField(QStringLiteral("deblocking_filter_control_present_flag"));
    pps.constrainedIntraPredFlag = readBitField(QStringLiteral("constrained_intra_pred_flag"));
    pps.redundantPicCntPresentFlag = readBitField(QStringLiteral("redundant_pic_cnt_present_flag"));

    if (reader.moreRbspData()) {
        const SpsInfo sps = m_spsById.value(pps.seqParameterSetId);
        pps.transform8x8ModeFlag = readBitField(QStringLiteral("transform_8x8_mode_flag"));
        pps.picScalingMatrixPresentFlag = readBitField(QStringLiteral("pic_scaling_matrix_present_flag"));
        if (pps.picScalingMatrixPresentFlag) {
            const int chromaFormatIdc = sps.valid ? sps.chromaFormatIdc : 1;
            const int scalingListCount = 6 + ((chromaFormatIdc != 3) ? 2 : 6) * (pps.transform8x8ModeFlag ? 1 : 0);
            for (int i = 0; i < scalingListCount; ++i) {
                if (reader.readBit()) {
                    skipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
        }
        pps.secondChromaQpIndexOffset = readSEField(QStringLiteral("second_chroma_qp_index_offset"));
    }

    pps.valid = !reader.hasError();
    return pps;
}

SliceInfo H264Parser::parseSliceHeader(const QByteArray &rbsp, int nalUnitType, int nalRefIdc) const
{
    BitReader reader(rbsp);
    SliceInfo slice;
    auto addField = [&slice](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        slice.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readSEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto appendDiagnostic = [&slice](const QString &code, const QString &message) {
        slice.diagnostics.append({code, message});
        slice.macroblockParseWarnings.append(message);
    };
    auto appendHeaderTruncated = [&appendDiagnostic]() {
        appendDiagnostic(
            QStringLiteral("slice_header_truncated"),
            QStringLiteral("slice header ended unexpectedly; slice_data was not parsed."));
    };

    slice.nalUnitType = nalUnitType;
    slice.nalRefIdc = nalRefIdc;
    slice.firstMbInSlice = static_cast<int>(readUEField(QStringLiteral("first_mb_in_slice")));
    slice.sliceType = static_cast<int>(readUEField(QStringLiteral("slice_type")));
    slice.sliceTypeName = sliceTypeName(slice.sliceType);
    slice.picParameterSetId = static_cast<int>(readUEField(QStringLiteral("pic_parameter_set_id")));
    if (reader.hasError()) {
        appendHeaderTruncated();
        slice.valid = false;
        return slice;
    }

    const PpsInfo pps = m_ppsById.value(slice.picParameterSetId);
    const SpsInfo sps = m_spsById.value(pps.seqParameterSetId);
    if (!pps.valid || !sps.valid) {
        if (reader.hasError()) {
            appendHeaderTruncated();
        }
        slice.valid = !reader.hasError();
        return slice;
    }

    slice.frameNum = static_cast<int>(readBitsField(QStringLiteral("frame_num"), sps.log2MaxFrameNumMinus4 + 4));
    slice.picWidthInMbs = sps.picWidthInMbsMinus1 + 1;
    slice.picHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) * (sps.picHeightInMapUnitsMinus1 + 1);
    if (!sps.frameMbsOnlyFlag) {
        slice.fieldPicFlag = readBitField(QStringLiteral("field_pic_flag"));
        if (slice.fieldPicFlag) {
            slice.bottomFieldFlag = readBitField(QStringLiteral("bottom_field_flag"));
        }
    }

    if (nalUnitType == 5) {
        slice.idrPicId = static_cast<int>(readUEField(QStringLiteral("idr_pic_id")));
    }

    if (sps.picOrderCntType == 0) {
        slice.picOrderCntLsb = static_cast<int>(readBitsField(QStringLiteral("pic_order_cnt_lsb"), sps.log2MaxPicOrderCntLsbMinus4 + 4));
        if (pps.bottomFieldPicOrderInFramePresentFlag && !slice.fieldPicFlag) {
            readSEField(QStringLiteral("delta_pic_order_cnt_bottom"));
        }
    } else if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag) {
        readSEField(QStringLiteral("delta_pic_order_cnt[0]"));
        if (pps.bottomFieldPicOrderInFramePresentFlag && !slice.fieldPicFlag) {
            readSEField(QStringLiteral("delta_pic_order_cnt[1]"));
        }
    }

    const int normalizedSliceType = slice.sliceType % 5;
    if (pps.redundantPicCntPresentFlag) {
        readUEField(QStringLiteral("redundant_pic_cnt"));
    }

    if (normalizedSliceType == 1) {
        slice.directSpatialMvPredFlag = readBitField(QStringLiteral("direct_spatial_mv_pred_flag"));
    }

    int numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
    int numRefIdxL1ActiveMinus1 = pps.numRefIdxL1DefaultActiveMinus1;
    if (normalizedSliceType == 0 || normalizedSliceType == 1 || normalizedSliceType == 3) {
        slice.numRefIdxActiveOverrideFlag = readBitField(QStringLiteral("num_ref_idx_active_override_flag"));
        if (slice.numRefIdxActiveOverrideFlag) {
            numRefIdxL0ActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l0_active_minus1")));
            if (normalizedSliceType == 1) {
                numRefIdxL1ActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l1_active_minus1")));
            }
        }
    }
    slice.numRefIdxL0ActiveMinus1 = numRefIdxL0ActiveMinus1;
    slice.numRefIdxL1ActiveMinus1 = numRefIdxL1ActiveMinus1;

    auto parseRefPicListModification = [&reader, &readBitField](const QString &flagName, bool *flag) {
        *flag = readBitField(flagName);
        if (!*flag) {
            return QStringLiteral("not present");
        }
        QStringList operations;
        while (reader.bitsRemaining() > 0) {
            const quint32 modificationOfPicNumsIdc = reader.readUE();
            if (modificationOfPicNumsIdc == 3) {
                break;
            }
            if (modificationOfPicNumsIdc == 0 || modificationOfPicNumsIdc == 1) {
                const quint32 absDiffPicNumMinus1 = reader.readUE();
                operations.append(QStringLiteral("idc=%1 abs_diff_pic_num_minus1=%2")
                                      .arg(modificationOfPicNumsIdc)
                                      .arg(absDiffPicNumMinus1));
            } else if (modificationOfPicNumsIdc == 2) {
                const quint32 longTermPicNum = reader.readUE();
                operations.append(QStringLiteral("idc=2 long_term_pic_num=%1").arg(longTermPicNum));
            } else {
                operations.append(QStringLiteral("unsupported idc=%1").arg(modificationOfPicNumsIdc));
                break;
            }
        }
        return operations.isEmpty() ? QStringLiteral("present, no operations") : operations.join(QStringLiteral("; "));
    };

    if (normalizedSliceType != 2 && normalizedSliceType != 4) {
        slice.refPicListModificationSummary =
            QStringLiteral("L0: %1").arg(parseRefPicListModification(QStringLiteral("ref_pic_list_modification_flag_l0"),
                                                                     &slice.refPicListModificationFlagL0));
    }
    if (normalizedSliceType == 1) {
        const QString l1Summary = parseRefPicListModification(QStringLiteral("ref_pic_list_modification_flag_l1"),
                                                             &slice.refPicListModificationFlagL1);
        slice.refPicListModificationSummary =
            slice.refPicListModificationSummary.isEmpty()
            ? QStringLiteral("L1: %1").arg(l1Summary)
            : QStringLiteral("%1; L1: %2").arg(slice.refPicListModificationSummary, l1Summary);
    }

    auto parsePredWeightTable = [&reader, &sps](int l0Count, int l1Count, bool hasList1) {
        const int chromaArrayType = sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc;
        const quint32 lumaLog2WeightDenom = reader.readUE();
        quint32 chromaLog2WeightDenom = 0;
        if (chromaArrayType != 0) {
            chromaLog2WeightDenom = reader.readUE();
        }

        int lumaWeightFlags = 0;
        int chromaWeightFlags = 0;
        auto parseList = [&reader, chromaArrayType, &lumaWeightFlags, &chromaWeightFlags](int count) {
            for (int i = 0; i <= count; ++i) {
                if (reader.readBit()) {
                    ++lumaWeightFlags;
                    reader.readSE();
                    reader.readSE();
                }
                if (chromaArrayType != 0 && reader.readBit()) {
                    ++chromaWeightFlags;
                    for (int j = 0; j < 2; ++j) {
                        reader.readSE();
                        reader.readSE();
                    }
                }
            }
        };

        parseList(l0Count);
        if (hasList1) {
            parseList(l1Count);
        }
        return QStringLiteral("luma_denom=%1 chroma_denom=%2 luma_flags=%3 chroma_flags=%4")
            .arg(lumaLog2WeightDenom)
            .arg(chromaArrayType != 0 ? QString::number(chromaLog2WeightDenom) : QStringLiteral("-"))
            .arg(lumaWeightFlags)
            .arg(chromaWeightFlags);
    };

    const bool usePredWeightTable =
        (pps.weightedPredFlag && (normalizedSliceType == 0 || normalizedSliceType == 3))
        || (pps.weightedBipredIdc == 1 && normalizedSliceType == 1);
    if (usePredWeightTable) {
        slice.predWeightTablePresent = true;
        slice.predWeightTableSummary =
            parsePredWeightTable(numRefIdxL0ActiveMinus1, numRefIdxL1ActiveMinus1, normalizedSliceType == 1);
    }

    if (nalRefIdc != 0) {
        slice.decRefPicMarkingPresent = true;
        if (nalUnitType == 5) {
            slice.noOutputOfPriorPicsFlag = readBitField(QStringLiteral("no_output_of_prior_pics_flag"));
            slice.longTermReferenceFlag = readBitField(QStringLiteral("long_term_reference_flag"));
            slice.decRefPicMarkingSummary =
                QStringLiteral("IDR no_output=%1 long_term_reference=%2")
                    .arg(boolText(slice.noOutputOfPriorPicsFlag), boolText(slice.longTermReferenceFlag));
        } else {
            slice.adaptiveRefPicMarkingModeFlag = readBitField(QStringLiteral("adaptive_ref_pic_marking_mode_flag"));
            if (slice.adaptiveRefPicMarkingModeFlag) {
                QStringList operations;
                while (reader.bitsRemaining() > 0) {
                    const quint32 op = reader.readUE();
                    if (op == 0) {
                        break;
                    }
                    if (op == 1 || op == 3) {
                        operations.append(QStringLiteral("op=%1 difference_of_pic_nums_minus1=%2")
                                              .arg(op)
                                              .arg(reader.readUE()));
                    }
                    if (op == 2) {
                        operations.append(QStringLiteral("op=2 long_term_pic_num=%1").arg(reader.readUE()));
                    }
                    if (op == 3 || op == 6) {
                        operations.append(QStringLiteral("op=%1 long_term_frame_idx=%2")
                                              .arg(op)
                                              .arg(reader.readUE()));
                    }
                    if (op == 4) {
                        operations.append(QStringLiteral("op=4 max_long_term_frame_idx_plus1=%1").arg(reader.readUE()));
                    }
                    if (op > 6) {
                        operations.append(QStringLiteral("unsupported op=%1").arg(op));
                        break;
                    }
                }
                slice.decRefPicMarkingSummary =
                    operations.isEmpty() ? QStringLiteral("adaptive, no operations") : operations.join(QStringLiteral("; "));
            } else {
                slice.decRefPicMarkingSummary = QStringLiteral("not adaptive");
            }
        }
    }

    if (pps.entropyCodingModeFlag && normalizedSliceType != 2 && normalizedSliceType != 4) {
        readUEField(QStringLiteral("cabac_init_idc"));
    }
    slice.sliceQpDelta = readSEField(QStringLiteral("slice_qp_delta"));
    if (normalizedSliceType == 3 || normalizedSliceType == 4) {
        if (normalizedSliceType == 3) {
            readBitField(QStringLiteral("sp_for_switch_flag"));
        }
        readSEField(QStringLiteral("slice_qs_delta"));
    }
    if (pps.deblockingFilterControlPresentFlag) {
        const quint32 disableDeblockingFilterIdc = readUEField(QStringLiteral("disable_deblocking_filter_idc"));
        if (disableDeblockingFilterIdc != 1) {
            readSEField(QStringLiteral("slice_alpha_c0_offset_div2"));
            readSEField(QStringLiteral("slice_beta_offset_div2"));
        }
    }
    slice.derivedQp = std::clamp(26 + pps.picInitQpMinus26 + slice.sliceQpDelta, 0, 51);
    if (reader.hasError()) {
        appendHeaderTruncated();
    }
    slice.valid = !reader.hasError();

    if (slice.valid) {
        parseSliceData(reader, slice, pps, sps);
    }
    return slice;
}

void H264Parser::parseSliceData(BitReader &reader, SliceInfo &slice, const PpsInfo &pps, const SpsInfo &sps) const
{
    struct MacroblockMvState
    {
        bool valid = false;
        int refIndex = 0;
        int mvX = 0;
        int mvY = 0;
    };

    struct PartitionMv
    {
        int refIndex = 0;
        int mvdX = 0;
        int mvdY = 0;
        int mvX = 0;
        int mvY = 0;
    };

    enum class PredictionList
    {
        None,
        L0,
        L1,
        Bi
    };

    struct BPartitionModes
    {
        int partitionCount = 1;
        std::array<PredictionList, 2> modes {PredictionList::None, PredictionList::None};
        bool supported = false;
        QString unsupportedCode;
        QString unsupportedMessage;
    };

    struct CoeffToken
    {
        int totalCoeff = 0;
        int trailingOnes = 0;
        bool valid = false;
    };

    struct MacroblockCoeffState
    {
        int luma16x16Dc = 0;
        std::array<int, 16> luma {};
        std::array<std::array<int, 4>, 2> chroma {};
    };

    const int totalMacroblocks = std::max(1, slice.picWidthInMbs * slice.picHeightInMbs);
    int currentAddress = std::clamp(slice.firstMbInSlice, 0, totalMacroblocks - 1);
    int currentQp = slice.derivedQp;
    const int normalizedSliceType = slice.sliceType % 5;
    const bool isISlice = normalizedSliceType == 2 || normalizedSliceType == 4;
    const bool isPSlice = normalizedSliceType == 0 || normalizedSliceType == 3;
    const bool isBSlice = normalizedSliceType == 1;
    const int chromaArrayType = sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc;
    QVector<MacroblockMvState> mvStatesL0(totalMacroblocks);
    QVector<MacroblockMvState> mvStatesL1(totalMacroblocks);
    QVector<MacroblockCoeffState> coeffStates(totalMacroblocks);

    auto median = [](int a, int b, int c) {
        return a + b + c - std::min({a, b, c}) - std::max({a, b, c});
    };

    auto neighborMv = [&](const QVector<MacroblockMvState> &states, int address) {
        if (address < 0 || address >= states.size()) {
            return MacroblockMvState {};
        }
        return states[address];
    };

    auto predictMv = [&](const QVector<MacroblockMvState> &states, int address, int refIndex) {
        const int mbX = address % slice.picWidthInMbs;
        const int leftAddress = mbX > 0 ? address - 1 : -1;
        const int topAddress = address >= slice.picWidthInMbs ? address - slice.picWidthInMbs : -1;
        const int topRightAddress = (topAddress >= 0 && mbX + 1 < slice.picWidthInMbs)
            ? topAddress + 1
            : -1;
        const int topLeftAddress = (topAddress >= 0 && mbX > 0)
            ? topAddress - 1
            : -1;

        const MacroblockMvState a = neighborMv(states, leftAddress);
        const MacroblockMvState b = neighborMv(states, topAddress);
        MacroblockMvState c = neighborMv(states, topRightAddress);
        if (!c.valid) {
            c = neighborMv(states, topLeftAddress);
        }

        QVector<MacroblockMvState> matching;
        for (const MacroblockMvState &candidate : {a, b, c}) {
            if (candidate.valid && candidate.refIndex == refIndex) {
                matching.append(candidate);
            }
        }
        if (matching.size() == 1) {
            return matching.first();
        }

        MacroblockMvState result;
        result.valid = a.valid || b.valid || c.valid;
        result.refIndex = refIndex;
        result.mvX = median(a.valid ? a.mvX : 0, b.valid ? b.mvX : 0, c.valid ? c.mvX : 0);
        result.mvY = median(a.valid ? a.mvY : 0, b.valid ? b.mvY : 0, c.valid ? c.mvY : 0);
        return result;
    };

    auto addMotionVector = [&](MacroblockInfo &mb, int list, int refIndex, int mvX, int mvY) {
        MotionVectorInfo mv;
        mv.list = list;
        mv.referenceIndex = refIndex;
        mv.mvXQuarterPel = mvX;
        mv.mvYQuarterPel = mvY;
        mb.motionVectors.append(mv);
    };

    auto setMvState = [&](const MacroblockInfo &mb) {
        if (mb.address < 0 || mb.motionVectors.isEmpty()) {
            return;
        }
        for (const MotionVectorInfo &mv : mb.motionVectors) {
            QVector<MacroblockMvState> &states = mv.list == 1 ? mvStatesL1 : mvStatesL0;
            if (mb.address < states.size()) {
                states[mb.address] = {true, mv.referenceIndex, mv.mvXQuarterPel, mv.mvYQuarterPel};
            }
        }
    };

    auto readTE = [&](int range) {
        if (range <= 0) {
            return 0;
        }
        if (range == 1) {
            return reader.readBit() ? 0 : 1;
        }
        return static_cast<int>(reader.readUE());
    };

    auto bPartitionModes = [](int mbType) {
        BPartitionModes result;
        if (mbType == 0) {
            result.unsupportedCode = QStringLiteral("b_direct_macroblock_unsupported");
            result.unsupportedMessage = QStringLiteral("B_Direct motion vector derivation is not implemented.");
            return result;
        }
        if (mbType == 22) {
            result.unsupportedCode = QStringLiteral("b8x8_sub_macroblock_unsupported");
            result.unsupportedMessage = QStringLiteral("B_8x8 sub-macroblock motion vector parsing is not implemented.");
            return result;
        }
        if (mbType < 1 || mbType > 21) {
            result.unsupportedCode = QStringLiteral("b_slice_macroblock_unsupported");
            result.unsupportedMessage = QStringLiteral("Unsupported B-slice macroblock type %1.").arg(mbType);
            return result;
        }

        result.supported = true;
        if (mbType <= 3) {
            result.partitionCount = 1;
            result.modes[0] = mbType == 1 ? PredictionList::L0
                : (mbType == 2 ? PredictionList::L1 : PredictionList::Bi);
            return result;
        }

        result.partitionCount = 2;
        switch (mbType) {
        case 4:
        case 5:
            result.modes = {PredictionList::L0, PredictionList::L0};
            break;
        case 6:
        case 7:
            result.modes = {PredictionList::L1, PredictionList::L1};
            break;
        case 8:
        case 9:
            result.modes = {PredictionList::L0, PredictionList::L1};
            break;
        case 10:
        case 11:
            result.modes = {PredictionList::L1, PredictionList::L0};
            break;
        case 12:
        case 13:
            result.modes = {PredictionList::L0, PredictionList::Bi};
            break;
        case 14:
        case 15:
            result.modes = {PredictionList::L1, PredictionList::Bi};
            break;
        case 16:
        case 17:
            result.modes = {PredictionList::Bi, PredictionList::L0};
            break;
        case 18:
        case 19:
            result.modes = {PredictionList::Bi, PredictionList::L1};
            break;
        case 20:
        case 21:
            result.modes = {PredictionList::Bi, PredictionList::Bi};
            break;
        }
        return result;
    };

    auto appendDiagnostic = [&](const QString &code, const QString &message) {
        if (!message.isEmpty()) {
            slice.diagnostics.append({code, message});
            slice.macroblockParseWarnings.append(message);
        }
    };

    auto appendEstimatedRemainder = [&](const QString &code, const QString &message) {
        if (!message.isEmpty()) {
            appendDiagnostic(code, message);
        }
        for (int address = currentAddress; address < totalMacroblocks; ++address) {
            MacroblockInfo mb;
            mb.address = address;
            mb.mbType = QStringLiteral("Estimated");
            mb.predictionMode = QStringLiteral("unknown");
            mb.qp = currentQp;
            mb.note = message.isEmpty()
                ? QStringLiteral("QP carried forward after slice_data parsing stopped.")
                : message;
            slice.macroblocks.append(mb);
        }
    };

    auto luma4x4Index = [](int x, int y) {
        const int i8x8 = (y / 2) * 2 + (x / 2);
        const int i4x4 = (y % 2) * 2 + (x % 2);
        return i8x8 * 4 + i4x4;
    };

    auto luma4x4Coord = [](int index) {
        const int i8x8 = index / 4;
        const int i4x4 = index % 4;
        const int x = (i8x8 % 2) * 2 + (i4x4 % 2);
        const int y = (i8x8 / 2) * 2 + (i4x4 / 2);
        return std::array<int, 2> {x, y};
    };

    auto predictedLumaNonZero = [&](int address, int blockIndex) {
        const std::array<int, 2> coord = luma4x4Coord(blockIndex);
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (coord[0] > 0) {
            left = coeffStates[address].luma[luma4x4Index(coord[0] - 1, coord[1])];
            leftAvailable = true;
        } else if (mbX > 0) {
            left = coeffStates[address - 1].luma[luma4x4Index(3, coord[1])];
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (coord[1] > 0) {
            top = coeffStates[address].luma[luma4x4Index(coord[0], coord[1] - 1)];
            topAvailable = true;
        } else if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].luma[luma4x4Index(coord[0], 3)];
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto predictedIntra16x16DcNonZero = [&](int address) {
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (mbX > 0) {
            left = coeffStates[address - 1].luma16x16Dc;
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].luma16x16Dc;
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto chroma4x4Index = [](int x, int y) {
        return y * 2 + x;
    };

    auto chroma4x4Coord = [](int index) {
        return std::array<int, 2> {index % 2, index / 2};
    };

    auto predictedChromaNonZero = [&](int address, int component, int blockIndex) {
        const std::array<int, 2> coord = chroma4x4Coord(blockIndex);
        const int mbX = address % slice.picWidthInMbs;
        const int mbY = address / slice.picWidthInMbs;

        int left = 0;
        bool leftAvailable = false;
        if (coord[0] > 0) {
            left = coeffStates[address].chroma[component][chroma4x4Index(coord[0] - 1, coord[1])];
            leftAvailable = true;
        } else if (mbX > 0) {
            left = coeffStates[address - 1].chroma[component][chroma4x4Index(1, coord[1])];
            leftAvailable = true;
        }

        int top = 0;
        bool topAvailable = false;
        if (coord[1] > 0) {
            top = coeffStates[address].chroma[component][chroma4x4Index(coord[0], coord[1] - 1)];
            topAvailable = true;
        } else if (mbY > 0) {
            top = coeffStates[address - slice.picWidthInMbs].chroma[component][chroma4x4Index(coord[0], 1)];
            topAvailable = true;
        }

        if (leftAvailable && topAvailable) {
            return (left + top + 1) / 2;
        }
        return (leftAvailable ? left : 0) + (topAvailable ? top : 0);
    };

    auto readVlcValue = [&](const uint8_t *lengths, const uint8_t *bits, int entryCount) {
        int code = 0;
        for (int length = 1; length <= 16 && !reader.hasError(); ++length) {
            code = (code << 1) | (reader.readBit() ? 1 : 0);
            for (int value = 0; value < entryCount; ++value) {
                if (lengths[value] == length && bits[value] == code) {
                    return value;
                }
            }
        }
        return -1;
    };

    auto readCoeffToken = [&](int nC, int maxNumCoeff) {
        static constexpr uint8_t coeffTokenLen[4][68] = {
            {
                1, 0, 0, 0,
                6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5,
                10, 9, 8, 6, 11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9,
                13, 13, 13, 10, 14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14,
                15, 15, 15, 14, 16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16,
                16, 16, 16, 16
            },
            {
                2, 0, 0, 0,
                6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4,
                8, 6, 6, 4, 8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6,
                11, 11, 11, 7, 12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11,
                13, 13, 13, 12, 13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13,
                14, 14, 14, 14
            },
            {
                4, 0, 0, 0,
                6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4,
                7, 5, 5, 4, 7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4,
                8, 7, 7, 5, 8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8,
                9, 9, 9, 8, 10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10,
                10, 10, 10, 10
            },
            {
                6, 0, 0, 0,
                6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                6, 6, 6, 6
            }
        };
        static constexpr uint8_t coeffTokenBits[4][68] = {
            {
                1, 0, 0, 0,
                5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3,
                7, 6, 5, 3, 7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4,
                8, 10, 13, 4, 15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12,
                11, 10, 13, 8, 15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12,
                4, 6, 5, 8
            },
            {
                3, 0, 0, 0,
                11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5,
                7, 6, 5, 4, 4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4,
                11, 14, 13, 4, 15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8,
                15, 14, 13, 12, 11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1,
                7, 6, 5, 4
            },
            {
                15, 0, 0, 0,
                15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12,
                15, 10, 11, 11, 11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8,
                15, 14, 13, 13, 11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12,
                8, 10, 13, 8, 13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6,
                1, 4, 3, 2
            },
            {
                3, 0, 0, 0,
                0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11,
                12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
                28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                60, 61, 62, 63
            }
        };
        static constexpr uint8_t chromaDcCoeffTokenLen[20] = {
            2, 0, 0, 0,
            6, 1, 0, 0,
            6, 6, 3, 0,
            6, 7, 7, 6,
            6, 8, 8, 7
        };
        static constexpr uint8_t chromaDcCoeffTokenBits[20] = {
            1, 0, 0, 0,
            7, 1, 0, 0,
            4, 6, 1, 0,
            3, 3, 2, 5,
            2, 3, 2, 0
        };

        const uint8_t *lengths = nullptr;
        const uint8_t *bits = nullptr;
        int entryCount = 0;
        if (nC < 0) {
            lengths = chromaDcCoeffTokenLen;
            bits = chromaDcCoeffTokenBits;
            entryCount = 20;
        } else {
            const int table = nC < 2 ? 0 : (nC < 4 ? 1 : (nC < 8 ? 2 : 3));
            lengths = coeffTokenLen[table];
            bits = coeffTokenBits[table];
            entryCount = 68;
        }

        const int value = readVlcValue(lengths, bits, entryCount);
        CoeffToken token;
        if (value < 0) {
            return token;
        }

        token.totalCoeff = value / 4;
        token.trailingOnes = value % 4;
        token.valid = token.totalCoeff <= maxNumCoeff
            && token.trailingOnes <= std::min(3, token.totalCoeff);
        return token;
    };

    auto readTotalZeros = [&](int totalCoeff, int maxNumCoeff, bool chromaDc) {
        static constexpr uint8_t totalZerosLen[15][16] = {
            {1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
            {3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6, 0},
            {4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6, 0, 0},
            {5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5, 0, 0, 0},
            {4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5, 0, 0, 0, 0},
            {6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6, 0, 0, 0, 0, 0},
            {6, 5, 3, 3, 3, 2, 3, 4, 3, 6, 0, 0, 0, 0, 0, 0},
            {6, 4, 5, 3, 2, 2, 3, 3, 6, 0, 0, 0, 0, 0, 0, 0},
            {6, 6, 4, 2, 2, 3, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0},
            {5, 5, 3, 2, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {4, 4, 3, 3, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {4, 4, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
        };
        static constexpr uint8_t totalZerosBits[15][16] = {
            {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1},
            {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0, 0},
            {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0, 0, 0},
            {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0, 0, 0, 0},
            {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0},
            {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0},
            {1, 1, 5, 4, 3, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 1, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 0, 1, 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 0, 1, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
        };
        static constexpr uint8_t chromaDcTotalZerosLen[3][4] = {
            {1, 2, 3, 3},
            {1, 2, 2, 0},
            {1, 1, 0, 0}
        };
        static constexpr uint8_t chromaDcTotalZerosBits[3][4] = {
            {1, 1, 1, 0},
            {1, 1, 0, 0},
            {1, 0, 0, 0}
        };

        if (totalCoeff <= 0 || totalCoeff >= maxNumCoeff) {
            return 0;
        }

        const int maxZeros = maxNumCoeff - totalCoeff;
        const uint8_t *lengths = chromaDc ? chromaDcTotalZerosLen[totalCoeff - 1] : totalZerosLen[totalCoeff - 1];
        const uint8_t *bits = chromaDc ? chromaDcTotalZerosBits[totalCoeff - 1] : totalZerosBits[totalCoeff - 1];
        return readVlcValue(lengths, bits, maxZeros + 1);
    };

    auto readRunBefore = [&](int zerosLeft) {
        static constexpr uint8_t runLen[7][16] = {
            {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 2, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}
        };
        static constexpr uint8_t runBits[7][16] = {
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 2, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 0, 1, 3, 2, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}
        };

        if (zerosLeft <= 0) {
            return 0;
        }
        const int row = std::min(zerosLeft, 7) - 1;
        return readVlcValue(runLen[row], runBits[row], zerosLeft + 1);
    };

    auto parseResidualBlockCavlc = [&](int nC, int maxNumCoeff) {
        const bool chromaDc = nC < 0;
        const CoeffToken token = readCoeffToken(nC, maxNumCoeff);
        if (!token.valid) {
            return -1;
        }

        int suffixLength = token.totalCoeff > 10 && token.trailingOnes < 3 ? 1 : 0;
        for (int i = 0; i < token.totalCoeff; ++i) {
            if (i < token.trailingOnes) {
                reader.readBit();
            } else {
                int levelPrefix = 0;
                while (!reader.hasError() && !reader.readBit()) {
                    ++levelPrefix;
                    if (levelPrefix > 15) {
                        break;
                    }
                }

                int levelSuffixSize = suffixLength;
                if (levelPrefix == 14 && suffixLength == 0) {
                    levelSuffixSize = 4;
                } else if (levelPrefix == 15) {
                    levelSuffixSize = 12;
                }

                int levelCode = (std::min(levelPrefix, 15) << suffixLength);
                if (levelSuffixSize > 0) {
                    levelCode += static_cast<int>(reader.readBits(levelSuffixSize));
                }
                if (levelPrefix == 15 && suffixLength == 0) {
                    levelCode += 15;
                }
                if (token.trailingOnes < 3) {
                    levelCode += 2;
                }

                const int absLevel = (levelCode + 2) / 2;
                if (suffixLength == 0) {
                    suffixLength = 1;
                }
                if (absLevel > (3 << (suffixLength - 1)) && suffixLength < 6) {
                    ++suffixLength;
                }
            }
        }

        const int totalZeros = readTotalZeros(token.totalCoeff, maxNumCoeff, chromaDc);
        if (totalZeros < 0 || reader.hasError()) {
            return -1;
        }

        int zerosLeft = totalZeros;
        for (int i = 0; i < token.totalCoeff - 1 && zerosLeft > 0; ++i) {
            const int runBefore = readRunBefore(zerosLeft);
            if (runBefore < 0 || reader.hasError()) {
                return -1;
            }
            zerosLeft -= runBefore;
        }
        return token.totalCoeff;
    };

    auto parseResidualBlock = [&](MacroblockInfo &mb, int nC, int maxNumCoeff) {
        const int totalCoeff = parseResidualBlockCavlc(nC, maxNumCoeff);
        if (totalCoeff < 0) {
            return -1;
        }
        ++mb.residualBlockCount;
        mb.residualCoefficientCount += totalCoeff;
        return totalCoeff;
    };

    auto parseResidual = [&](MacroblockInfo &mb,
                             bool intra16x16,
                             bool transform8x8,
                             MacroblockCoeffState &coeffState) {
        Q_UNUSED(transform8x8);

        if (intra16x16) {
            const int totalCoeff = parseResidualBlock(mb, predictedIntra16x16DcNonZero(mb.address), 16);
            if (totalCoeff < 0) {
                return false;
            }
            coeffState.luma16x16Dc = totalCoeff;
        }

        if (intra16x16) {
            for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
                if (mb.codedBlockPatternLuma > 0) {
                    const int totalCoeff = parseResidualBlock(mb, predictedLumaNonZero(mb.address, blockIndex), 15);
                    if (totalCoeff < 0) {
                        return false;
                    }
                    coeffState.luma[blockIndex] = totalCoeff;
                }
            }
        } else {
            for (int i8x8 = 0; i8x8 < 4; ++i8x8) {
                if (((mb.codedBlockPatternLuma >> i8x8) & 0x01) == 0) {
                    continue;
                }
                for (int i4x4 = 0; i4x4 < 4; ++i4x4) {
                    const int blockIndex = i8x8 * 4 + i4x4;
                    const int totalCoeff = parseResidualBlock(mb, predictedLumaNonZero(mb.address, blockIndex), 16);
                    if (totalCoeff < 0) {
                        return false;
                    }
                    coeffState.luma[blockIndex] = totalCoeff;
                }
            }
        }

        if (chromaArrayType == 0 || mb.codedBlockPatternChroma == 0) {
            return true;
        }
        if (chromaArrayType != 1) {
            appendDiagnostic(
                QStringLiteral("chroma_residual_unsupported"),
                QStringLiteral("CAVLC residual parsing currently supports 4:2:0 chroma only."));
            return false;
        }

        if ((mb.codedBlockPatternChroma & 0x03) != 0) {
            for (int component = 0; component < 2; ++component) {
                if (parseResidualBlock(mb, -1, 4) < 0) {
                    return false;
                }
            }
        }

        if ((mb.codedBlockPatternChroma & 0x02) != 0) {
            for (int component = 0; component < 2; ++component) {
                for (int blockIndex = 0; blockIndex < 4; ++blockIndex) {
                    const int totalCoeff =
                        parseResidualBlock(mb, predictedChromaNonZero(mb.address, component, blockIndex), 15);
                    if (totalCoeff < 0) {
                        return false;
                    }
                    coeffState.chroma[component][blockIndex] = totalCoeff;
                }
            }
        }

        return true;
    };

    if (pps.entropyCodingModeFlag) {
        appendEstimatedRemainder(
            QStringLiteral("cabac_unsupported"),
            QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header."));
        return;
    }

    if (!sps.frameMbsOnlyFlag || pps.numSliceGroupsMinus1 > 0) {
        appendEstimatedRemainder(
            QStringLiteral("interlaced_or_fmo_unsupported"),
            QStringLiteral("Interlaced/MBAFF or FMO slice_data parsing is not implemented; macroblock QP is carried forward from the slice header."));
        return;
    }

    while (currentAddress < totalMacroblocks && reader.moreRbspData() && !reader.hasError()) {
        if (!isISlice) {
            const quint32 mbSkipRun = reader.readUE();
            if (reader.hasError()) {
                break;
            }
            for (quint32 i = 0; i < mbSkipRun && currentAddress < totalMacroblocks; ++i) {
                MacroblockInfo mb;
                mb.address = currentAddress++;
                mb.mbType = isPSlice ? QStringLiteral("P_Skip") : QStringLiteral("B_Skip/Direct");
                mb.predictionMode = isPSlice ? QStringLiteral("Pred_L0") : QStringLiteral("Direct");
                mb.codedBlockPattern = 0;
                mb.codedBlockPatternLuma = 0;
                mb.codedBlockPatternChroma = 0;
                mb.qp = currentQp;
                mb.skipped = true;
                mb.parsed = true;
                if (isPSlice) {
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, 0);
                    addMotionVector(mb, 0, 0, predicted.mvX, predicted.mvY);
                    setMvState(mb);
                    mb.note = QStringLiteral("Parsed from mb_skip_run; P_Skip motion vector uses neighboring median prediction.");
                } else {
                    mb.note = QStringLiteral("Parsed from mb_skip_run; B direct motion vector parsing is not implemented.");
                }
                coeffStates[mb.address] = MacroblockCoeffState {};
                slice.macroblocks.append(mb);
            }
            if (mbSkipRun > 0 && !reader.moreRbspData()) {
                break;
            }
            if (currentAddress >= totalMacroblocks) {
                break;
            }
        }

        const quint32 rawMbType = reader.readUE();
        if (reader.hasError()) {
            break;
        }

        MacroblockInfo mb;
        mb.address = currentAddress;
        mb.qp = currentQp;
        mb.parsed = true;

        bool intra = isISlice;
        int localMbType = static_cast<int>(rawMbType);
        if (isPSlice && localMbType >= 5) {
            intra = true;
            localMbType -= 5;
        } else if (isBSlice && localMbType >= 23) {
            intra = true;
            localMbType -= 23;
        }

        const bool intra16x16 = intra && localMbType >= 1 && localMbType <= 24;
        const bool iPcm = intra && localMbType == 25;
        const bool p8x8 = isPSlice && !intra && (localMbType == 3 || localMbType == 4);
        bool transform8x8 = false;

        if (intra) {
            mb.mbType = intraMbTypeName(localMbType);
            mb.predictionMode = intra16x16 ? QStringLiteral("Intra_16x16")
                                           : (iPcm ? QStringLiteral("I_PCM") : QStringLiteral("Intra_4x4/8x8"));
        } else if (isPSlice) {
            mb.mbType = pMbTypeName(localMbType);
            mb.predictionMode = QStringLiteral("Pred_L0");
        } else if (isBSlice) {
            mb.mbType = bMbTypeName(localMbType);
            mb.predictionMode = QStringLiteral("BiPred");
        } else {
            mb.mbType = QStringLiteral("Unsupported");
            mb.predictionMode = QStringLiteral("unsupported");
            mb.note = QStringLiteral("This slice type is not supported for macroblock parsing.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            appendEstimatedRemainder(QStringLiteral("slice_macroblock_unsupported"), mb.note);
            return;
        }

        if (iPcm) {
            mb.note = QStringLiteral("I_PCM macroblock payload is byte aligned raw samples; skipping exact payload is not implemented.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            appendEstimatedRemainder(QStringLiteral("i_pcm_unsupported"), mb.note);
            return;
        }

        if (intra && !intra16x16) {
            transform8x8 = pps.transform8x8ModeFlag && reader.readBit();
            const int predictionBlocks = transform8x8 ? 4 : 16;
            for (int i = 0; i < predictionBlocks; ++i) {
                if (!reader.readBit()) {
                    reader.readBits(3);
                }
            }
            reader.readUE();
        } else if (intra16x16) {
            reader.readUE();
        } else if (isPSlice) {
            int partitionCount = 1;
            std::array<PartitionMv, 2> partitions;
            if (localMbType == 1 || localMbType == 2) {
                partitionCount = 2;
            } else if (p8x8) {
                struct SubMacroblock
                {
                    int type = 0;
                    int refIndex = 0;
                    int partitionCount = 1;
                };

                std::array<SubMacroblock, 4> subMacroblocks;
                for (SubMacroblock &subMb : subMacroblocks) {
                    subMb.type = static_cast<int>(reader.readUE());
                    switch (subMb.type) {
                    case 0:
                        subMb.partitionCount = 1;
                        break;
                    case 1:
                    case 2:
                        subMb.partitionCount = 2;
                        break;
                    case 3:
                        subMb.partitionCount = 4;
                        break;
                    default:
                        mb.note = QStringLiteral("Unsupported P sub_mb_type %1; remaining macroblocks are estimated.")
                                      .arg(subMb.type);
                        currentAddress = mb.address + 1;
                        slice.macroblocks.append(mb);
                        appendEstimatedRemainder(QStringLiteral("p8x8_sub_macroblock_type_unsupported"), mb.note);
                        return;
                    }
                }

                if (slice.numRefIdxL0ActiveMinus1 > 0 && localMbType != 4) {
                    for (SubMacroblock &subMb : subMacroblocks) {
                        subMb.refIndex = readTE(slice.numRefIdxL0ActiveMinus1);
                    }
                }

                for (const SubMacroblock &subMb : subMacroblocks) {
                    for (int part = 0; part < subMb.partitionCount; ++part) {
                        const int mvdX = reader.readSE();
                        const int mvdY = reader.readSE();
                        const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, subMb.refIndex);
                        addMotionVector(mb, 0, subMb.refIndex, predicted.mvX + mvdX, predicted.mvY + mvdY);
                    }
                }
                setMvState(mb);
                mb.predictionMode = QStringLiteral("Pred_L0 sub-macroblock");
            }

            if (!p8x8 && slice.numRefIdxL0ActiveMinus1 > 0) {
                for (int i = 0; i < partitionCount; ++i) {
                    partitions[i].refIndex = readTE(slice.numRefIdxL0ActiveMinus1);
                }
            }
            if (!p8x8) {
                for (int i = 0; i < partitionCount; ++i) {
                    partitions[i].mvdX = reader.readSE();
                    partitions[i].mvdY = reader.readSE();
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, partitions[i].refIndex);
                    partitions[i].mvX = predicted.mvX + partitions[i].mvdX;
                    partitions[i].mvY = predicted.mvY + partitions[i].mvdY;
                    addMotionVector(mb, 0, partitions[i].refIndex, partitions[i].mvX, partitions[i].mvY);
                }
                setMvState(mb);
            }
        } else if (isBSlice) {
            const BPartitionModes modes = bPartitionModes(localMbType);
            if (!modes.supported) {
                mb.note = modes.unsupportedMessage;
                currentAddress = mb.address + 1;
                slice.macroblocks.append(mb);
                appendEstimatedRemainder(modes.unsupportedCode, mb.note);
                return;
            }

            std::array<PartitionMv, 2> l0Partitions;
            std::array<PartitionMv, 2> l1Partitions;
            if (slice.numRefIdxL0ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == PredictionList::L0 || modes.modes[i] == PredictionList::Bi) {
                        l0Partitions[i].refIndex = readTE(slice.numRefIdxL0ActiveMinus1);
                    }
                }
            }
            if (slice.numRefIdxL1ActiveMinus1 > 0) {
                for (int i = 0; i < modes.partitionCount; ++i) {
                    if (modes.modes[i] == PredictionList::L1 || modes.modes[i] == PredictionList::Bi) {
                        l1Partitions[i].refIndex = readTE(slice.numRefIdxL1ActiveMinus1);
                    }
                }
            }

            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == PredictionList::L0 || modes.modes[i] == PredictionList::Bi) {
                    l0Partitions[i].mvdX = reader.readSE();
                    l0Partitions[i].mvdY = reader.readSE();
                    const MacroblockMvState predicted = predictMv(mvStatesL0, mb.address, l0Partitions[i].refIndex);
                    l0Partitions[i].mvX = predicted.mvX + l0Partitions[i].mvdX;
                    l0Partitions[i].mvY = predicted.mvY + l0Partitions[i].mvdY;
                    addMotionVector(mb, 0, l0Partitions[i].refIndex, l0Partitions[i].mvX, l0Partitions[i].mvY);
                }
            }
            for (int i = 0; i < modes.partitionCount; ++i) {
                if (modes.modes[i] == PredictionList::L1 || modes.modes[i] == PredictionList::Bi) {
                    l1Partitions[i].mvdX = reader.readSE();
                    l1Partitions[i].mvdY = reader.readSE();
                    const MacroblockMvState predicted = predictMv(mvStatesL1, mb.address, l1Partitions[i].refIndex);
                    l1Partitions[i].mvX = predicted.mvX + l1Partitions[i].mvdX;
                    l1Partitions[i].mvY = predicted.mvY + l1Partitions[i].mvdY;
                    addMotionVector(mb, 1, l1Partitions[i].refIndex, l1Partitions[i].mvX, l1Partitions[i].mvY);
                }
            }
            setMvState(mb);
        }

        if (reader.hasError()) {
            break;
        }

        if (intra16x16) {
            const int typeOffset = localMbType - 1;
            mb.codedBlockPatternChroma = (typeOffset / 4) % 3;
            mb.codedBlockPatternLuma = (typeOffset / 12) != 0 ? 15 : 0;
            mb.codedBlockPattern = mb.codedBlockPatternLuma | (mb.codedBlockPatternChroma << 4);
        } else {
            mb.codedBlockPattern = codedBlockPatternFromCodeNum(reader.readUE(), intra, chromaArrayType);
            mb.codedBlockPatternLuma = mb.codedBlockPattern & 0x0f;
            mb.codedBlockPatternChroma = (mb.codedBlockPattern >> 4) & 0x03;
            if (mb.codedBlockPatternLuma > 0 && pps.transform8x8ModeFlag && !intra) {
                transform8x8 = reader.readBit();
            }
        }

        const bool hasResidual = intra16x16 || mb.codedBlockPatternLuma > 0 || mb.codedBlockPatternChroma > 0;
        MacroblockCoeffState coeffState;
        if (hasResidual) {
            mb.mbQpDelta = reader.readSE();
            currentQp = (currentQp + mb.mbQpDelta + 52) % 52;
            mb.qp = currentQp;
            if (!parseResidual(mb, intra16x16, transform8x8, coeffState) || reader.hasError()) {
                mb.note = QStringLiteral("Parsed macroblock header and mb_qp_delta, but residual CAVLC parsing stopped on malformed or unsupported residual data.");
                currentAddress = mb.address + 1;
                slice.macroblocks.append(mb);
                appendEstimatedRemainder(
                    QStringLiteral("cavlc_residual_parse_failed"),
                    QStringLiteral("CAVLC residual data could not be fully parsed; remaining macroblocks are estimated."));
                slice.macroblocksParsed = !slice.macroblocks.isEmpty();
                return;
            }
            mb.residualParsed = true;
            coeffStates[mb.address] = coeffState;
            mb.note = QStringLiteral("Parsed macroblock header, mb_qp_delta, and CAVLC residual data.");
            currentAddress = mb.address + 1;
            slice.macroblocks.append(mb);
            continue;
        }

        mb.qp = currentQp;
        coeffStates[mb.address] = coeffState;
        mb.note = QStringLiteral("Parsed macroblock header; no residual data present.");
        currentAddress = mb.address + 1;
        slice.macroblocks.append(mb);
    }

    if (reader.hasError()) {
        appendEstimatedRemainder(
            QStringLiteral("slice_data_truncated"),
            QStringLiteral("slice_data ended unexpectedly; remaining macroblocks are estimated."));
    }

    if (slice.macroblocks.isEmpty()) {
        appendEstimatedRemainder(
            QStringLiteral("slice_data_missing"),
            QStringLiteral("No macroblock data could be parsed; QP is carried forward from the slice header."));
    }

    slice.macroblocksParsed = !slice.macroblocks.isEmpty();
}

QByteArray H264Parser::rbspFromEbsp(const uint8_t *data, qsizetype size)
{
    QByteArray rbsp;
    rbsp.reserve(size);
    int zeroCount = 0;

    for (qsizetype i = 0; i < size; ++i) {
        const uint8_t byte = data[i];
        if (zeroCount == 2 && byte == 0x03) {
            zeroCount = 0;
            continue;
        }
        rbsp.append(static_cast<char>(byte));
        zeroCount = byte == 0 ? zeroCount + 1 : 0;
    }

    return rbsp;
}

bool H264Parser::hasAnnexBStartCode(const QByteArray &data)
{
    const qsizetype scanLimit = std::min<qsizetype>(data.size(), 64);
    for (qsizetype i = 0; i < scanLimit; ++i) {
        if (startCodeSizeAt(data, i) != 0) {
            return true;
        }
    }
    return false;
}

qsizetype H264Parser::startCodeSizeAt(const QByteArray &data, qsizetype offset)
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

int H264Parser::readBigEndianLength(const uint8_t *data, int size)
{
    int value = 0;
    for (int i = 0; i < size; ++i) {
        value = (value << 8) | data[i];
    }
    return value;
}

void H264Parser::skipScalingList(BitReader &reader, int sizeOfScalingList)
{
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < sizeOfScalingList; ++j) {
        if (nextScale != 0) {
            const int deltaScale = reader.readSE();
            nextScale = (lastScale + deltaScale + 256) % 256;
        }
        lastScale = nextScale == 0 ? lastScale : nextScale;
    }
}

int H264Parser::codedBlockPatternFromCodeNum(quint32 codeNum, bool intra, int chromaArrayType)
{
    static constexpr int codedBlockPatternChromaTable[48][2] = {
        {0, 47}, {16, 31}, {1, 15}, {2, 0}, {4, 23}, {8, 27}, {32, 29}, {3, 30},
        {5, 7}, {10, 11}, {12, 13}, {15, 14}, {47, 39}, {7, 43}, {11, 45}, {13, 46},
        {14, 16}, {6, 3}, {9, 5}, {31, 10}, {35, 12}, {37, 19}, {42, 21}, {44, 26},
        {33, 28}, {34, 35}, {36, 37}, {40, 42}, {39, 44}, {43, 33}, {45, 34}, {46, 36},
        {17, 40}, {18, 8}, {20, 17}, {24, 18}, {19, 20}, {21, 24}, {26, 6}, {28, 9},
        {23, 22}, {27, 25}, {29, 32}, {30, 38}, {22, 41}, {25, 4}, {38, 1}, {41, 2}
    };

    static constexpr int codedBlockPatternMonoTable[16][2] = {
        {0, 15}, {1, 0}, {2, 7}, {4, 11}, {8, 13}, {3, 14}, {5, 3}, {10, 5},
        {12, 10}, {15, 12}, {7, 1}, {11, 2}, {13, 4}, {14, 8}, {6, 6}, {9, 9}
    };

    if (chromaArrayType == 1 || chromaArrayType == 2) {
        if (codeNum >= 48) {
            return 0;
        }
        return codedBlockPatternChromaTable[codeNum][intra ? 1 : 0];
    }

    if (codeNum >= 16) {
        return 0;
    }
    return codedBlockPatternMonoTable[codeNum][intra ? 1 : 0];
}

QString H264Parser::intraMbTypeName(int mbType)
{
    if (mbType == 0) {
        return QStringLiteral("I_NxN");
    }
    if (mbType == 25) {
        return QStringLiteral("I_PCM");
    }
    if (mbType >= 1 && mbType <= 24) {
        return QStringLiteral("I_16x16");
    }
    return QStringLiteral("I_Unknown(%1)").arg(mbType);
}

QString H264Parser::pMbTypeName(int mbType)
{
    switch (mbType) {
    case 0: return QStringLiteral("P_L0_16x16");
    case 1: return QStringLiteral("P_L0_L0_16x8");
    case 2: return QStringLiteral("P_L0_L0_8x16");
    case 3: return QStringLiteral("P_8x8");
    case 4: return QStringLiteral("P_8x8ref0");
    default: return QStringLiteral("P_Unknown(%1)").arg(mbType);
    }
}

QString H264Parser::bMbTypeName(int mbType)
{
    switch (mbType) {
    case 0: return QStringLiteral("B_Direct_16x16");
    case 1: return QStringLiteral("B_L0_16x16");
    case 2: return QStringLiteral("B_L1_16x16");
    case 3: return QStringLiteral("B_Bi_16x16");
    case 4: return QStringLiteral("B_L0_L0_16x8");
    case 5: return QStringLiteral("B_L0_L0_8x16");
    case 6: return QStringLiteral("B_L1_L1_16x8");
    case 7: return QStringLiteral("B_L1_L1_8x16");
    case 8: return QStringLiteral("B_L0_L1_16x8");
    case 9: return QStringLiteral("B_L0_L1_8x16");
    case 10: return QStringLiteral("B_L1_L0_16x8");
    case 11: return QStringLiteral("B_L1_L0_8x16");
    case 12: return QStringLiteral("B_L0_Bi_16x8");
    case 13: return QStringLiteral("B_L0_Bi_8x16");
    case 14: return QStringLiteral("B_L1_Bi_16x8");
    case 15: return QStringLiteral("B_L1_Bi_8x16");
    case 16: return QStringLiteral("B_Bi_L0_16x8");
    case 17: return QStringLiteral("B_Bi_L0_8x16");
    case 18: return QStringLiteral("B_Bi_L1_16x8");
    case 19: return QStringLiteral("B_Bi_L1_8x16");
    case 20: return QStringLiteral("B_Bi_Bi_16x8");
    case 21: return QStringLiteral("B_Bi_Bi_8x16");
    case 22: return QStringLiteral("B_8x8");
    default: return QStringLiteral("B_Unknown(%1)").arg(mbType);
    }
}
