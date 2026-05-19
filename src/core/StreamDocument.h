#pragma once

#include "core/CodecKind.h"
#include "core/MediaTypes.h"

#include <QString>
#include <QtGlobal>
#include <QVector>

struct MediaStreamInfo
{
    int streamIndex = -1;
    MediaKind mediaKind = MediaKind::Unknown;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName;
    QString pixelFormatName;
    QString sampleFormatName;
    QString channelLayoutName;
    qint64 bitRate = 0;
    qint64 durationUs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int sampleRate = 0;
    int channels = 0;
    bool selected = false;
};

struct StreamInfo
{
    QString absoluteFilePath;
    QString fileName;
    QString directory;
    MediaKind mediaKind = MediaKind::Unknown;
    int streamIndex = -1;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName;
    QString pixelFormatName;
    QString sampleFormatName;
    QString channelLayoutName;
    qint64 sizeBytes = 0;
    qint64 bitRate = 0;
    qint64 durationUs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int sampleRate = 0;
    int channels = 0;
    QVector<MediaStreamInfo> streams;
    bool isValid = false;
};

class StreamDocument
{
public:
    bool openFile(const QString &filePath, QString *errorMessage = nullptr);
    void clear();
    void updateStreamInfo(const StreamInfo &streamInfo);

    const StreamInfo &streamInfo() const;

private:
    StreamInfo m_streamInfo;
};
