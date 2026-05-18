#pragma once

#include <QString>
#include <QtGlobal>

struct StreamInfo
{
    QString absoluteFilePath;
    QString fileName;
    QString directory;
    QString codecName;
    QString pixelFormatName;
    qint64 sizeBytes = 0;
    qint64 bitRate = 0;
    qint64 durationUs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    bool isValid = false;
};

class StreamDocument
{
public:
    bool openFile(const QString &filePath, QString *errorMessage = nullptr);
    void clear();

    const StreamInfo &streamInfo() const;

private:
    StreamInfo m_streamInfo;
};
