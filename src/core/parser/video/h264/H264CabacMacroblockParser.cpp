#include "core/parser/video/h264/H264CabacMacroblockParser.h"

#include "core/parser/video/h264/H264SliceDataContext.h"

H264CabacUnsupportedResult h264CabacUnsupportedResult()
{
    return {
        QStringLiteral("cabac_unsupported"),
        QStringLiteral("CABAC slice_data parsing is not implemented; macroblock QP is carried forward from the slice header.")
    };
}

void h264AppendUnsupportedCabacMacroblocks(H264SliceDataContext &context)
{
    const H264CabacUnsupportedResult cabac = h264CabacUnsupportedResult();
    context.appendEstimatedRemainder(cabac.code, cabac.message);
}
