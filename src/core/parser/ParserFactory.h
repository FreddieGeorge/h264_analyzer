#pragma once

#include "core/model/CodecKind.h"
#include "core/parser/BitstreamParser.h"

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

CodecKind codecKindFromAvCodecId(AVCodecID codecId);
std::unique_ptr<IBitstreamParser> makeBitstreamParserForAvCodecId(AVCodecID codecId);
std::unique_ptr<IBitstreamParser> makeBitstreamParserForCodecKind(CodecKind codecKind);
