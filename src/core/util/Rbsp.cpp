#include "core/util/Rbsp.h"

#include <algorithm>

QByteArray rbspFromEbsp(const uint8_t *data, qsizetype size)
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

QVector<qsizetype> rbspByteToPacketByteOffsets(const QByteArray &nalu, qsizetype packetPayloadOffset)
{
    QVector<qsizetype> offsets;
    offsets.reserve(std::max<qsizetype>(0, nalu.size() - 1));
    int zeroCount = 0;

    for (qsizetype i = 1; i < nalu.size(); ++i) {
        const uint8_t byte = static_cast<uint8_t>(nalu.at(i));
        if (zeroCount == 2 && byte == 0x03) {
            zeroCount = 0;
            continue;
        }

        offsets.append(packetPayloadOffset + (i - 1));
        zeroCount = byte == 0 ? zeroCount + 1 : 0;
    }
    return offsets;
}

QVector<AnalysisBitRange> packetBitRangesForRbspField(qsizetype rbspBitOffset,
                                                      qsizetype bitLength,
                                                      const QVector<qsizetype> &rbspByteToPacketByte)
{
    QVector<AnalysisBitRange> ranges;
    if (rbspBitOffset < 0 || bitLength <= 0) {
        return ranges;
    }

    qsizetype currentStart = -1;
    qsizetype currentLength = 0;
    qsizetype previousPacketBit = -1;
    for (qsizetype i = 0; i < bitLength; ++i) {
        const qsizetype rbspBit = rbspBitOffset + i;
        const qsizetype rbspByte = rbspBit / 8;
        if (rbspByte < 0 || rbspByte >= rbspByteToPacketByte.size()) {
            break;
        }

        const qsizetype packetBit = rbspByteToPacketByte.at(rbspByte) * 8 + (rbspBit % 8);
        if (currentStart < 0) {
            currentStart = packetBit;
            currentLength = 1;
        } else if (packetBit == previousPacketBit + 1) {
            ++currentLength;
        } else {
            ranges.append({currentStart, currentLength, QStringLiteral("packet")});
            currentStart = packetBit;
            currentLength = 1;
        }
        previousPacketBit = packetBit;
    }

    if (currentStart >= 0 && currentLength > 0) {
        ranges.append({currentStart, currentLength, QStringLiteral("packet")});
    }
    return ranges;
}
