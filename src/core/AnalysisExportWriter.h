#pragma once

#include "core/FrameAnalysis.h"
#include "core/H264Parser.h"
#include "core/StreamDocument.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

QJsonObject streamInfoToJson(const StreamInfo &stream);
QJsonObject h264FrameSyntaxToJson(const FrameSyntaxInfo &syntaxInfo);
QJsonObject frameAnalysisToJson(const FrameAnalysis &analysis);
QJsonObject selectedFrameExportToJson(const StreamInfo &stream,
                                      const FrameAnalysis &analysis,
                                      const QString &generator,
                                      const QString &generatorVersion);
QJsonObject allFramesExportToJson(const StreamInfo &stream,
                                  const QVector<FrameAnalysis> &frames,
                                  const QString &generator,
                                  const QString &generatorVersion);
