#pragma once

#include "core/parser/video/h264/H264CabacContextModel.h"

#include <QString>
#include <QVector>

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
    bool needsSubMacroblockTypes = false;
    int mbType = -1;
    int prefixBin = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypeResult
{
    bool ok = false;
    bool complete = false;
    int subMbType = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypesResult
{
    bool ok = false;
    bool complete = false;
    QVector<int> subMbTypes;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxResult
{
    bool ok = false;
    bool present = false;
    int refIdx = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxListResult
{
    bool ok = false;
    bool present = false;
    QVector<int> refIdx;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdResult
{
    bool ok = false;
    bool complete = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdPair
{
    int x = 0;
    int y = 0;
};

struct H264CabacMvdListResult
{
    bool ok = false;
    bool complete = false;
    QVector<H264CabacMvdPair> mvd;
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

H264CabacSubMbTypeResult h264ReadCabacPSubMbType(BitReader &reader,
                                                 H264CabacDecoder &decoder,
                                                 H264CabacContextModelSet &contexts,
                                                 const H264SliceDataContext &sliceContext);

H264CabacSubMbTypesResult h264ReadCabacPSubMbTypes(BitReader &reader,
                                                   H264CabacDecoder &decoder,
                                                   H264CabacContextModelSet &contexts,
                                                   const H264SliceDataContext &sliceContext,
                                                   int subMacroblockCount);

H264CabacRefIdxResult h264ReadCabacRefIdxL0(BitReader &reader,
                                            H264CabacDecoder &decoder,
                                            H264CabacContextModelSet &contexts,
                                            const H264SliceDataContext &sliceContext);

H264CabacRefIdxListResult h264ReadCabacPSubMbRefIdxL0(BitReader &reader,
                                                      H264CabacDecoder &decoder,
                                                      H264CabacContextModelSet &contexts,
                                                      const H264SliceDataContext &sliceContext,
                                                      int subMacroblockCount);

H264CabacMvdResult h264ReadCabacMvdL0Component(BitReader &reader,
                                               H264CabacDecoder &decoder,
                                               H264CabacContextModelSet &contexts,
                                               int component);

H264CabacMvdListResult h264ReadCabacPSubMbMvdL0(BitReader &reader,
                                                H264CabacDecoder &decoder,
                                                H264CabacContextModelSet &contexts,
                                                const QVector<int> &subMbTypes);
