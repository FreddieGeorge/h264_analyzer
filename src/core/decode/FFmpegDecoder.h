#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamInfo.h"

#include <QString>
#include <QVector>

#include <memory>

class AccessUnitAnalyzer;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;

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
    FrameSeekCheckpoint lastFrameSeekCheckpoint() const;
    QString lastError() const;
    void close();

    static DecodedVideoFramePtr copyFrame(const AVFrame *frame);

private:
    void setError(const QString &message);
    static QString ffmpegErrorString(int errorCode);

    struct PendingFrameInfo
    {
        FrameAnalysis analysis;
        FrameSeekCheckpoint checkpoint;
    };

    AVFormatContext *m_formatContext = nullptr;
    AVCodecContext *m_codecContext = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;

    int m_videoStreamIndex = -1;
    int m_containerPacketIndex = 0;
    bool m_draining = false;
    StreamInfo m_streamInfo;
    std::unique_ptr<AccessUnitAnalyzer> m_accessUnitAnalyzer;
    QVector<FrameAnalysis> m_pendingAccessUnitAnalyses;
    QVector<PendingFrameInfo> m_pendingFrames;
    FrameAnalysis m_lastFrameAnalysis;
    FrameSeekCheckpoint m_lastFrameSeekCheckpoint;
    QString m_lastError;
};
