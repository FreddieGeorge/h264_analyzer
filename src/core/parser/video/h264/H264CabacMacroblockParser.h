#pragma once

#include <QString>

struct H264CabacUnsupportedResult
{
    QString code;
    QString message;
};

H264CabacUnsupportedResult h264CabacUnsupportedResult();
