#pragma once

#include "core/parser/video/h264/cabac/H264CabacContextModel.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxTypes.h"

class BitReader;
class H264CabacDecoder;
struct H264SliceDataContext;

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
