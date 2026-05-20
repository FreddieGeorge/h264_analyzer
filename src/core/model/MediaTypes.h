#pragma once

#include <QString>

enum class MediaKind
{
    Unknown,
    Video,
    Audio,
    Subtitle,
    Data
};

enum class AccessUnitKind
{
    Unknown,
    VideoFrame,
    AudioFrame,
    Packet
};

QString mediaKindName(MediaKind kind);
QString accessUnitKindName(AccessUnitKind kind);
