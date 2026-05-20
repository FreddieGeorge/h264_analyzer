#include "core/export/AnalysisExportWriter.h"
#include "core/parser/audio/AacAdtsParser.h"
#include "core/parser/h264/H264FrameAnalysisAdapter.h"
#include "core/parser/h264/H264Parser.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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

QByteArray loadFixture(const QString &name)
{
    const QString path = QStringLiteral(H264_ANALYZER_TEST_FIXTURE_DIR) + QLatin1Char('/') + name;
    QFile file(path);
    require(file.open(QIODevice::ReadOnly | QIODevice::Text), "open fixture");
    QByteArray hex = file.readAll();
    hex = hex.simplified();
    hex.replace(" ", "");
    const QByteArray data = QByteArray::fromHex(hex);
    require(!data.isEmpty(), "decode fixture hex");
    return data;
}

const SliceInfo &firstSlice(const FrameSyntaxInfo &frame, const char *message)
{
    require(!frame.slices.isEmpty(), message);
    return frame.slices.first();
}

bool hasDiagnosticCode(const SliceInfo &slice, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : slice.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasFrameDiagnosticCode(const FrameSyntaxInfo &frame, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : frame.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasNaluDiagnosticCode(const NaluInfo &nalu, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : nalu.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasAnalysisDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
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

QByteArray makeMinimalPpsNalu(int numRefIdxL0DefaultActiveMinus1 = 0,
                              int numRefIdxL1DefaultActiveMinus1 = 0)
{
    BitWriter rbsp;
    rbsp.writeUE(0);                                      // pic_parameter_set_id
    rbsp.writeUE(0);                                      // seq_parameter_set_id
    rbsp.writeBit(false);                                 // entropy_coding_mode_flag: CAVLC
    rbsp.writeBit(false);                                 // bottom_field_pic_order_in_frame_present_flag
    rbsp.writeUE(0);                                      // num_slice_groups_minus1
    rbsp.writeUE(numRefIdxL0DefaultActiveMinus1);         // num_ref_idx_l0_default_active_minus1
    rbsp.writeUE(numRefIdxL1DefaultActiveMinus1);         // num_ref_idx_l1_default_active_minus1
    rbsp.writeBit(false);                                 // weighted_pred_flag
    rbsp.writeBits(0, 2);                                 // weighted_bipred_idc
    rbsp.writeSE(0);                                      // pic_init_qp_minus26
    rbsp.writeSE(0);                                      // pic_init_qs_minus26
    rbsp.writeSE(0);                                      // chroma_qp_index_offset
    rbsp.writeBit(true);                                  // deblocking_filter_control_present_flag
    rbsp.writeBit(false);                                 // constrained_intra_pred_flag
    rbsp.writeBit(false);                                 // redundant_pic_cnt_present_flag

    QByteArray nalu;
    nalu.append(char(0x68));
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeP8x8SliceNalu(int mbType, int numRefIdxL0DefaultActiveMinus1 = 0)
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(0);         // slice_type: P
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(mbType);    // mb_type: P_8x8 or P_8x8ref0
    for (int i = 0; i < 4; ++i) {
        rbsp.writeUE(0);     // sub_mb_type: P_L0_8x8
    }
    if (numRefIdxL0DefaultActiveMinus1 > 0 && mbType != 4) {
        rbsp.writeBit(false); // ref_idx_l0[0] as te(v), range 1 -> 1
        rbsp.writeBit(true);  // ref_idx_l0[1] as te(v), range 1 -> 0
        rbsp.writeBit(true);  // ref_idx_l0[2] as te(v), range 1 -> 0
        rbsp.writeBit(true);  // ref_idx_l0[3] as te(v), range 1 -> 0
    }

    rbsp.writeSE(2);
    rbsp.writeSE(-1);
    rbsp.writeSE(0);
    rbsp.writeSE(1);
    rbsp.writeSE(-2);
    rbsp.writeSE(0);
    rbsp.writeSE(1);
    rbsp.writeSE(1);
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(0);         // mb_type: B_Direct_16x16

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBMotionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(3);         // mb_type: B_Bi_16x16
    rbsp.writeSE(2);         // mvd_l0[0][0][0]
    rbsp.writeSE(-1);        // mvd_l0[0][0][1]
    rbsp.writeSE(-2);        // mvd_l1[0][0][0]
    rbsp.writeSE(1);         // mvd_l1[0][0][1]
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeAnnexBPacket(std::initializer_list<QByteArray> nalus)
{
    QByteArray packet;
    for (const QByteArray &nalu : nalus) {
        packet.append(char(0x00));
        packet.append(char(0x00));
        packet.append(char(0x01));
        packet.append(nalu);
    }
    return packet;
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
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    require(frame.codecKind == CodecKind::H264, "Annex B codec kind");
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
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    require(frame.codecKind == CodecKind::H264, "AVCC codec kind");
    require(frame.nalus.size() == 1, "AVCC NALU count");
    require(frame.nalus[0].sps.valid, "AVCC SPS validity");
    require(frame.nalus[0].sps.width == 16, "AVCC SPS width");
    require(frame.nalus[0].sps.height == 16, "AVCC SPS height");
}

void testAnnexBFixtureWithIdr()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("annexb_sps_pps_idr_i.hex")), 0, 0, 0);
    require(frame.nalus.size() == 3, "fixture Annex B NALU count");
    require(frame.slices.size() == 1, "fixture Annex B frame slice count");
    require(frame.nalus[0].sps.valid, "fixture Annex B SPS validity");
    require(frame.nalus[0].sps.width == 16, "fixture Annex B SPS width");
    require(frame.nalus[0].sps.height == 16, "fixture Annex B SPS height");

    const SliceInfo &slice = firstSlice(frame, "fixture Annex B frame has slice");
    require(slice.sliceTypeName == QStringLiteral("I"), "fixture IDR slice type");
    require(slice.macroblocks.size() == 1, "fixture IDR macroblock count");
    require(slice.macroblocks.first().parsed, "fixture IDR macroblock parsed");
}

void testAvccFixtureWithSpsPps()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("avcc_sps_pps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 2, "fixture AVCC NALU count");
    require(frame.nalus[0].sps.valid, "fixture AVCC SPS validity");
    require(frame.nalus[1].pps.valid, "fixture AVCC PPS validity");
    require(frame.nalus[0].sps.width == 16, "fixture AVCC SPS width");
    require(frame.nalus[0].sps.height == 16, "fixture AVCC SPS height");
}

void testAvccLengthExceedsPacketReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("avcc_length_exceeds_packet.hex")), 0, 0, 0);
    require(frame.nalus.isEmpty(), "AVCC overrun fixture has no complete NALU");
    require(hasFrameDiagnosticCode(frame, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            "AVCC overrun fixture reports structured diagnostic");
    require(frame.slices.isEmpty(), "AVCC overrun fixture has no slices");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("avcc_length_exceeds_packet.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            "AVCC overrun diagnostic is exposed in FrameAnalysis");
}

void testTruncatedSpsReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_sps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 1, "truncated SPS fixture NALU count");
    require(frame.nalus.first().nalUnitType == 7, "truncated SPS fixture NALU type");
    require(!frame.nalus.first().sps.valid, "truncated SPS fixture is invalid");
    require(hasNaluDiagnosticCode(frame.nalus.first(), QStringLiteral("sps_truncated")),
            "truncated SPS fixture reports structured diagnostic");
    require(parser.spsMap().isEmpty(), "truncated SPS fixture is not cached");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("truncated_sps.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("sps_truncated")),
            "truncated SPS diagnostic is exposed in FrameAnalysis");
}

void testTruncatedPpsReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_pps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 1, "truncated PPS fixture NALU count");
    require(frame.nalus.first().nalUnitType == 8, "truncated PPS fixture NALU type");
    require(!frame.nalus.first().pps.valid, "truncated PPS fixture is invalid");
    require(hasNaluDiagnosticCode(frame.nalus.first(), QStringLiteral("pps_truncated")),
            "truncated PPS fixture reports structured diagnostic");
    require(parser.ppsMap().isEmpty(), "truncated PPS fixture is not cached");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("truncated_pps.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("pps_truncated")),
            "truncated PPS diagnostic is exposed in FrameAnalysis");
}

void testCavlcIFrameQpDeltaFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_i_qp_delta.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC I fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("I"), "CAVLC I fixture slice type");
    require(slice.macroblocks.size() == 2, "CAVLC I fixture macroblock count");
    require(slice.macroblocks.first().mbQpDelta == 3, "CAVLC I fixture non-zero mb_qp_delta");
    require(slice.macroblocks.first().qp == 29, "CAVLC I fixture derived macroblock QP");
    require(slice.macroblocks.first().residualParsed, "CAVLC I fixture residual parsed");
    require(slice.macroblocks.first().residualBlockCount == 17, "CAVLC I fixture residual block count");
    require(slice.macroblocks.first().residualCoefficientCount == 0, "CAVLC I fixture zero residual coefficients");
    require(slice.macroblocks[1].parsed, "CAVLC I fixture continues after residual macroblock");
    require(!hasDiagnosticCode(slice, QStringLiteral("cavlc_residual_unsupported")),
            "CAVLC I fixture has no residual unsupported diagnostic");
}

void testCavlcPSkipFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_skip.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P skip fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P skip fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P skip fixture macroblock count");
    require(slice.macroblocks.first().skipped, "CAVLC P skip fixture skipped macroblock");
    require(!slice.macroblocks.first().motionVectors.isEmpty(), "CAVLC P skip fixture motion vector");
}

void testCavlcPMotionVectorFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P MV fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P MV fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P MV fixture macroblock count");
    require(!slice.macroblocks.first().motionVectors.isEmpty(), "CAVLC P MV fixture has motion vector");

    const MotionVectorInfo &mv = slice.macroblocks.first().motionVectors.first();
    require(mv.mvXQuarterPel == 2, "CAVLC P MV fixture mv_x");
    require(mv.mvYQuarterPel == -1, "CAVLC P MV fixture mv_y");
}

void testCavlcP8x8MotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeP8x8SliceNalu(3)
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P_8x8 fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P_8x8 fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P_8x8 fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("P_8x8"), "CAVLC P_8x8 fixture mb_type");
    require(slice.macroblocks.first().parsed, "CAVLC P_8x8 fixture macroblock parsed");
    require(slice.macroblocks.first().motionVectors.size() == 4, "CAVLC P_8x8 fixture motion vector count");
    require(!hasDiagnosticCode(slice, QStringLiteral("p8x8_sub_macroblock_unsupported")),
            "CAVLC P_8x8 fixture has no unsupported diagnostic");

    const QVector<MotionVectorInfo> &mvs = slice.macroblocks.first().motionVectors;
    require(mvs[0].mvXQuarterPel == 2 && mvs[0].mvYQuarterPel == -1, "CAVLC P_8x8 fixture first mv");
    require(mvs[1].mvXQuarterPel == 0 && mvs[1].mvYQuarterPel == 1, "CAVLC P_8x8 fixture second mv");
    require(mvs[2].mvXQuarterPel == -2 && mvs[2].mvYQuarterPel == 0, "CAVLC P_8x8 fixture third mv");
    require(mvs[3].mvXQuarterPel == 1 && mvs[3].mvYQuarterPel == 1, "CAVLC P_8x8 fixture fourth mv");
}

void testCavlcP8x8Ref0MotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(1),
        makeP8x8SliceNalu(4, 1)
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P_8x8ref0 fixture has slice");
    require(slice.macroblocks.size() == 1, "CAVLC P_8x8ref0 fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("P_8x8ref0"), "CAVLC P_8x8ref0 fixture mb_type");
    require(slice.macroblocks.first().motionVectors.size() == 4, "CAVLC P_8x8ref0 fixture motion vector count");
    for (const MotionVectorInfo &mv : slice.macroblocks.first().motionVectors) {
        require(mv.referenceIndex == 0, "CAVLC P_8x8ref0 fixture forces ref_idx_l0 to zero");
    }
}

void testBSliceReportsMotionVectorDiagnostic()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "B-slice fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "B-slice fixture slice type");
    require(hasDiagnosticCode(slice, QStringLiteral("b_direct_macroblock_unsupported")),
            "B direct fixture reports structured unsupported diagnostic");
    require(!slice.macroblocks.isEmpty(), "B-slice fixture keeps estimated macroblock data");
    require(slice.macroblocks.first().mbType == QStringLiteral("B_Direct_16x16"), "B-slice fixture macroblock type");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("b_direct_macroblock_unsupported")),
            "B-slice diagnostic is exposed in FrameAnalysis");
}

void testCavlcBSliceBiMotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBMotionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC B Bi fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "CAVLC B Bi fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC B Bi fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("B_Bi_16x16"), "CAVLC B Bi fixture mb_type");
    require(slice.macroblocks.first().parsed, "CAVLC B Bi fixture macroblock parsed");
    require(slice.macroblocks.first().motionVectors.size() == 2, "CAVLC B Bi fixture motion vector count");
    require(!hasDiagnosticCode(slice, QStringLiteral("b_slice_macroblock_unsupported")),
            "CAVLC B Bi fixture has no generic unsupported diagnostic");

    const QVector<MotionVectorInfo> &mvs = slice.macroblocks.first().motionVectors;
    require(mvs[0].list == 0 && mvs[0].mvXQuarterPel == 2 && mvs[0].mvYQuarterPel == -1,
            "CAVLC B Bi fixture L0 mv");
    require(mvs[1].list == 1 && mvs[1].mvXQuarterPel == -2 && mvs[1].mvYQuarterPel == 1,
            "CAVLC B Bi fixture L1 mv");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(analysis.motionVectors.size() == 2, "CAVLC B Bi FrameAnalysis motion vectors");
    require(analysis.motionVectors[0].list == 0, "CAVLC B Bi FrameAnalysis L0 vector");
    require(analysis.motionVectors[1].list == 1, "CAVLC B Bi FrameAnalysis L1 vector");
}

void testFrameAnalysisMirrorsH264SyntaxFixture()
{
    H264Parser parser;
    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 11, 7, 3);
    require(analysis.codecKind == CodecKind::H264, "FrameAnalysis codec kind");
    require(analysis.frameIndex == 3, "FrameAnalysis packet index");
    require(analysis.pts == 11, "FrameAnalysis pts");
    require(analysis.dts == 7, "FrameAnalysis dts");
    require(analysis.frameType == QStringLiteral("P"), "FrameAnalysis frame type");
    require(analysis.hasFrame, "FrameAnalysis has frame");
    require(!analysis.units.isEmpty(), "FrameAnalysis units");
    require(!analysis.regions.isEmpty(), "FrameAnalysis regions");
    require(!analysis.motionVectors.isEmpty(), "FrameAnalysis motion vectors");
    require(analysis.regions.first().kind == AnalysisRegionKind::Macroblock, "FrameAnalysis macroblock region kind");
    require(analysis.regions.first().qp == 26, "FrameAnalysis macroblock QP");
    require(analysis.motionVectors.first().mvXQuarterPel == 2, "FrameAnalysis mv_x");
    require(analysis.motionVectors.first().mvYQuarterPel == -1, "FrameAnalysis mv_y");

    const FrameSyntaxInfo syntax = h264SyntaxFromFrameAnalysis(analysis);
    require(syntax.slices.size() == 1, "FrameAnalysis keeps H264 details");
    require(syntax.slices.first().macroblocks.size() == 1, "FrameAnalysis keeps H264 macroblocks");
}

void testFrameAnalysisExportSchemaFixture()
{
    H264Parser parser;
    FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 11, 7, 3);
    analysis.packet.streamPacketIndex = 3;
    analysis.packet.containerPacketIndex = 9;
    analysis.packet.streamIndex = 0;
    analysis.packet.mediaKind = MediaKind::Video;
    analysis.packet.codecKind = CodecKind::H264;
    analysis.packet.pts = 11;
    analysis.packet.dts = 7;
    analysis.packet.duration = 2;
    analysis.packet.position = 1234;
    analysis.packet.size = 42;
    analysis.packet.keyframe = true;
    analysis.packet.bytes = QByteArray::fromHex("00000165");

    StreamInfo stream;
    stream.fileName = QStringLiteral("fixture.264");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/fixture.264");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecName = QStringLiteral("h264");
    stream.pixelFormatName = QStringLiteral("yuv420p");
    stream.width = 16;
    stream.height = 16;
    stream.frameRate = 30.0;
    stream.streams.append({
        0,
        MediaKind::Video,
        CodecKind::H264,
        QStringLiteral("h264"),
        QStringLiteral("yuv420p"),
        QString {},
        QString {},
        0,
        0,
        16,
        16,
        30.0,
        0,
        0,
        true
    });
    stream.streams.append({
        1,
        MediaKind::Audio,
        CodecKind::AAC,
        QStringLiteral("aac"),
        QString {},
        QStringLiteral("fltp"),
        QStringLiteral("stereo"),
        0,
        0,
        0,
        0,
        0.0,
        48000,
        2,
        false
    });
    stream.isValid = true;

    const QJsonObject selected = selectedFrameExportToJson(stream,
                                                          analysis,
                                                          QStringLiteral("test-generator"),
                                                          QStringLiteral("1.0"));
    require(selected.value(QStringLiteral("schema_version")).toInt() == 3, "selected export schema version");
    require(selected.value(QStringLiteral("generator")).toString() == QStringLiteral("test-generator"),
            "selected export generator");
    const QJsonArray streams = selected.value(QStringLiteral("stream")).toObject().value(QStringLiteral("streams")).toArray();
    require(streams.size() == 2, "selected export stream discovery list");
    require(streams[1].toObject().value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "selected export audio stream media kind");
    require(streams[1].toObject().value(QStringLiteral("sample_rate")).toInt() == 48000,
            "selected export audio stream sample rate");
    require(selected.contains(QStringLiteral("frame_analysis")), "selected export has frame_analysis");
    require(selected.contains(QStringLiteral("frame")), "selected export keeps legacy frame details");

    const QJsonObject frameAnalysis = selected.value(QStringLiteral("frame_analysis")).toObject();
    require(frameAnalysis.value(QStringLiteral("media_kind")).toString() == QStringLiteral("video"),
            "frame_analysis media kind");
    require(frameAnalysis.value(QStringLiteral("access_unit_kind")).toString() == QStringLiteral("video_frame"),
            "frame_analysis access unit kind");
    require(frameAnalysis.value(QStringLiteral("has_frame")).toBool(), "frame_analysis has_frame");
    require(!frameAnalysis.value(QStringLiteral("units")).toArray().isEmpty(), "frame_analysis units");
    require(!frameAnalysis.value(QStringLiteral("regions")).toArray().isEmpty(), "frame_analysis regions");
    require(!frameAnalysis.value(QStringLiteral("motion_vectors")).toArray().isEmpty(),
            "frame_analysis motion vectors");
    const QJsonArray bitFields = frameAnalysis.value(QStringLiteral("bit_fields")).toArray();
    require(!bitFields.isEmpty(), "frame_analysis bit fields");
    require(bitFields.first().toObject().value(QStringLiteral("offset_basis")).toString() == QStringLiteral("rbsp"),
            "h264 bit fields export rbsp offset basis");
    const QJsonArray packetBitRanges = bitFields.first().toObject().value(QStringLiteral("packet_bit_ranges")).toArray();
    require(!packetBitRanges.isEmpty(), "h264 bit fields export packet bit ranges");
    require(packetBitRanges.first().toObject().value(QStringLiteral("offset_basis")).toString() == QStringLiteral("packet"),
            "h264 packet bit range basis");
    bool hasMacroblockField = false;
    for (const QJsonValue &fieldValue : bitFields) {
        const QJsonObject field = fieldValue.toObject();
        if (field.value(QStringLiteral("path")).toString().contains(QStringLiteral("macroblocks"))
            && field.value(QStringLiteral("name")).toString() == QStringLiteral("mb_type")
            && !field.value(QStringLiteral("packet_bit_ranges")).toArray().isEmpty()) {
            hasMacroblockField = true;
            break;
        }
    }
    require(hasMacroblockField, "h264 macroblock bit fields export packet ranges");
    const QJsonObject packet = frameAnalysis.value(QStringLiteral("packet")).toObject();
    require(packet.value(QStringLiteral("packet_index")).toInt() == 3, "frame_analysis packet index");
    require(packet.value(QStringLiteral("stream_packet_index")).toInt() == 3, "frame_analysis stream packet index");
    require(packet.value(QStringLiteral("container_packet_index")).toInt() == 9, "frame_analysis container packet index");
    require(packet.value(QStringLiteral("pos")).toInt() == 1234, "frame_analysis packet pos");
    require(packet.value(QStringLiteral("size")).toInt() == 42, "frame_analysis packet size");
    require(packet.value(QStringLiteral("keyframe")).toBool(), "frame_analysis packet keyframe");
    require(packet.value(QStringLiteral("raw_bytes_size")).toInt() == 4, "frame_analysis raw byte size");

    const QJsonObject legacyFrame = selected.value(QStringLiteral("frame")).toObject();
    require(!legacyFrame.value(QStringLiteral("slices")).toArray().isEmpty(), "legacy frame slices");

    const QJsonObject batch = allFramesExportToJson(stream,
                                                   QVector<FrameAnalysis> {FrameAnalysis {}, analysis},
                                                   QStringLiteral("test-generator"),
                                                   QStringLiteral("1.0"));
    const QJsonArray frames = batch.value(QStringLiteral("frames")).toArray();
    require(frames.size() == 1, "batch export skips invalid frame entries");
    require(frames.first().toObject().contains(QStringLiteral("h264")), "batch export keeps h264 details");
}

QByteArray makeAacLc44100StereoHeaderOnlyFrame()
{
    QByteArray packet;
    packet.append(char(0xff));
    packet.append(char(0xf1));
    packet.append(char(0x50));
    packet.append(char(0x80));
    packet.append(char(0x00));
    packet.append(char(0xff));
    packet.append(char(0xfc));
    return packet;
}

void testAudioFrameAnalysisExportSchemaFixture()
{
    AacAdtsParser parser;
    FrameAnalysis analysis = parser.parsePacket(makeAacLc44100StereoHeaderOnlyFrame(), 21, 19, 2);
    analysis.streamIndex = 1;

    StreamInfo stream;
    stream.fileName = QStringLiteral("fixture-audio.mp4");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/fixture-audio.mp4");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecName = QStringLiteral("h264");
    stream.streams.append({
        1,
        MediaKind::Audio,
        CodecKind::AAC,
        QStringLiteral("aac"),
        QString {},
        QStringLiteral("fltp"),
        QStringLiteral("stereo"),
        0,
        0,
        0,
        0,
        0.0,
        44100,
        2,
        false
    });
    stream.isValid = true;

    const QJsonObject selected = selectedFrameExportToJson(stream,
                                                          analysis,
                                                          QStringLiteral("test-generator"),
                                                          QStringLiteral("1.0"));
    require(selected.value(QStringLiteral("schema_version")).toInt() == 3, "audio selected export schema version");
    require(selected.contains(QStringLiteral("frame_analysis")), "audio selected export has frame_analysis");
    require(!selected.contains(QStringLiteral("frame")), "audio selected export does not invent legacy h264 frame");

    const QJsonObject frameAnalysis = selected.value(QStringLiteral("frame_analysis")).toObject();
    require(frameAnalysis.value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "audio frame_analysis media kind");
    require(frameAnalysis.value(QStringLiteral("access_unit_kind")).toString() == QStringLiteral("audio_frame"),
            "audio frame_analysis access unit kind");
    require(frameAnalysis.value(QStringLiteral("codec")).toString() == QStringLiteral("AAC"),
            "audio frame_analysis codec");
    require(frameAnalysis.value(QStringLiteral("units")).toArray().first().toObject().value(QStringLiteral("kind")).toString()
                == QStringLiteral("adts_frame"),
            "audio frame_analysis adts unit");
    require(!frameAnalysis.value(QStringLiteral("bit_fields")).toArray().isEmpty(),
            "audio frame_analysis bit fields");

    const QJsonObject batch = allFramesExportToJson(stream,
                                                   QVector<FrameAnalysis> {analysis},
                                                   QStringLiteral("test-generator"),
                                                   QStringLiteral("1.0"));
    const QJsonObject batchFrame = batch.value(QStringLiteral("frames")).toArray().first().toObject();
    require(batchFrame.value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "audio batch media kind");
    require(!batchFrame.contains(QStringLiteral("h264")), "audio batch does not include h264 block");
}

void testCavlcPResidualContinuesToMotionVectorFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_residual_then_motion_vector.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P residual fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P residual fixture slice type");
    require(slice.macroblocks.size() == 2, "CAVLC P residual fixture macroblock count");
    require(slice.macroblocks.first().mbQpDelta == 2, "CAVLC P residual fixture mb_qp_delta");
    require(slice.macroblocks.first().qp == 28, "CAVLC P residual fixture first macroblock QP");
    require(slice.macroblocks.first().residualParsed, "CAVLC P residual fixture residual parsed");
    require(slice.macroblocks.first().residualBlockCount == 4, "CAVLC P residual fixture residual block count");
    require(slice.macroblocks[1].parsed, "CAVLC P residual fixture second macroblock parsed");
    require(!slice.macroblocks[1].motionVectors.isEmpty(), "CAVLC P residual fixture later motion vector");

    const MotionVectorInfo &mv = slice.macroblocks[1].motionVectors.first();
    require(mv.mvXQuarterPel == 2, "CAVLC P residual fixture mv_x");
    require(mv.mvYQuarterPel == -1, "CAVLC P residual fixture mv_y");
}

void testUnsupportedCabacFixtureReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("unsupported_cabac_p.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CABAC fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CABAC fixture slice type");
    require(slice.macroblocks.size() == 1, "CABAC fixture estimated macroblock count");
    require(!slice.macroblocks.first().parsed, "CABAC fixture macroblock remains estimated");
    require(hasDiagnosticCode(slice, QStringLiteral("cabac_unsupported")), "CABAC fixture diagnostic");
    require(!slice.macroblockParseWarnings.isEmpty(), "CABAC fixture warning text");
}

void testTruncatedPSliceDataReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_p_slice_data.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "truncated P fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "truncated P fixture slice type");
    require(hasDiagnosticCode(slice, QStringLiteral("slice_data_truncated")),
            "truncated P fixture reports structured diagnostic");
    require(!slice.macroblocks.isEmpty(), "truncated P fixture keeps estimated macroblock data");
    require(!slice.macroblocks.first().parsed, "truncated P fixture macroblock is not marked parsed");
    require(!slice.macroblockParseWarnings.isEmpty(), "truncated P fixture warning text");
}

void testTruncatedSliceHeaderReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_slice_header.hex")), 0, 0, 0);
    require(frame.nalus.size() == 3, "truncated header fixture NALU count");
    const SliceInfo &slice = firstSlice(frame, "truncated header fixture has diagnostic slice");
    require(!slice.valid, "truncated header fixture slice is invalid");
    require(hasDiagnosticCode(slice, QStringLiteral("slice_header_truncated")),
            "truncated header fixture reports structured diagnostic");
    require(slice.macroblocks.isEmpty(), "truncated header fixture has no macroblock data");
    require(!slice.macroblockParseWarnings.isEmpty(), "truncated header fixture warning text");
}
}

int main()
{
    testExpGolomb();
    testAnnexBSps();
    testAvccSps();
    testAnnexBFixtureWithIdr();
    testAvccFixtureWithSpsPps();
    testAvccLengthExceedsPacketReportsDiagnostic();
    testTruncatedSpsReportsDiagnostic();
    testTruncatedPpsReportsDiagnostic();
    testCavlcIFrameQpDeltaFixture();
    testCavlcPSkipFixture();
    testCavlcPMotionVectorFixture();
    testCavlcP8x8MotionVectorFixture();
    testCavlcP8x8Ref0MotionVectorFixture();
    testBSliceReportsMotionVectorDiagnostic();
    testCavlcBSliceBiMotionVectorFixture();
    testFrameAnalysisMirrorsH264SyntaxFixture();
    testFrameAnalysisExportSchemaFixture();
    testAudioFrameAnalysisExportSchemaFixture();
    testCavlcPResidualContinuesToMotionVectorFixture();
    testUnsupportedCabacFixtureReportsDiagnostic();
    testTruncatedPSliceDataReportsDiagnostic();
    testTruncatedSliceHeaderReportsDiagnostic();
    std::cout << "H264Parser tests passed\n";
    return 0;
}
