#include "core/H264Parser.h"

#include <QByteArray>

#include <cstdlib>
#include <iostream>

namespace
{
class BitWriter
{
public:
    void writeBit(bool bit)
    {
        if (m_bitOffset == 0) {
            m_data.append(char(0));
        }
        if (bit) {
            m_data[m_data.size() - 1] = char(static_cast<unsigned char>(m_data[m_data.size() - 1])
                                             | static_cast<unsigned char>(1U << (7 - m_bitOffset)));
        }
        m_bitOffset = (m_bitOffset + 1) % 8;
    }

    void writeBits(quint32 value, int count)
    {
        for (int bit = count - 1; bit >= 0; --bit) {
            writeBit(((value >> bit) & 1U) != 0);
        }
    }

    void writeUE(quint32 value)
    {
        const quint32 codeNum = value + 1;
        int leadingZeroBits = 0;
        for (quint32 probe = codeNum; probe > 1; probe >>= 1) {
            ++leadingZeroBits;
        }
        for (int i = 0; i < leadingZeroBits; ++i) {
            writeBit(false);
        }
        writeBits(codeNum, leadingZeroBits + 1);
    }

    void writeSE(qint32 value)
    {
        const quint32 codeNum = value <= 0
            ? static_cast<quint32>(-value * 2)
            : static_cast<quint32>(value * 2 - 1);
        writeUE(codeNum);
    }

    QByteArray finishRbsp()
    {
        writeBit(true);
        while (m_bitOffset != 0) {
            writeBit(false);
        }
        return m_data;
    }

    QByteArray data() const
    {
        return m_data;
    }

private:
    QByteArray m_data;
    int m_bitOffset = 0;
};

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

QByteArray makeMinimalSpsNalu()
{
    BitWriter rbsp;
    rbsp.writeBits(66, 8); // profile_idc: baseline
    rbsp.writeBits(0, 8);  // constraint flags + reserved bits
    rbsp.writeBits(30, 8); // level_idc
    rbsp.writeUE(0);       // seq_parameter_set_id
    rbsp.writeUE(0);       // log2_max_frame_num_minus4
    rbsp.writeUE(0);       // pic_order_cnt_type
    rbsp.writeUE(0);       // log2_max_pic_order_cnt_lsb_minus4
    rbsp.writeUE(1);       // max_num_ref_frames
    rbsp.writeBit(false);  // gaps_in_frame_num_value_allowed_flag
    rbsp.writeUE(0);       // pic_width_in_mbs_minus1 -> 16 px
    rbsp.writeUE(0);       // pic_height_in_map_units_minus1 -> 16 px
    rbsp.writeBit(true);   // frame_mbs_only_flag
    rbsp.writeBit(true);   // direct_8x8_inference_flag
    rbsp.writeBit(false);  // frame_cropping_flag
    rbsp.writeBit(false);  // vui_parameters_present_flag

    QByteArray nalu;
    nalu.append(char(0x67));
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

void testExpGolomb()
{
    BitWriter ueWriter;
    ueWriter.writeUE(5);
    bool ok = false;
    require(H264Parser::decodeUnsignedExpGolombForTest(ueWriter.data(), &ok) == 5 && ok,
            "unsigned Exp-Golomb value 5");

    BitWriter seWriter;
    seWriter.writeSE(-2);
    require(H264Parser::decodeSignedExpGolombForTest(seWriter.data(), &ok) == -2 && ok,
            "signed Exp-Golomb value -2");
}

void testAnnexBSps()
{
    QByteArray packet;
    packet.append(char(0x00));
    packet.append(char(0x00));
    packet.append(char(0x01));
    packet.append(makeMinimalSpsNalu());

    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacket(packet, 0, 0, 0);
    require(frame.nalus.size() == 1, "Annex B NALU count");
    require(frame.nalus[0].sps.valid, "Annex B SPS validity");
    require(frame.nalus[0].sps.width == 16, "Annex B SPS width");
    require(frame.nalus[0].sps.height == 16, "Annex B SPS height");
}

void testAvccSps()
{
    const QByteArray nalu = makeMinimalSpsNalu();
    QByteArray packet;
    const int size = nalu.size();
    packet.append(char((size >> 24) & 0xff));
    packet.append(char((size >> 16) & 0xff));
    packet.append(char((size >> 8) & 0xff));
    packet.append(char(size & 0xff));
    packet.append(nalu);

    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacket(packet, 0, 0, 0);
    require(frame.nalus.size() == 1, "AVCC NALU count");
    require(frame.nalus[0].sps.valid, "AVCC SPS validity");
    require(frame.nalus[0].sps.width == 16, "AVCC SPS width");
    require(frame.nalus[0].sps.height == 16, "AVCC SPS height");
}
}

int main()
{
    testExpGolomb();
    testAnnexBSps();
    testAvccSps();
    std::cout << "H264Parser tests passed\n";
    return 0;
}
