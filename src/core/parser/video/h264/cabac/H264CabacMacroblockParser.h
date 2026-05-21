#pragma once

#include "core/parser/video/h264/cabac/H264CabacSyntaxTypes.h"

#include <QString>
#include <QVector>

class H264CabacDecoder;
class H264CabacContextModelSet;
struct H264SliceDataContext;

struct H264CabacUnsupportedResult
{
    QString code;
    QString message;
};

struct H264CabacMacroblockSyntaxResult
{
    bool ok = false;
    bool complete = false;
    bool parsedSubMacroblockSyntax = false;
    int mbType = -1;
    int firstSyntaxCtxIdx = -1;
    QString firstSyntaxName;
    QVector<int> subMbTypes;
    bool refIdxL0Present = false;
    QVector<int> refIdxL0;
    QVector<H264CabacMvdPair> mvdL0;
    QString diagnosticCode;
    QString diagnosticMessage;
};

H264CabacUnsupportedResult h264CabacUnsupportedResult();
H264CabacMacroblockSyntaxResult h264ReadCabacMacroblockSyntax(H264SliceDataContext &context,
                                                              H264CabacDecoder &decoder,
                                                              H264CabacContextModelSet &contexts);
bool h264AppendCabacMacroblockSyntaxSkeleton(H264SliceDataContext &context,
                                             const H264CabacMacroblockSyntaxResult &syntax);
void h264AppendUnsupportedCabacMacroblocks(H264SliceDataContext &context);
