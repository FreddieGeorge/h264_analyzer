#include "core/FFmpegDecoder.h"

#include "core/H264FrameAnalysisAdapter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <utility>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

FFmpegDecoder::FFmpegDecoder()
{
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    if (m_packet == nullptr || m_frame == nullptr) {
        setError(QStringLiteral("Unable to allocate FFmpeg packet/frame."));
    }
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
}

void FFmpegDecoder::createParserForCodec(AVCodecID codecId)
{
    if (codecId == AV_CODEC_ID_H264) {
        m_parser = std::make_unique<H264Parser>();
        return;
    }

    m_parser.reset();
}

bool FFmpegDecoder::openFile(const QString &filePath)
{
    close();

    const QByteArray encodedPath = QFile::encodeName(filePath);
    int ret = avformat_open_input(&m_formatContext, encodedPath.constData(), nullptr, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avformat_open_input failed: %1").arg(ffmpegErrorString(ret)));
        return false;
    }

    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avformat_find_stream_info failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    const AVCodec *decoder = nullptr;
    ret = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0) {
        setError(QStringLiteral("No video stream found: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    m_videoStreamIndex = ret;
    m_packetIndex = 0;
    AVStream *videoStream = m_formatContext->streams[m_videoStreamIndex];

    m_codecContext = avcodec_alloc_context3(decoder);
    if (m_codecContext == nullptr) {
        setError(QStringLiteral("Unable to allocate AVCodecContext."));
        close();
        return false;
    }

    ret = avcodec_parameters_to_context(m_codecContext, videoStream->codecpar);
    if (ret < 0) {
        setError(QStringLiteral("avcodec_parameters_to_context failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    ret = avcodec_open2(m_codecContext, decoder, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avcodec_open2 failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    const QFileInfo info(filePath);
    const AVRational rate = videoStream->avg_frame_rate.num != 0
        ? videoStream->avg_frame_rate
        : videoStream->r_frame_rate;

    m_streamInfo.absoluteFilePath = info.absoluteFilePath();
    m_streamInfo.fileName = info.fileName();
    m_streamInfo.directory = info.absolutePath();
    m_streamInfo.sizeBytes = info.size();
    m_streamInfo.codecName = QString::fromUtf8(decoder->name);
    const char *pixelFormatName = av_get_pix_fmt_name(m_codecContext->pix_fmt);
    m_streamInfo.pixelFormatName = pixelFormatName != nullptr
        ? QString::fromUtf8(pixelFormatName)
        : QStringLiteral("unknown");
    m_streamInfo.bitRate = m_codecContext->bit_rate > 0
        ? m_codecContext->bit_rate
        : m_formatContext->bit_rate;
    m_streamInfo.durationUs = m_formatContext->duration;
    m_streamInfo.width = m_codecContext->width;
    m_streamInfo.height = m_codecContext->height;
    m_streamInfo.frameRate = rationalToDouble(rate);
    m_streamInfo.isValid = true;

    createParserForCodec(m_codecContext->codec_id);
    if (m_parser != nullptr) {
        m_parser->reset();
    }
    if (m_parser != nullptr && m_codecContext->extradata != nullptr) {
        const QByteArray extraData(reinterpret_cast<const char *>(m_codecContext->extradata),
                                   m_codecContext->extradata_size);
        m_parser->parseDecoderConfigurationRecord(extraData);
    }
    m_pendingFrames.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    m_draining = false;
    m_lastError.clear();

    return true;
}

bool FFmpegDecoder::seekToCheckpoint(const FrameSeekCheckpoint &checkpoint)
{
    if (m_formatContext == nullptr || m_codecContext == nullptr) {
        setError(QStringLiteral("Decoder is not open."));
        return false;
    }

    int ret = AVERROR(EINVAL);
    if (checkpoint.packetPts != AV_NOPTS_VALUE) {
        ret = av_seek_frame(m_formatContext,
                            m_videoStreamIndex,
                            checkpoint.packetPts,
                            AVSEEK_FLAG_BACKWARD);
    } else if (checkpoint.packetDts != AV_NOPTS_VALUE) {
        ret = av_seek_frame(m_formatContext,
                            m_videoStreamIndex,
                            checkpoint.packetDts,
                            AVSEEK_FLAG_BACKWARD);
    } else if (checkpoint.packetPosition >= 0) {
        ret = av_seek_frame(m_formatContext,
                            -1,
                            checkpoint.packetPosition,
                            AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);
    }

    if (ret < 0) {
        setError(QStringLiteral("av_seek_frame failed for checkpoint at frame %1: %2")
                     .arg(checkpoint.frameIndex)
                     .arg(ffmpegErrorString(ret)));
        return false;
    }

    avcodec_flush_buffers(m_codecContext);
    av_packet_unref(m_packet);
    av_frame_unref(m_frame);
    m_pendingFrames.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    if (m_parser != nullptr && checkpoint.codecKind == m_parser->codecKind()) {
        m_parser->restoreState(checkpoint.parserState);
    }
    m_packetIndex = std::max(0, checkpoint.packetIndex);
    m_draining = false;
    m_lastError.clear();
    return true;
}

AVFrame *FFmpegDecoder::decodeNextFrame()
{
    if (m_formatContext == nullptr || m_codecContext == nullptr) {
        setError(QStringLiteral("Decoder is not open."));
        return nullptr;
    }

    while (true) {
        int ret = avcodec_receive_frame(m_codecContext, m_frame);
        if (ret == 0) {
            if (!m_pendingFrames.isEmpty()) {
                const PendingFrameInfo info = m_pendingFrames.takeFirst();
                m_lastFrameAnalysis = info.analysis;
                m_lastFrameSeekCheckpoint = info.checkpoint;
            } else {
                m_lastFrameAnalysis = FrameAnalysis {};
                m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
            }
            m_lastFrameAnalysis.pts = m_frame->best_effort_timestamp;
            return m_frame;
        }

        if (ret == AVERROR_EOF) {
            return nullptr;
        }

        if (ret != AVERROR(EAGAIN)) {
            setError(QStringLiteral("avcodec_receive_frame failed: %1").arg(ffmpegErrorString(ret)));
            return nullptr;
        }

        if (m_draining) {
            return nullptr;
        }

        while (true) {
            ret = av_read_frame(m_formatContext, m_packet);
            if (ret < 0) {
                avcodec_send_packet(m_codecContext, nullptr);
                m_draining = true;
                break;
            }

            if (m_packet->stream_index != m_videoStreamIndex) {
                av_packet_unref(m_packet);
                continue;
            }

            if (m_parser != nullptr && m_packet->data != nullptr && m_packet->size > 0) {
                const QByteArray packetData(reinterpret_cast<const char *>(m_packet->data), m_packet->size);
                const int packetIndex = m_packetIndex++;
                FrameAnalysis analysis = m_parser->parsePacket(packetData,
                                                               m_packet->pts,
                                                               m_packet->dts,
                                                               packetIndex);
                if (analysis.hasFrame) {
                    FrameSeekCheckpoint checkpoint;
                    checkpoint.packetIndex = packetIndex;
                    checkpoint.packetPosition = m_packet->pos;
                    checkpoint.packetPts = m_packet->pts;
                    checkpoint.packetDts = m_packet->dts;
                    checkpoint.keyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;
                    checkpoint.codecKind = m_parser->codecKind();
                    checkpoint.parserState = m_parser->snapshotState();
                    for (const AnalysisUnit &unit : analysis.units) {
                        if (analysis.codecKind == CodecKind::H264
                            && unit.kind == AnalysisUnitKind::Nalu
                            && unit.type == 5) {
                            checkpoint.idr = true;
                            break;
                        }
                    }
                    m_pendingFrames.append({analysis, checkpoint});
                }
            }

            ret = avcodec_send_packet(m_codecContext, m_packet);
            av_packet_unref(m_packet);
            if (ret < 0) {
                setError(QStringLiteral("avcodec_send_packet failed: %1").arg(ffmpegErrorString(ret)));
                return nullptr;
            }
            break;
        }
    }
}

StreamInfo FFmpegDecoder::getStreamInfo() const
{
    return m_streamInfo;
}

FrameAnalysis FFmpegDecoder::lastFrameAnalysis() const
{
    return m_lastFrameAnalysis;
}

FrameSyntaxInfo FFmpegDecoder::lastFrameSyntaxInfo() const
{
    return h264SyntaxFromFrameAnalysis(m_lastFrameAnalysis);
}

FrameSeekCheckpoint FFmpegDecoder::lastFrameSeekCheckpoint() const
{
    return m_lastFrameSeekCheckpoint;
}

QString FFmpegDecoder::lastError() const
{
    return m_lastError;
}

void FFmpegDecoder::close()
{
    m_draining = false;
    m_videoStreamIndex = -1;
    m_packetIndex = 0;
    m_streamInfo = StreamInfo {};
    m_pendingFrames.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    if (m_parser != nullptr) {
        m_parser->reset();
    }
    m_parser.reset();

    if (m_codecContext != nullptr) {
        avcodec_free_context(&m_codecContext);
    }

    if (m_formatContext != nullptr) {
        avformat_close_input(&m_formatContext);
    }

    if (m_packet != nullptr) {
        av_packet_unref(m_packet);
    }

    if (m_frame != nullptr) {
        av_frame_unref(m_frame);
    }
}

DecodedVideoFramePtr FFmpegDecoder::copyFrame(const AVFrame *frame)
{
    if (frame == nullptr) {
        return {};
    }

    auto result = std::make_shared<DecodedVideoFrame>();
    result->width = frame->width;
    result->height = frame->height;
    result->pixelFormat = static_cast<AVPixelFormat>(frame->format);
    result->pts = frame->best_effort_timestamp;

    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));

    for (int plane = 0; plane < 4; ++plane) {
        result->lineSize[plane] = frame->linesize[plane];
        if (frame->data[plane] == nullptr || frame->linesize[plane] <= 0) {
            continue;
        }

        int planeHeight = frame->height;
        if (descriptor != nullptr && plane > 0) {
            planeHeight = AV_CEIL_RSHIFT(frame->height, descriptor->log2_chroma_h);
        }

        const int bytes = frame->linesize[plane] * planeHeight;
        result->planes[plane] = QByteArray(reinterpret_cast<const char *>(frame->data[plane]), bytes);
    }

    return result;
}

void FFmpegDecoder::setError(const QString &message)
{
    m_lastError = message;
}

QString FFmpegDecoder::ffmpegErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

double FFmpegDecoder::rationalToDouble(AVRational value)
{
    if (value.num == 0 || value.den == 0) {
        return 0.0;
    }
    return static_cast<double>(value.num) / static_cast<double>(value.den);
}
