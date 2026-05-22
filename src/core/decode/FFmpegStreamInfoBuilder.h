#pragma once

#include "core/model/MediaTypes.h"
#include "core/model/StreamInfo.h"

#include <QString>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

MediaKind mediaKindFromAvMediaType(AVMediaType mediaType);
double rationalToDouble(AVRational value);

void populateContainerStreamInfo(AVFormatContext *formatContext,
                                 const QString &filePath,
                                 StreamInfo *streamInfo);

void populateSelectedVideoStreamInfo(AVFormatContext *formatContext,
                                     AVCodecContext *codecContext,
                                     int videoStreamIndex,
                                     const AVCodec *decoder,
                                     StreamInfo *streamInfo);
