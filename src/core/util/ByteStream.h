#pragma once

#include <QByteArray>

#include <cstdint>

bool hasAnnexBStartCode(const QByteArray &data);
qsizetype annexBStartCodeSizeAt(const QByteArray &data, qsizetype offset);
int readBigEndianLength(const uint8_t *data, int size);
