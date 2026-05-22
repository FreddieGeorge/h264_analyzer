#pragma once

#include "core/model/StreamInfo.h"

#include <QString>

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
