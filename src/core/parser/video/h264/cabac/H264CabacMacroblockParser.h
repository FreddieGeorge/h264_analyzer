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
    bool parsedCodedBlockPattern = false;
    bool parsedCodedBlockPatternZero = false;
    bool parsedResidualCodedBlockFlagsZero = false;
    int mbType = -1;
    int firstSyntaxCtxIdx = -1;
    QString firstSyntaxName;
    QVector<int> subMbTypes;
    bool refIdxL0Present = false;
    QVector<int> refIdxL0;
    QVector<H264CabacMvdPair> mvdL0;
    int codedBlockPattern = -1;
    int codedBlockPatternLuma = -1;
    int codedBlockPatternChroma = -1;
    int mbQpDelta = 0;
    QVector<int> residualLuma4x4BlockIndices;
    QVector<int> residualCodedBlockFlags;
    QVector<int> residualChromaDcComponents;
    QVector<int> residualChromaDcCodedBlockFlags;
    int residualIncompleteBlockIndex = -1;
    int residualIncompleteComponent = -1;
    QString residualIncompleteCategory;
    QString residualIncompleteStage;
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
