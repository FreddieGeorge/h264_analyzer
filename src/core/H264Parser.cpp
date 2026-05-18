#include "core/H264Parser.h"

#include <QtEndian>

#include <algorithm>

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

private:
    const uint8_t *m_data = nullptr;
    qsizetype m_sizeBits = 0;
    qsizetype m_bitOffset = 0;
    bool m_error = false;
};

H264Parser::H264Parser() = default;

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

FrameSyntaxInfo H264Parser::parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    FrameSyntaxInfo frame;
    frame.index = packetIndex;
    frame.pts = pts;
    frame.dts = dts;

    frame.nalus = splitNalus(packetData);
    for (NaluInfo &nalu : frame.nalus) {
        if (nalu.sps.valid) {
            m_spsById.insert(nalu.sps.seqParameterSetId, nalu.sps);
        }
        if (nalu.pps.valid) {
            m_ppsById.insert(nalu.pps.picParameterSetId, nalu.pps);
        }
        if (nalu.slice.valid) {
            frame.slices.append(nalu.slice);
            if (frame.frameNum < 0) {
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

QVector<NaluInfo> H264Parser::splitNalus(const QByteArray &packetData)
{
    return hasAnnexBStartCode(packetData)
        ? splitAnnexBNalus(packetData)
        : splitLengthPrefixedNalus(packetData);
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

QVector<NaluInfo> H264Parser::splitLengthPrefixedNalus(const QByteArray &packetData)
{
    QVector<NaluInfo> nalus;
    qsizetype offset = 0;

    while (offset + m_nalLengthSize <= packetData.size()) {
        const int naluLength = readBigEndianLength(
            reinterpret_cast<const uint8_t *>(packetData.constData() + offset),
            m_nalLengthSize);
        offset += m_nalLengthSize;

        if (naluLength <= 0 || offset + naluLength > packetData.size()) {
            break;
        }

        NaluInfo base;
        base.offset = offset;
        base.size = naluLength;
        const QByteArray nalu = packetData.mid(offset, naluLength);
        nalus.append(parseNaluPayload(base, nalu));
        offset += naluLength;
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
        }
    } else if (info.nalUnitType == 8) {
        info.pps = parsePps(rbsp);
        if (info.pps.valid) {
            m_ppsById.insert(info.pps.picParameterSetId, info.pps);
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
    sps.profileIdc = static_cast<int>(reader.readBits(8));
    reader.readBits(8);
    sps.levelIdc = static_cast<int>(reader.readBits(8));
    sps.seqParameterSetId = static_cast<int>(reader.readUE());

    const bool highProfile = sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122
        || sps.profileIdc == 244 || sps.profileIdc == 44 || sps.profileIdc == 83
        || sps.profileIdc == 86 || sps.profileIdc == 118 || sps.profileIdc == 128
        || sps.profileIdc == 138 || sps.profileIdc == 139 || sps.profileIdc == 134 || sps.profileIdc == 135;

    if (highProfile) {
        sps.chromaFormatIdc = static_cast<int>(reader.readUE());
        if (sps.chromaFormatIdc == 3) {
            reader.readBit();
        }
        reader.readUE();
        reader.readUE();
        reader.readBit();
        if (reader.readBit()) {
            const int scalingListCount = sps.chromaFormatIdc != 3 ? 8 : 12;
            for (int i = 0; i < scalingListCount; ++i) {
                if (reader.readBit()) {
                    skipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
        }
    }

    sps.log2MaxFrameNumMinus4 = static_cast<int>(reader.readUE());
    sps.picOrderCntType = static_cast<int>(reader.readUE());
    if (sps.picOrderCntType == 0) {
        sps.log2MaxPicOrderCntLsbMinus4 = static_cast<int>(reader.readUE());
    } else if (sps.picOrderCntType == 1) {
        reader.readBit();
        reader.readSE();
        reader.readSE();
        const quint32 cycleCount = reader.readUE();
        for (quint32 i = 0; i < cycleCount; ++i) {
            reader.readSE();
        }
    }

    reader.readUE();
    reader.readBit();
    sps.picWidthInMbsMinus1 = static_cast<int>(reader.readUE());
    sps.picHeightInMapUnitsMinus1 = static_cast<int>(reader.readUE());
    sps.frameMbsOnlyFlag = reader.readBit();
    if (!sps.frameMbsOnlyFlag) {
        reader.readBit();
    }
    reader.readBit();
    if (reader.readBit()) {
        sps.frameCropLeftOffset = static_cast<int>(reader.readUE());
        sps.frameCropRightOffset = static_cast<int>(reader.readUE());
        sps.frameCropTopOffset = static_cast<int>(reader.readUE());
        sps.frameCropBottomOffset = static_cast<int>(reader.readUE());
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
    pps.picParameterSetId = static_cast<int>(reader.readUE());
    pps.seqParameterSetId = static_cast<int>(reader.readUE());
    pps.entropyCodingModeFlag = reader.readBit();
    pps.bottomFieldPicOrderInFramePresentFlag = reader.readBit();
    pps.numSliceGroupsMinus1 = static_cast<int>(reader.readUE());

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

    pps.numRefIdxL0DefaultActiveMinus1 = static_cast<int>(reader.readUE());
    pps.numRefIdxL1DefaultActiveMinus1 = static_cast<int>(reader.readUE());
    pps.weightedPredFlag = reader.readBit();
    pps.weightedBipredIdc = static_cast<int>(reader.readBits(2));
    pps.picInitQpMinus26 = reader.readSE();
    reader.readSE();
    reader.readSE();
    pps.deblockingFilterControlPresentFlag = reader.readBit();
    reader.readBit();
    pps.redundantPicCntPresentFlag = reader.readBit();
    pps.valid = !reader.hasError();
    return pps;
}

SliceInfo H264Parser::parseSliceHeader(const QByteArray &rbsp, int nalUnitType, int nalRefIdc) const
{
    BitReader reader(rbsp);
    SliceInfo slice;
    slice.nalUnitType = nalUnitType;
    slice.nalRefIdc = nalRefIdc;
    slice.firstMbInSlice = static_cast<int>(reader.readUE());
    slice.sliceType = static_cast<int>(reader.readUE());
    slice.sliceTypeName = sliceTypeName(slice.sliceType);
    slice.picParameterSetId = static_cast<int>(reader.readUE());

    const PpsInfo pps = m_ppsById.value(slice.picParameterSetId);
    const SpsInfo sps = m_spsById.value(pps.seqParameterSetId);
    if (!pps.valid || !sps.valid) {
        slice.valid = !reader.hasError();
        return slice;
    }

    slice.frameNum = static_cast<int>(reader.readBits(sps.log2MaxFrameNumMinus4 + 4));
    slice.picWidthInMbs = sps.picWidthInMbsMinus1 + 1;
    slice.picHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) * (sps.picHeightInMapUnitsMinus1 + 1);
    if (!sps.frameMbsOnlyFlag) {
        const bool fieldPicFlag = reader.readBit();
        if (fieldPicFlag) {
            reader.readBit();
        }
    }

    if (nalUnitType == 5) {
        slice.idrPicId = static_cast<int>(reader.readUE());
    }

    if (sps.picOrderCntType == 0) {
        slice.picOrderCntLsb = static_cast<int>(reader.readBits(sps.log2MaxPicOrderCntLsbMinus4 + 4));
        if (pps.bottomFieldPicOrderInFramePresentFlag && sps.frameMbsOnlyFlag) {
            reader.readSE();
        }
    }

    const int normalizedSliceType = slice.sliceType % 5;
    if (pps.redundantPicCntPresentFlag) {
        reader.readUE();
    }

    if (normalizedSliceType == 1) {
        reader.readBit();
    }

    int numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
    int numRefIdxL1ActiveMinus1 = pps.numRefIdxL1DefaultActiveMinus1;
    if (normalizedSliceType == 0 || normalizedSliceType == 1 || normalizedSliceType == 3) {
        if (reader.readBit()) {
            numRefIdxL0ActiveMinus1 = static_cast<int>(reader.readUE());
            if (normalizedSliceType == 1) {
                numRefIdxL1ActiveMinus1 = static_cast<int>(reader.readUE());
            }
        }
    }

    auto skipRefPicListModification = [&reader]() {
        if (!reader.readBit()) {
            return;
        }
        while (reader.bitsRemaining() > 0) {
            const quint32 modificationOfPicNumsIdc = reader.readUE();
            if (modificationOfPicNumsIdc == 3) {
                break;
            }
            reader.readUE();
        }
    };

    if (normalizedSliceType != 2 && normalizedSliceType != 4) {
        skipRefPicListModification();
    }
    if (normalizedSliceType == 1) {
        skipRefPicListModification();
    }

    auto skipPredWeightTable = [&reader, &sps](int l0Count, int l1Count, bool hasList1) {
        const int chromaArrayType = sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc;
        reader.readUE();
        if (chromaArrayType != 0) {
            reader.readUE();
        }

        auto skipList = [&reader, chromaArrayType](int count) {
            for (int i = 0; i <= count; ++i) {
                if (reader.readBit()) {
                    reader.readSE();
                    reader.readSE();
                }
                if (chromaArrayType != 0 && reader.readBit()) {
                    for (int j = 0; j < 2; ++j) {
                        reader.readSE();
                        reader.readSE();
                    }
                }
            }
        };

        skipList(l0Count);
        if (hasList1) {
            skipList(l1Count);
        }
    };

    const bool usePredWeightTable =
        (pps.weightedPredFlag && (normalizedSliceType == 0 || normalizedSliceType == 3))
        || (pps.weightedBipredIdc == 1 && normalizedSliceType == 1);
    if (usePredWeightTable) {
        skipPredWeightTable(numRefIdxL0ActiveMinus1, numRefIdxL1ActiveMinus1, normalizedSliceType == 1);
    }

    if (nalRefIdc != 0) {
        if (nalUnitType == 5) {
            reader.readBit();
            reader.readBit();
        } else {
            if (reader.readBit()) {
                while (reader.bitsRemaining() > 0) {
                    const quint32 op = reader.readUE();
                    if (op == 0) {
                        break;
                    }
                    if (op == 1 || op == 3) {
                        reader.readUE();
                    }
                    if (op == 2) {
                        reader.readUE();
                    }
                    if (op == 3 || op == 6) {
                        reader.readUE();
                    }
                    if (op == 4) {
                        reader.readUE();
                    }
                }
            }
        }
    }

    if (pps.entropyCodingModeFlag && normalizedSliceType != 2 && normalizedSliceType != 4) {
        reader.readUE();
    }
    slice.sliceQpDelta = reader.readSE();
    slice.derivedQp = std::clamp(26 + pps.picInitQpMinus26 + slice.sliceQpDelta, 0, 51);
    slice.valid = !reader.hasError();

    MacroblockInfo firstMb;
    const int totalMacroblocks = std::max(1, slice.picWidthInMbs * slice.picHeightInMbs);
    const int startMacroblock = std::clamp(slice.firstMbInSlice, 0, totalMacroblocks - 1);
    for (int address = startMacroblock; address < totalMacroblocks; ++address) {
        MacroblockInfo mb;
        mb.address = address;
        mb.mbType = QStringLiteral("Estimated");
        mb.qp = slice.derivedQp;
        mb.note = QStringLiteral("QP is estimated from PPS pic_init_qp_minus26 + slice_qp_delta until slice_data macroblock parsing is implemented.");
        slice.macroblocks.append(mb);
    }

    if (slice.macroblocks.isEmpty()) {
        firstMb.address = slice.firstMbInSlice;
        firstMb.mbType = QStringLiteral("Not parsed yet");
        firstMb.qp = slice.derivedQp;
        firstMb.note = QStringLiteral("QP derived from PPS pic_init_qp_minus26 + slice_qp_delta; mb_type requires slice_data CABAC/CAVLC parsing.");
        slice.macroblocks.append(firstMb);
    }
    return slice;
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
