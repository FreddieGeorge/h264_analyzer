#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QtGlobal>

#include <array>
#include <memory>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
}

struct DecodedVideoFrame
{
    int width = 0;
    int height = 0;
    AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
    qint64 pts = AV_NOPTS_VALUE;
    std::array<int, 4> lineSize = {0, 0, 0, 0};
    std::array<QByteArray, 4> planes;
};

using DecodedVideoFramePtr = std::shared_ptr<DecodedVideoFrame>;

Q_DECLARE_METATYPE(DecodedVideoFramePtr)
