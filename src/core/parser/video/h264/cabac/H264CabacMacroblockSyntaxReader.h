#pragma once

#include "core/parser/video/h264/cabac/H264CabacContextModel.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxTypes.h"

class BitReader;
class H264CabacDecoder;
struct H264SliceDataContext;

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

H264CabacCodedBlockPatternResult h264ReadCabacCodedBlockPatternZero(BitReader &reader,
                                                                    H264CabacDecoder &decoder,
                                                                    H264CabacContextModelSet &contexts,
                                                                    const H264SliceDataContext &sliceContext);

H264CabacMbQpDeltaResult h264ReadCabacMbQpDeltaZero(BitReader &reader,
                                                    H264CabacDecoder &decoder,
                                                    H264CabacContextModelSet &contexts);
