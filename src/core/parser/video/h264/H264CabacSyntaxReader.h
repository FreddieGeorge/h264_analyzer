#pragma once

#include "core/parser/video/h264/H264CabacContextModel.h"

#include <QString>

class BitReader;
class H264CabacDecoder;
struct H264SliceDataContext;

struct H264CabacSyntaxResult
{
    bool ok = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMbTypeResult
{
    bool ok = false;
    bool complete = false;
    int mbType = -1;
    int prefixBin = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

H264CabacSyntaxResult h264ReadCabacMbSkipFlag(BitReader &reader,
                                              H264CabacDecoder &decoder,
                                              H264CabacContextModelSet &contexts,
                                              const H264SliceDataContext &sliceContext);

H264CabacSyntaxResult h264ReadCabacMbTypePrefix(BitReader &reader,
                                                H264CabacDecoder &decoder,
                                                H264CabacContextModelSet &contexts,
                                                const H264SliceDataContext &sliceContext);

H264CabacMbTypeResult h264ReadCabacMbType(BitReader &reader,
                                          H264CabacDecoder &decoder,
                                          H264CabacContextModelSet &contexts,
                                          const H264SliceDataContext &sliceContext);
