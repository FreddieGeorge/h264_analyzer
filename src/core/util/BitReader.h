#pragma once

#include <QByteArray>

#include <cstdint>

class BitReader
{
public:
    explicit BitReader(const QByteArray &data);

    bool readBit();
    quint32 readBits(int count);
    quint32 readUE();
    qint32 readSE();

    bool hasError() const;
    qsizetype bitsRemaining() const;
    qsizetype bitOffset() const;
    bool moreRbspData() const;

    static int readBitsAt(const QByteArray &data, int bitOffset, int bitCount, bool *ok);

private:
    const uint8_t *m_data = nullptr;
    qsizetype m_sizeBits = 0;
    qsizetype m_bitOffset = 0;
    bool m_error = false;
};
