#pragma once

#include "core/parser/video/h264/cavlc/H264CavlcMacroblockParser.h"

bool h264ParseCavlcMacroblockResidual(H264SliceDataContext &context,
                                      MacroblockInfo &mb,
                                      const H264CavlcMacroblockType &type,
                                      bool transform8x8,
                                      H264MacroblockCoeffState &coeffState);
