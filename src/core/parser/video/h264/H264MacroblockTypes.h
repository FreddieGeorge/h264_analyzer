#pragma once

#include <QString>

int h264CodedBlockPatternFromCodeNum(quint32 codeNum, bool intra, int chromaArrayType);
QString h264IntraMbTypeName(int mbType);
QString h264PMbTypeName(int mbType);
QString h264BMbTypeName(int mbType);
