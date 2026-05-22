#pragma once

#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QString>
#include <QVector>

class FFmpegDecoder;

struct DecodeStartPosition
{
    bool ok = true;
    int frameIndex = 0;
    bool reopenedStream = false;
    StreamInfo reopenedStreamInfo;
    QVector<QString> logMessages;
    QString errorMessage;
};

DecodeStartPosition prepareDecodeStart(FFmpegDecoder &decoder,
                                       const QString &filePath,
                                       int targetFrameIndex,
                                       const FrameSeekCheckpoint &checkpoint);
