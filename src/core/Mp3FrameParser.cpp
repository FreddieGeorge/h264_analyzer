#include "core/Mp3FrameParser.h"

#include <QString>

namespace
{
int samplesPerFrame(int versionId, int layer)
{
    if (layer == 3) {
        return 384;
    }
    if (layer == 2) {
        return 1152;
    }
    if (layer == 1) {
        return versionId == 3 ? 1152 : 576;
    }
    return 0;
}

int estimatedFrameLength(int versionId, int layer, int bitrateKbps, int sampleRate, int padding)
{
    if (bitrateKbps <= 0 || sampleRate <= 0) {
        return 0;
    }

    const int bitrate = bitrateKbps * 1000;
    if (layer == 3) {
        return ((12 * bitrate) / sampleRate + padding) * 4;
    }
    if (layer == 1 && versionId != 3) {
        return (72 * bitrate) / sampleRate + padding;
    }
    return (144 * bitrate) / sampleRate + padding;
}
}

Mp3FrameParser::Mp3FrameParser() = default;

CodecKind Mp3FrameParser::codecKind() const
{
    return CodecKind::MP3;
}

void Mp3FrameParser::reset()
{
}

void Mp3FrameParser::parseDecoderConfigurationRecord(const QByteArray &extraData)
{
    Q_UNUSED(extraData);
}

FrameAnalysis Mp3FrameParser::parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    FrameAnalysis analysis;
    analysis.frameIndex = packetIndex;
    analysis.mediaKind = MediaKind::Audio;
    analysis.accessUnitKind = AccessUnitKind::AudioFrame;
    analysis.codecKind = CodecKind::MP3;
    analysis.codecName = QStringLiteral("mp3");
    analysis.pts = pts;
    analysis.dts = dts;

    if (packetData.size() < 4) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_frame_header_truncated"),
            QStringLiteral("MP3 frame header is shorter than 4 bytes."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    bool ok = true;
    const int syncword = readBits(packetData, 0, 11, &ok);
    appendField(analysis, QStringLiteral("syncword"), 0, 11, QStringLiteral("0x%1").arg(syncword, 3, 16, QLatin1Char('0')));
    if (!ok || syncword != 0x07ff) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_frame_sync_missing"),
            QStringLiteral("MP3 frame syncword was not found."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    const int versionId = readBits(packetData, 11, 2, &ok);
    const int layer = readBits(packetData, 13, 2, &ok);
    const int protectionAbsent = readBits(packetData, 15, 1, &ok);
    const int bitrateIndex = readBits(packetData, 16, 4, &ok);
    const int samplingFrequencyIndex = readBits(packetData, 20, 2, &ok);
    const int padding = readBits(packetData, 22, 1, &ok);
    const int privateBit = readBits(packetData, 23, 1, &ok);
    const int channelMode = readBits(packetData, 24, 2, &ok);
    const int modeExtension = readBits(packetData, 26, 2, &ok);
    const int copyright = readBits(packetData, 28, 1, &ok);
    const int original = readBits(packetData, 29, 1, &ok);
    const int emphasis = readBits(packetData, 30, 2, &ok);
    const int bitrateKbps = bitrateKbpsForIndex(versionId, layer, bitrateIndex);
    const int sampleRate = sampleRateForIndex(versionId, samplingFrequencyIndex);
    const int frameLength = estimatedFrameLength(versionId, layer, bitrateKbps, sampleRate, padding);

    appendField(analysis, QStringLiteral("version_id"), 11, 2, QString::number(versionId));
    appendField(analysis, QStringLiteral("version"), 11, 2, versionName(versionId));
    appendField(analysis, QStringLiteral("layer"), 13, 2, layerName(layer));
    appendField(analysis, QStringLiteral("protection_absent"), 15, 1, QString::number(protectionAbsent));
    appendField(analysis, QStringLiteral("bitrate_index"), 16, 4, QString::number(bitrateIndex));
    appendField(analysis, QStringLiteral("bitrate_kbps"), 16, 4, QString::number(bitrateKbps));
    appendField(analysis, QStringLiteral("sampling_frequency_index"), 20, 2, QString::number(samplingFrequencyIndex));
    appendField(analysis, QStringLiteral("sample_rate"), 20, 2, QString::number(sampleRate));
    appendField(analysis, QStringLiteral("padding"), 22, 1, QString::number(padding));
    appendField(analysis, QStringLiteral("private_bit"), 23, 1, QString::number(privateBit));
    appendField(analysis, QStringLiteral("channel_mode"), 24, 2, channelModeName(channelMode));
    appendField(analysis, QStringLiteral("mode_extension"), 26, 2, QString::number(modeExtension));
    appendField(analysis, QStringLiteral("copyright"), 28, 1, QString::number(copyright));
    appendField(analysis, QStringLiteral("original"), 29, 1, QString::number(original));
    appendField(analysis, QStringLiteral("emphasis"), 30, 2, QString::number(emphasis));
    appendField(analysis, QStringLiteral("samples_per_frame"), 32, 0, QString::number(samplesPerFrame(versionId, layer)));
    appendField(analysis, QStringLiteral("estimated_frame_length"), 32, 0, QString::number(frameLength));

    if (!ok) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_frame_header_truncated"),
            QStringLiteral("MP3 frame header ended unexpectedly."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    if (versionId == 1) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_reserved_version"),
            QStringLiteral("MP3 frame uses the reserved MPEG audio version id."),
            QStringLiteral("warning")
        });
    }
    if (layer == 0) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_reserved_layer"),
            QStringLiteral("MP3 frame uses the reserved layer value."),
            QStringLiteral("warning")
        });
    }
    if (bitrateIndex == 0 || bitrateIndex == 15 || bitrateKbps <= 0) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_invalid_bitrate_index"),
            QStringLiteral("MP3 bitrate_index is free, bad, or unsupported by this skeleton."),
            QStringLiteral("warning")
        });
    }
    if (samplingFrequencyIndex == 3 || sampleRate <= 0) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_invalid_sample_rate_index"),
            QStringLiteral("MP3 sampling_frequency_index is reserved or unsupported."),
            QStringLiteral("warning")
        });
    }
    if (frameLength > 0 && frameLength > packetData.size()) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("mp3_frame_length_exceeds_packet"),
            QStringLiteral("Estimated MP3 frame length exceeds the packet size."),
            QStringLiteral("info")
        });
    }

    AnalysisUnit unit;
    unit.kind = AnalysisUnitKind::Mp3Frame;
    unit.offset = 0;
    unit.size = frameLength > 0 ? frameLength : packetData.size();
    unit.type = layer;
    unit.typeName = layerName(layer);
    analysis.units.append(unit);
    analysis.hasFrame = layer != 0 && versionId != 1 && sampleRate > 0 && bitrateKbps > 0;
    analysis.frameType = QStringLiteral("MP3 Frame");
    return analysis;
}

BitstreamParserStatePtr Mp3FrameParser::snapshotState() const
{
    return nullptr;
}

void Mp3FrameParser::restoreState(const BitstreamParserStatePtr &state)
{
    Q_UNUSED(state);
}

QString Mp3FrameParser::versionName(int versionId)
{
    switch (versionId) {
    case 0: return QStringLiteral("MPEG 2.5");
    case 2: return QStringLiteral("MPEG 2");
    case 3: return QStringLiteral("MPEG 1");
    default: return QStringLiteral("Reserved");
    }
}

QString Mp3FrameParser::layerName(int layer)
{
    switch (layer) {
    case 1: return QStringLiteral("Layer III");
    case 2: return QStringLiteral("Layer II");
    case 3: return QStringLiteral("Layer I");
    default: return QStringLiteral("Reserved");
    }
}

QString Mp3FrameParser::channelModeName(int channelMode)
{
    switch (channelMode) {
    case 0: return QStringLiteral("Stereo");
    case 1: return QStringLiteral("Joint stereo");
    case 2: return QStringLiteral("Dual channel");
    case 3: return QStringLiteral("Single channel");
    default: return QStringLiteral("Unknown");
    }
}

int Mp3FrameParser::sampleRateForIndex(int versionId, int samplingFrequencyIndex)
{
    static constexpr int mpeg1Rates[] = {44100, 48000, 32000, 0};
    static constexpr int mpeg2Rates[] = {22050, 24000, 16000, 0};
    static constexpr int mpeg25Rates[] = {11025, 12000, 8000, 0};
    if (samplingFrequencyIndex < 0 || samplingFrequencyIndex >= 4) {
        return 0;
    }
    if (versionId == 3) {
        return mpeg1Rates[samplingFrequencyIndex];
    }
    if (versionId == 2) {
        return mpeg2Rates[samplingFrequencyIndex];
    }
    if (versionId == 0) {
        return mpeg25Rates[samplingFrequencyIndex];
    }
    return 0;
}

int Mp3FrameParser::bitrateKbpsForIndex(int versionId, int layer, int bitrateIndex)
{
    if (bitrateIndex <= 0 || bitrateIndex >= 15) {
        return 0;
    }

    static constexpr int mpeg1Layer1[] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
    static constexpr int mpeg1Layer2[] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
    static constexpr int mpeg1Layer3[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
    static constexpr int mpeg2Layer1[] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
    static constexpr int mpeg2Layer23[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};

    if (versionId == 3) {
        if (layer == 3) {
            return mpeg1Layer1[bitrateIndex];
        }
        if (layer == 2) {
            return mpeg1Layer2[bitrateIndex];
        }
        if (layer == 1) {
            return mpeg1Layer3[bitrateIndex];
        }
    } else if (versionId == 2 || versionId == 0) {
        if (layer == 3) {
            return mpeg2Layer1[bitrateIndex];
        }
        if (layer == 2 || layer == 1) {
            return mpeg2Layer23[bitrateIndex];
        }
    }
    return 0;
}

int Mp3FrameParser::readBits(const QByteArray &data, int bitOffset, int bitCount, bool *ok)
{
    int value = 0;
    for (int i = 0; i < bitCount; ++i) {
        const int bit = bitOffset + i;
        if (bit < 0 || bit / 8 >= data.size()) {
            if (ok != nullptr) {
                *ok = false;
            }
            return value;
        }
        const int bitInByte = 7 - (bit % 8);
        value = (value << 1) | ((static_cast<unsigned char>(data.at(bit / 8)) >> bitInByte) & 0x01);
    }
    return value;
}

void Mp3FrameParser::appendField(FrameAnalysis &analysis,
                                 const QString &name,
                                 int bitOffset,
                                 int bitLength,
                                 const QString &value)
{
    analysis.bitFields.append({
        QStringLiteral("mpeg_audio/header"),
        name,
        bitOffset,
        bitLength,
        value
    });
}
