#pragma once

#include "core/model/FrameAnalysis.h"
#include "core/model/StreamInfo.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

QJsonObject streamInfoToJson(const StreamInfo &stream);
QJsonObject mediaStreamInfoToJson(const MediaStreamInfo &stream);
QJsonObject frameAnalysisToJson(const FrameAnalysis &analysis);
QJsonObject selectedFrameExportToJson(const StreamInfo &stream,
                                      const FrameAnalysis &analysis,
                                      const QString &generator,
                                      const QString &generatorVersion);
QJsonObject allFramesExportToJson(const StreamInfo &stream,
                                  const QVector<FrameAnalysis> &frames,
                                  const QString &generator,
                                  const QString &generatorVersion);
