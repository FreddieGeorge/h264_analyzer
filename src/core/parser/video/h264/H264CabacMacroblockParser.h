#pragma once

#include <QString>

struct H264SliceDataContext;

struct H264CabacUnsupportedResult
{
    QString code;
    QString message;
};

H264CabacUnsupportedResult h264CabacUnsupportedResult();
void h264AppendUnsupportedCabacMacroblocks(H264SliceDataContext &context);
