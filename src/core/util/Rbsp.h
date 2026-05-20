#pragma once

#include "core/model/FrameAnalysis.h"

#include <QByteArray>
#include <QVector>

#include <cstdint>

QByteArray rbspFromEbsp(const uint8_t *data, qsizetype size);
QVector<qsizetype> rbspByteToPacketByteOffsets(const QByteArray &nalu, qsizetype packetPayloadOffset);
QVector<AnalysisBitRange> packetBitRangesForRbspField(qsizetype rbspBitOffset,
                                                      qsizetype bitLength,
                                                      const QVector<qsizetype> &rbspByteToPacketByte);
