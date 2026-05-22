#include "core/parser/ParserFactory.h"

#include "core/parser/audio/AacAdtsParser.h"
#include "core/parser/audio/Mp3FrameParser.h"
#include "core/parser/video/h264/H264Parser.h"
#include "core/parser/video/hevc/HevcParser.h"

CodecKind codecKindFromAvCodecId(AVCodecID codecId)
{
    switch (codecId) {
    case AV_CODEC_ID_H264:
        return CodecKind::H264;
    case AV_CODEC_ID_HEVC:
        return CodecKind::HEVC;
    case AV_CODEC_ID_AV1:
        return CodecKind::AV1;
    case AV_CODEC_ID_VP9:
        return CodecKind::VP9;
    case AV_CODEC_ID_VVC:
        return CodecKind::VVC;
    case AV_CODEC_ID_AAC:
        return CodecKind::AAC;
    case AV_CODEC_ID_MP3:
        return CodecKind::MP3;
    default:
        return CodecKind::Unknown;
    }
}

std::unique_ptr<IBitstreamParser> makeBitstreamParserForAvCodecId(AVCodecID codecId)
{
    return makeBitstreamParserForCodecKind(codecKindFromAvCodecId(codecId));
}

std::unique_ptr<IBitstreamParser> makeBitstreamParserForCodecKind(CodecKind codecKind)
{
    switch (codecKind) {
    case CodecKind::H264:
        return std::make_unique<H264Parser>();
    case CodecKind::HEVC:
        return std::make_unique<HevcParser>();
    case CodecKind::AAC:
        return std::make_unique<AacAdtsParser>();
    case CodecKind::MP3:
        return std::make_unique<Mp3FrameParser>();
    case CodecKind::AV1:
    case CodecKind::VP9:
    case CodecKind::VVC:
    case CodecKind::Unknown:
    default:
        return nullptr;
    }
}
