#pragma once

#include "core/H264Parser.h"
#include "core/StreamDocument.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <array>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

struct FrameSeekCheckpoint
{
    int frameIndex = -1;
    int packetIndex = -1;
    qint64 packetPosition = -1;
    qint64 packetPts = AV_NOPTS_VALUE;
    qint64 packetDts = AV_NOPTS_VALUE;
    bool keyframe = false;
    bool idr = false;
    QHash<int, SpsInfo> spsById;
    QHash<int, PpsInfo> ppsById;
};

class FFmpegDecoder
{
public:
    FFmpegDecoder();
    ~FFmpegDecoder();

    FFmpegDecoder(const FFmpegDecoder &) = delete;
    FFmpegDecoder &operator=(const FFmpegDecoder &) = delete;

    FFmpegDecoder(FFmpegDecoder &&) = delete;
    FFmpegDecoder &operator=(FFmpegDecoder &&) = delete;

    bool openFile(const QString &filePath);
    bool seekToCheckpoint(const FrameSeekCheckpoint &checkpoint);
    AVFrame *decodeNextFrame();
    StreamInfo getStreamInfo() const;
    FrameSyntaxInfo lastFrameSyntaxInfo() const;
    FrameSeekCheckpoint lastFrameSeekCheckpoint() const;
    QString lastError() const;
    void close();

    static DecodedVideoFramePtr copyFrame(const AVFrame *frame);

private:
    void setError(const QString &message);
    static QString ffmpegErrorString(int errorCode);
    static double rationalToDouble(AVRational value);

    struct PendingFrameInfo
    {
        FrameSyntaxInfo syntax;
        FrameSeekCheckpoint checkpoint;
    };

    AVFormatContext *m_formatContext = nullptr;
    AVCodecContext *m_codecContext = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;

    int m_videoStreamIndex = -1;
    int m_packetIndex = 0;
    bool m_draining = false;
    StreamInfo m_streamInfo;
    H264Parser m_h264Parser;
    QVector<PendingFrameInfo> m_pendingFrames;
    FrameSyntaxInfo m_lastFrameSyntax;
    FrameSeekCheckpoint m_lastFrameSeekCheckpoint;
    QString m_lastError;
};

Q_DECLARE_METATYPE(StreamInfo)
Q_DECLARE_METATYPE(DecodedVideoFramePtr)
Q_DECLARE_METATYPE(FrameSeekCheckpoint)
