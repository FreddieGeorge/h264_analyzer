#include "core/DecodeWorker.h"

#include <QThread>

DecodeWorker::DecodeWorker(QObject *parent)
    : QObject(parent)
{
}

void DecodeWorker::decodeFile(const QString &filePath)
{
    m_stopRequested.store(false);

    FFmpegDecoder decoder;
    if (!decoder.openFile(filePath)) {
        emit errorOccurred(decoder.lastError());
        emit finished();
        return;
    }

    const StreamInfo streamInfo = decoder.getStreamInfo();
    emit streamOpened(streamInfo);
    emit logMessage(tr("[Info] Video stream: %1x%2, %3 fps, codec=%4, pix_fmt=%5")
                        .arg(streamInfo.width)
                        .arg(streamInfo.height)
                        .arg(streamInfo.frameRate, 0, 'f', 3)
                        .arg(streamInfo.codecName, streamInfo.pixelFormatName));

    const unsigned long frameDelayMs = streamInfo.frameRate > 0.0
        ? static_cast<unsigned long>(1000.0 / streamInfo.frameRate)
        : 0UL;

    while (!m_stopRequested.load()) {
        AVFrame *frame = decoder.decodeNextFrame();
        if (frame == nullptr) {
            if (!decoder.lastError().isEmpty()) {
                emit errorOccurred(decoder.lastError());
            }
            break;
        }

        DecodedVideoFramePtr copy = FFmpegDecoder::copyFrame(frame);
        if (copy) {
            emit frameDecoded(copy);
        }
        const FrameSyntaxInfo syntaxInfo = decoder.lastFrameSyntaxInfo();
        if (!syntaxInfo.slices.isEmpty()) {
            emit frameSyntaxDecoded(syntaxInfo);
        }

        if (frameDelayMs > 0) {
            QThread::msleep(frameDelayMs);
        }
    }

    emit finished();
}

void DecodeWorker::stop()
{
    m_stopRequested.store(true);
}
