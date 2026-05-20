#pragma once

#include "core/parser/BitstreamParser.h"
#include "core/parser/h264/H264Parser.h"
#include "core/model/StreamDocument.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <array>
#include <memory>
#include <vector>

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
    int containerPacketIndex = -1;
    qint64 packetPosition = -1;
    qint64 packetPts = AV_NOPTS_VALUE;
    qint64 packetDts = AV_NOPTS_VALUE;
    bool keyframe = false;
    bool idr = false;
    CodecKind codecKind = CodecKind::Unknown;
    BitstreamParserStatePtr parserState;
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
    FrameAnalysis lastFrameAnalysis() const;
    QVector<FrameAnalysis> takePendingAccessUnitAnalyses();
    FrameSyntaxInfo lastFrameSyntaxInfo() const;
    FrameSeekCheckpoint lastFrameSeekCheckpoint() const;
    QString lastError() const;
    void close();

    static DecodedVideoFramePtr copyFrame(const AVFrame *frame);

private:
    void setError(const QString &message);
    void createParserForCodec(AVCodecID codecId);
    static QString ffmpegErrorString(int errorCode);
    static double rationalToDouble(AVRational value);

    struct PendingFrameInfo
    {
        FrameAnalysis analysis;
        FrameSeekCheckpoint checkpoint;
    };

    struct StreamPacketParser
    {
        int streamIndex = -1;
        MediaKind mediaKind = MediaKind::Unknown;
        int packetIndex = 0;
        std::unique_ptr<IBitstreamParser> parser;
    };

    AVFormatContext *m_formatContext = nullptr;
    AVCodecContext *m_codecContext = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;

    int m_videoStreamIndex = -1;
    int m_packetIndex = 0;
    int m_containerPacketIndex = 0;
    bool m_draining = false;
    StreamInfo m_streamInfo;
    std::unique_ptr<IBitstreamParser> m_parser;
    std::vector<StreamPacketParser> m_packetParsers;
    QVector<FrameAnalysis> m_pendingAccessUnitAnalyses;
    QVector<PendingFrameInfo> m_pendingFrames;
    FrameAnalysis m_lastFrameAnalysis;
    FrameSeekCheckpoint m_lastFrameSeekCheckpoint;
    QString m_lastError;
};

Q_DECLARE_METATYPE(StreamInfo)
Q_DECLARE_METATYPE(DecodedVideoFramePtr)
Q_DECLARE_METATYPE(FrameSeekCheckpoint)
