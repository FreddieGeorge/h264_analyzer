#include "core/model/StreamDocument.h"

#include <QDir>
#include <QFileInfo>

bool StreamDocument::openFile(const QString &filePath, QString *errorMessage)
{
    clear();

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("File not found: %1")
                                .arg(QDir::toNativeSeparators(filePath));
        }
        return false;
    }

    m_streamInfo.absoluteFilePath = info.absoluteFilePath();
    m_streamInfo.fileName = info.fileName();
    m_streamInfo.directory = info.absolutePath();
    m_streamInfo.sizeBytes = info.size();
    m_streamInfo.isValid = true;
    return true;
}

void StreamDocument::clear()
{
    m_streamInfo = StreamInfo {};
}

void StreamDocument::updateStreamInfo(const StreamInfo &streamInfo)
{
    m_streamInfo = streamInfo;
}

const StreamInfo &StreamDocument::streamInfo() const
{
    return m_streamInfo;
}
