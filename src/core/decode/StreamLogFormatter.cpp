#include "core/decode/StreamLogFormatter.h"

#include <QCoreApplication>

namespace
{
QString trLog(const char *text)
{
    return QCoreApplication::translate("DecodeWorker", text);
}

QString unknownText()
{
    return trLog("unknown");
}

QString streamDetailsText(const MediaStreamInfo &stream)
{
    if (stream.mediaKind == MediaKind::Video) {
        return trLog("%1x%2, %3 fps, pix_fmt=%4")
            .arg(stream.width)
            .arg(stream.height)
            .arg(stream.frameRate, 0, 'f', 3)
            .arg(stream.pixelFormatName.isEmpty() ? unknownText() : stream.pixelFormatName);
    }

    if (stream.mediaKind == MediaKind::Audio) {
        return trLog("%1 Hz, %2 ch, sample_fmt=%3, layout=%4")
            .arg(stream.sampleRate)
            .arg(stream.channels)
            .arg(stream.sampleFormatName.isEmpty() ? unknownText() : stream.sampleFormatName)
            .arg(stream.channelLayoutName.isEmpty() ? unknownText() : stream.channelLayoutName);
    }

    return {};
}
}

QVector<QString> streamOpenedLogMessages(const StreamInfo &streamInfo)
{
    QVector<QString> messages;
    messages.append(trLog("[Info] Video stream: %1x%2, %3 fps, codec=%4, pix_fmt=%5")
                        .arg(streamInfo.width)
                        .arg(streamInfo.height)
                        .arg(streamInfo.frameRate, 0, 'f', 3)
                        .arg(streamInfo.codecName, streamInfo.pixelFormatName));
    messages.append(trLog("[Info] Container streams discovered: %1").arg(streamInfo.streams.size()));

    for (const MediaStreamInfo &stream : streamInfo.streams) {
        const QString details = streamDetailsText(stream);
        messages.append(trLog("[Info]   Stream #%1%2: %3, codec=%4%5")
                            .arg(stream.streamIndex)
                            .arg(stream.selected ? trLog(" selected") : QString())
                            .arg(mediaKindName(stream.mediaKind))
                            .arg(stream.codecName.isEmpty() ? codecKindName(stream.codecKind) : stream.codecName)
                            .arg(details.isEmpty() ? QString() : trLog(", %1").arg(details)));
    }

    return messages;
}

QString checkpointSeekLogMessage(int checkpointFrameIndex, int targetFrameIndex)
{
    return trLog("[Info] Seeking from checkpoint frame %1 toward frame %2.")
        .arg(checkpointFrameIndex)
        .arg(targetFrameIndex);
}

QString checkpointSeekFailedLogMessage(const QString &seekError)
{
    return trLog("[Warning] Checkpoint seek failed (%1); decoding from frame 0.")
        .arg(seekError.isEmpty() ? trLog("unknown error") : seekError);
}
