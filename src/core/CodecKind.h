#pragma once

#include <QString>

enum class CodecKind
{
    Unknown,
    H264,
    HEVC,
    AV1,
    VP9,
    VVC
};

QString codecKindName(CodecKind codecKind);
