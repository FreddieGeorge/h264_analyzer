#pragma once

#include <QString>

enum class CodecKind
{
    Unknown,
    H264,
    HEVC,
    AV1,
    VP9,
    VVC,
    MP3,
    AAC
};

QString codecKindName(CodecKind codecKind);
