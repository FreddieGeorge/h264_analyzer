#include "core/parser/video/h264/H264CabacMacroblockParser.h"

H264CabacUnsupportedResult h264CabacUnsupportedResult()
{
    return {
        QStringLiteral("cabac_unsupported"),
        QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header.")
    };
}
