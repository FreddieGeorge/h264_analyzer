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
    AVFrame *decodeNextFrame();
    StreamInfo getStreamInfo() const;
    FrameSyntaxInfo lastFrameSyntaxInfo() const;
    QString lastError() const;
    void close();

    static DecodedVideoFramePtr copyFrame(const AVFrame *frame);

private:
    void setError(const QString &message);
    static QString ffmpegErrorString(int errorCode);
    static double rationalToDouble(AVRational value);

    AVFormatContext *m_formatContext = nullptr;
    AVCodecContext *m_codecContext = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;

    int m_videoStreamIndex = -1;
    int m_packetIndex = 0;
    bool m_draining = false;
    StreamInfo m_streamInfo;
    H264Parser m_h264Parser;
    QVector<FrameSyntaxInfo> m_pendingSyntax;
    FrameSyntaxInfo m_lastFrameSyntax;
    QString m_lastError;
};

Q_DECLARE_METATYPE(StreamInfo)
Q_DECLARE_METATYPE(DecodedVideoFramePtr)
