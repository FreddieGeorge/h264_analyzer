#pragma once

#include "core/parser/video/h264/H264Parser.h"

#include <QJsonObject>

QJsonObject h264FrameSyntaxToJson(const FrameSyntaxInfo &syntaxInfo);
