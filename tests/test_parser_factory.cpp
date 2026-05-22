#include "core/parser/ParserFactory.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void codecKindMappingCoversKnownCodecs()
{
    require(codecKindFromAvCodecId(AV_CODEC_ID_H264) == CodecKind::H264, "H264 codec kind mapping");
    require(codecKindFromAvCodecId(AV_CODEC_ID_HEVC) == CodecKind::HEVC, "HEVC codec kind mapping");
    require(codecKindFromAvCodecId(AV_CODEC_ID_AAC) == CodecKind::AAC, "AAC codec kind mapping");
    require(codecKindFromAvCodecId(AV_CODEC_ID_MP3) == CodecKind::MP3, "MP3 codec kind mapping");
    require(codecKindFromAvCodecId(AV_CODEC_ID_AV1) == CodecKind::AV1, "AV1 codec kind mapping");
    require(codecKindFromAvCodecId(AV_CODEC_ID_NONE) == CodecKind::Unknown, "unknown codec kind mapping");
}

void parserFactoryCreatesSupportedParsers()
{
    auto h264 = makeBitstreamParserForAvCodecId(AV_CODEC_ID_H264);
    require(h264 != nullptr, "H264 parser is created");
    require(h264->codecKind() == CodecKind::H264, "H264 parser kind");

    auto hevc = makeBitstreamParserForCodecKind(CodecKind::HEVC);
    require(hevc != nullptr, "HEVC parser is created");
    require(hevc->codecKind() == CodecKind::HEVC, "HEVC parser kind");

    auto aac = makeBitstreamParserForCodecKind(CodecKind::AAC);
    require(aac != nullptr, "AAC parser is created");
    require(aac->codecKind() == CodecKind::AAC, "AAC parser kind");

    auto mp3 = makeBitstreamParserForAvCodecId(AV_CODEC_ID_MP3);
    require(mp3 != nullptr, "MP3 parser is created");
    require(mp3->codecKind() == CodecKind::MP3, "MP3 parser kind");
}

void parserFactoryRejectsUnsupportedParsers()
{
    require(makeBitstreamParserForCodecKind(CodecKind::AV1) == nullptr, "AV1 parser is not implemented");
    require(makeBitstreamParserForCodecKind(CodecKind::VP9) == nullptr, "VP9 parser is not implemented");
    require(makeBitstreamParserForCodecKind(CodecKind::VVC) == nullptr, "VVC parser is not implemented");
    require(makeBitstreamParserForCodecKind(CodecKind::Unknown) == nullptr, "unknown parser is not created");
}
}

int main()
{
    codecKindMappingCoversKnownCodecs();
    parserFactoryCreatesSupportedParsers();
    parserFactoryRejectsUnsupportedParsers();

    std::cout << "Parser factory tests passed\n";
    return 0;
}
