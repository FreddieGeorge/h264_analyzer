#include "core/model/MediaTypes.h"

QString mediaKindName(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video: return QStringLiteral("video");
    case MediaKind::Audio: return QStringLiteral("audio");
    case MediaKind::Subtitle: return QStringLiteral("subtitle");
    case MediaKind::Data: return QStringLiteral("data");
    case MediaKind::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

QString accessUnitKindName(AccessUnitKind kind)
{
    switch (kind) {
    case AccessUnitKind::VideoFrame: return QStringLiteral("video_frame");
    case AccessUnitKind::AudioFrame: return QStringLiteral("audio_frame");
    case AccessUnitKind::Packet: return QStringLiteral("packet");
    case AccessUnitKind::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}
