#pragma once

#include "core/model/StreamInfo.h"

#include <QString>
#include <QVector>

QVector<QString> streamOpenedLogMessages(const StreamInfo &streamInfo);
QString checkpointSeekLogMessage(int checkpointFrameIndex, int targetFrameIndex);
QString checkpointSeekFailedLogMessage(const QString &seekError);
