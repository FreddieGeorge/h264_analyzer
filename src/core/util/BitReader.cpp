#include "core/util/BitReader.h"

BitReader::BitReader(const QByteArray &data)
    : m_data(reinterpret_cast<const uint8_t *>(data.constData()))
    , m_sizeBits(data.size() * 8)
{
}

bool BitReader::readBit()
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

quint32 BitReader::readBits(int count)
{
    quint32 value = 0;
    for (int i = 0; i < count; ++i) {
        value = (value << 1) | (readBit() ? 1U : 0U);
    }
    return value;
}

quint32 BitReader::readUE()
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

qint32 BitReader::readSE()
{
    const quint32 codeNum = readUE();
    const qint32 value = static_cast<qint32>((codeNum + 1U) / 2U);
    return (codeNum % 2U) == 0U ? -value : value;
}

bool BitReader::hasError() const
{
    return m_error;
}

qsizetype BitReader::bitsRemaining() const
{
    return m_sizeBits - m_bitOffset;
}

qsizetype BitReader::bitOffset() const
{
    return m_bitOffset;
}

bool BitReader::moreRbspData() const
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

int BitReader::readBitsAt(const QByteArray &data, int bitOffset, int bitCount, bool *ok)
{
    int value = 0;
    for (int i = 0; i < bitCount; ++i) {
        const int bit = bitOffset + i;
        if (bit < 0 || bit / 8 >= data.size()) {
            if (ok != nullptr) {
                *ok = false;
            }
            return value;
        }
        const int bitInByte = 7 - (bit % 8);
        value = (value << 1) | ((static_cast<unsigned char>(data.at(bit / 8)) >> bitInByte) & 0x01);
    }
    return value;
}
