#include "core/AacAdtsParser.h"

#include <QString>

AacAdtsParser::AacAdtsParser() = default;

CodecKind AacAdtsParser::codecKind() const
{
    return CodecKind::AAC;
}

void AacAdtsParser::reset()
{
}

void AacAdtsParser::parseDecoderConfigurationRecord(const QByteArray &extraData)
{
    Q_UNUSED(extraData);
}

FrameAnalysis AacAdtsParser::parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex)
{
    FrameAnalysis analysis;
    analysis.frameIndex = packetIndex;
    analysis.mediaKind = MediaKind::Audio;
    analysis.accessUnitKind = AccessUnitKind::AudioFrame;
    analysis.codecKind = CodecKind::AAC;
    analysis.codecName = QStringLiteral("aac");
    analysis.pts = pts;
    analysis.dts = dts;

    if (packetData.size() < 7) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_header_truncated"),
            QStringLiteral("AAC ADTS header is shorter than 7 bytes."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    bool ok = true;
    const int syncword = readBits(packetData, 0, 12, &ok);
    appendField(analysis, QStringLiteral("syncword"), 0, 12, QStringLiteral("0x%1").arg(syncword, 3, 16, QLatin1Char('0')));
    if (!ok || syncword != 0x0fff) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_syncword_missing"),
            QStringLiteral("AAC ADTS syncword 0xFFF was not found."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    const int id = readBits(packetData, 12, 1, &ok);
    const int layer = readBits(packetData, 13, 2, &ok);
    const int protectionAbsent = readBits(packetData, 15, 1, &ok);
    const int profile = readBits(packetData, 16, 2, &ok);
    const int samplingFrequencyIndex = readBits(packetData, 18, 4, &ok);
    const int privateBit = readBits(packetData, 22, 1, &ok);
    const int channelConfiguration = readBits(packetData, 23, 3, &ok);
    const int originalCopy = readBits(packetData, 26, 1, &ok);
    const int home = readBits(packetData, 27, 1, &ok);
    const int copyrightIdBit = readBits(packetData, 28, 1, &ok);
    const int copyrightIdStart = readBits(packetData, 29, 1, &ok);
    const int frameLength = readBits(packetData, 30, 13, &ok);
    const int bufferFullness = readBits(packetData, 43, 11, &ok);
    const int rawDataBlocksMinus1 = readBits(packetData, 54, 2, &ok);

    appendField(analysis, QStringLiteral("id"), 12, 1, QString::number(id));
    appendField(analysis, QStringLiteral("layer"), 13, 2, QString::number(layer));
    appendField(analysis, QStringLiteral("protection_absent"), 15, 1, QString::number(protectionAbsent));
    appendField(analysis, QStringLiteral("profile_object_type_minus1"), 16, 2, QString::number(profile));
    appendField(analysis, QStringLiteral("profile"), 16, 2, profileName(profile));
    appendField(analysis, QStringLiteral("sampling_frequency_index"), 18, 4, QString::number(samplingFrequencyIndex));
    appendField(analysis, QStringLiteral("sample_rate"), 18, 4, QString::number(sampleRateForIndex(samplingFrequencyIndex)));
    appendField(analysis, QStringLiteral("private_bit"), 22, 1, QString::number(privateBit));
    appendField(analysis, QStringLiteral("channel_configuration"), 23, 3, QString::number(channelConfiguration));
    appendField(analysis, QStringLiteral("original_copy"), 26, 1, QString::number(originalCopy));
    appendField(analysis, QStringLiteral("home"), 27, 1, QString::number(home));
    appendField(analysis, QStringLiteral("copyright_identification_bit"), 28, 1, QString::number(copyrightIdBit));
    appendField(analysis, QStringLiteral("copyright_identification_start"), 29, 1, QString::number(copyrightIdStart));
    appendField(analysis, QStringLiteral("aac_frame_length"), 30, 13, QString::number(frameLength));
    appendField(analysis, QStringLiteral("adts_buffer_fullness"), 43, 11, QString::number(bufferFullness));
    appendField(analysis, QStringLiteral("number_of_raw_data_blocks_in_frame"), 54, 2, QString::number(rawDataBlocksMinus1 + 1));

    if (!ok) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_header_truncated"),
            QStringLiteral("AAC ADTS header ended unexpectedly."),
            QStringLiteral("warning")
        });
        return analysis;
    }

    if (layer != 0) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_invalid_layer"),
            QStringLiteral("AAC ADTS layer field must be zero."),
            QStringLiteral("warning")
        });
    }

    if (samplingFrequencyIndex == 15 || sampleRateForIndex(samplingFrequencyIndex) == 0) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_invalid_sample_rate_index"),
            QStringLiteral("AAC ADTS sampling_frequency_index is reserved or unsupported."),
            QStringLiteral("warning")
        });
    }

    if (frameLength < 7 || frameLength > packetData.size()) {
        analysis.diagnostics.append({
            QStringLiteral("packet"),
            QStringLiteral("aac_adts_frame_length_invalid"),
            QStringLiteral("AAC ADTS frame length is smaller than the header or exceeds the packet size."),
            QStringLiteral("warning")
        });
    }

    AnalysisUnit unit;
    unit.kind = AnalysisUnitKind::AdtsFrame;
    unit.offset = 0;
    unit.size = frameLength;
    unit.type = profile;
    unit.typeName = profileName(profile);
    analysis.units.append(unit);
    analysis.hasFrame = analysis.diagnostics.isEmpty()
        || !analysis.diagnostics.first().code.startsWith(QStringLiteral("aac_adts_syncword"));
    analysis.frameType = QStringLiteral("AAC ADTS");
    return analysis;
}

BitstreamParserStatePtr AacAdtsParser::snapshotState() const
{
    return nullptr;
}

void AacAdtsParser::restoreState(const BitstreamParserStatePtr &state)
{
    Q_UNUSED(state);
}

QString AacAdtsParser::profileName(int profileObjectTypeMinus1)
{
    switch (profileObjectTypeMinus1) {
    case 0: return QStringLiteral("Main");
    case 1: return QStringLiteral("LC");
    case 2: return QStringLiteral("SSR");
    case 3: return QStringLiteral("Reserved");
    default: return QStringLiteral("Unknown");
    }
}

int AacAdtsParser::sampleRateForIndex(int samplingFrequencyIndex)
{
    static constexpr int sampleRates[] = {
        96000, 88200, 64000, 48000,
        44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000,
        7350, 0, 0, 0
    };
    if (samplingFrequencyIndex < 0 || samplingFrequencyIndex >= 16) {
        return 0;
    }
    return sampleRates[samplingFrequencyIndex];
}

int AacAdtsParser::readBits(const QByteArray &data, int bitOffset, int bitCount, bool *ok)
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

void AacAdtsParser::appendField(FrameAnalysis &analysis,
                                const QString &name,
                                int bitOffset,
                                int bitLength,
                                const QString &value)
{
    analysis.bitFields.append({
        QStringLiteral("adts/header"),
        name,
        bitOffset,
        bitLength,
        value
    });
}
