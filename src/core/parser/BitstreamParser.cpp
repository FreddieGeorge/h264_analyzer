#include "core/model/CodecKind.h"

QString codecKindName(CodecKind codecKind)
{
    switch (codecKind) {
    case CodecKind::H264: return QStringLiteral("H.264/AVC");
    case CodecKind::HEVC: return QStringLiteral("HEVC/H.265");
    case CodecKind::AV1: return QStringLiteral("AV1");
    case CodecKind::VP9: return QStringLiteral("VP9");
    case CodecKind::VVC: return QStringLiteral("VVC/H.266");
    case CodecKind::MP3: return QStringLiteral("MP3");
    case CodecKind::AAC: return QStringLiteral("AAC");
    case CodecKind::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}
