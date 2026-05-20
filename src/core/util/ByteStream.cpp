#include "core/util/ByteStream.h"

#include <algorithm>

bool hasAnnexBStartCode(const QByteArray &data)
{
    const qsizetype scanLimit = std::min<qsizetype>(data.size(), 64);
    for (qsizetype i = 0; i < scanLimit; ++i) {
        if (annexBStartCodeSizeAt(data, i) != 0) {
            return true;
        }
    }
    return false;
}

qsizetype annexBStartCodeSizeAt(const QByteArray &data, qsizetype offset)
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

int readBigEndianLength(const uint8_t *data, int size)
{
    int value = 0;
    for (int i = 0; i < size; ++i) {
        value = (value << 8) | data[i];
    }
    return value;
}
