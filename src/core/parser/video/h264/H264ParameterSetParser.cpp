#include "core/parser/video/h264/H264Parser.h"

namespace
{
void skipScalingList(BitReader &reader, int sizeOfScalingList)
{
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < sizeOfScalingList; ++j) {
        if (nextScale != 0) {
            const int deltaScale = reader.readSE();
            nextScale = (lastScale + deltaScale + 256) % 256;
        }
        lastScale = nextScale == 0 ? lastScale : nextScale;
    }
}
}

SpsInfo H264Parser::parseSps(const QByteArray &rbsp) const
{
    BitReader reader(rbsp);
    SpsInfo sps;

    auto addField = [&sps](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        sps.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    sps.profileIdc = static_cast<int>(readBitsField(QStringLiteral("profile_idc"), 8));
    sps.constraintSet0Flag = readBitField(QStringLiteral("constraint_set0_flag"));
    sps.constraintSet1Flag = readBitField(QStringLiteral("constraint_set1_flag"));
    sps.constraintSet2Flag = readBitField(QStringLiteral("constraint_set2_flag"));
    sps.constraintSet3Flag = readBitField(QStringLiteral("constraint_set3_flag"));
    sps.constraintSet4Flag = readBitField(QStringLiteral("constraint_set4_flag"));
    sps.constraintSet5Flag = readBitField(QStringLiteral("constraint_set5_flag"));
    sps.reservedZero2Bits = static_cast<int>(readBitsField(QStringLiteral("reserved_zero_2bits"), 2));
    sps.levelIdc = static_cast<int>(readBitsField(QStringLiteral("level_idc"), 8));
    sps.seqParameterSetId = static_cast<int>(readUEField(QStringLiteral("seq_parameter_set_id")));

    const bool highProfile = sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122
        || sps.profileIdc == 244 || sps.profileIdc == 44 || sps.profileIdc == 83
        || sps.profileIdc == 86 || sps.profileIdc == 118 || sps.profileIdc == 128
        || sps.profileIdc == 138 || sps.profileIdc == 139 || sps.profileIdc == 134 || sps.profileIdc == 135;

    if (highProfile) {
        sps.chromaFormatIdc = static_cast<int>(readUEField(QStringLiteral("chroma_format_idc")));
        if (sps.chromaFormatIdc == 3) {
            readBitField(QStringLiteral("separate_colour_plane_flag"));
        }
        readUEField(QStringLiteral("bit_depth_luma_minus8"));
        readUEField(QStringLiteral("bit_depth_chroma_minus8"));
        readBitField(QStringLiteral("qpprime_y_zero_transform_bypass_flag"));
        if (readBitField(QStringLiteral("seq_scaling_matrix_present_flag"))) {
            const int scalingListCount = sps.chromaFormatIdc != 3 ? 8 : 12;
            for (int i = 0; i < scalingListCount; ++i) {
                if (reader.readBit()) {
                    skipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
        }
    }

    sps.log2MaxFrameNumMinus4 = static_cast<int>(readUEField(QStringLiteral("log2_max_frame_num_minus4")));
    sps.picOrderCntType = static_cast<int>(readUEField(QStringLiteral("pic_order_cnt_type")));
    if (sps.picOrderCntType == 0) {
        sps.log2MaxPicOrderCntLsbMinus4 = static_cast<int>(readUEField(QStringLiteral("log2_max_pic_order_cnt_lsb_minus4")));
    } else if (sps.picOrderCntType == 1) {
        sps.deltaPicOrderAlwaysZeroFlag = readBitField(QStringLiteral("delta_pic_order_always_zero_flag"));
        reader.readSE();
        reader.readSE();
        const quint32 cycleCount = reader.readUE();
        for (quint32 i = 0; i < cycleCount; ++i) {
            reader.readSE();
        }
    }

    readUEField(QStringLiteral("max_num_ref_frames"));
    readBitField(QStringLiteral("gaps_in_frame_num_value_allowed_flag"));
    sps.picWidthInMbsMinus1 = static_cast<int>(readUEField(QStringLiteral("pic_width_in_mbs_minus1")));
    sps.picHeightInMapUnitsMinus1 = static_cast<int>(readUEField(QStringLiteral("pic_height_in_map_units_minus1")));
    sps.frameMbsOnlyFlag = readBitField(QStringLiteral("frame_mbs_only_flag"));
    if (!sps.frameMbsOnlyFlag) {
        readBitField(QStringLiteral("mb_adaptive_frame_field_flag"));
    }
    readBitField(QStringLiteral("direct_8x8_inference_flag"));
    if (readBitField(QStringLiteral("frame_cropping_flag"))) {
        sps.frameCropLeftOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_left_offset")));
        sps.frameCropRightOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_right_offset")));
        sps.frameCropTopOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_top_offset")));
        sps.frameCropBottomOffset = static_cast<int>(readUEField(QStringLiteral("frame_crop_bottom_offset")));
    }

    auto skipHrdParameters = [&reader]() {
        const quint32 cpbCntMinus1 = reader.readUE();
        reader.readBits(4);
        reader.readBits(4);
        for (quint32 i = 0; i <= cpbCntMinus1; ++i) {
            reader.readUE();
            reader.readUE();
            reader.readBit();
        }
        reader.readBits(5);
        reader.readBits(5);
        reader.readBits(5);
        reader.readBits(5);
    };

    sps.vuiParametersPresentFlag = readBitField(QStringLiteral("vui_parameters_present_flag"));
    if (sps.vuiParametersPresentFlag) {
        sps.aspectRatioInfoPresentFlag = readBitField(QStringLiteral("aspect_ratio_info_present_flag"));
        if (sps.aspectRatioInfoPresentFlag) {
            sps.aspectRatioIdc = static_cast<int>(readBitsField(QStringLiteral("aspect_ratio_idc"), 8));
            if (sps.aspectRatioIdc == 255) {
                sps.sarWidth = static_cast<int>(readBitsField(QStringLiteral("sar_width"), 16));
                sps.sarHeight = static_cast<int>(readBitsField(QStringLiteral("sar_height"), 16));
            }
        }

        if (reader.readBit()) {
            reader.readBit();
        }
        if (reader.readBit()) {
            reader.readBits(3);
            reader.readBit();
            if (reader.readBit()) {
                reader.readBits(8);
                reader.readBits(8);
                reader.readBits(8);
            }
        }
        if (reader.readBit()) {
            reader.readUE();
            reader.readUE();
        }

        sps.timingInfoPresentFlag = readBitField(QStringLiteral("timing_info_present_flag"));
        if (sps.timingInfoPresentFlag) {
            sps.numUnitsInTick = readBitsField(QStringLiteral("num_units_in_tick"), 32);
            sps.timeScale = readBitsField(QStringLiteral("time_scale"), 32);
            sps.fixedFrameRateFlag = readBitField(QStringLiteral("fixed_frame_rate_flag"));
        }

        const bool nalHrdParametersPresentFlag = reader.readBit();
        if (nalHrdParametersPresentFlag) {
            skipHrdParameters();
        }
        const bool vclHrdParametersPresentFlag = reader.readBit();
        if (vclHrdParametersPresentFlag) {
            skipHrdParameters();
        }
        if (nalHrdParametersPresentFlag || vclHrdParametersPresentFlag) {
            reader.readBit();
        }
        reader.readBit();

        sps.bitstreamRestrictionFlag = readBitField(QStringLiteral("bitstream_restriction_flag"));
        if (sps.bitstreamRestrictionFlag) {
            sps.motionVectorsOverPicBoundariesFlag = readBitField(QStringLiteral("motion_vectors_over_pic_boundaries_flag"));
            sps.maxBytesPerPicDenom = static_cast<int>(readUEField(QStringLiteral("max_bytes_per_pic_denom")));
            sps.maxBitsPerMbDenom = static_cast<int>(readUEField(QStringLiteral("max_bits_per_mb_denom")));
            sps.log2MaxMvLengthHorizontal = static_cast<int>(readUEField(QStringLiteral("log2_max_mv_length_horizontal")));
            sps.log2MaxMvLengthVertical = static_cast<int>(readUEField(QStringLiteral("log2_max_mv_length_vertical")));
            sps.maxNumReorderFrames = static_cast<int>(readUEField(QStringLiteral("max_num_reorder_frames")));
            sps.maxDecFrameBuffering = static_cast<int>(readUEField(QStringLiteral("max_dec_frame_buffering")));
        }
    }

    const int widthMbs = sps.picWidthInMbsMinus1 + 1;
    const int heightMapUnits = sps.picHeightInMapUnitsMinus1 + 1;
    const int frameHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) * heightMapUnits;

    int cropUnitX = 1;
    int cropUnitY = 2 - (sps.frameMbsOnlyFlag ? 1 : 0);
    if (sps.chromaFormatIdc == 1) {
        cropUnitX = 2;
        cropUnitY = 2 * (2 - (sps.frameMbsOnlyFlag ? 1 : 0));
    } else if (sps.chromaFormatIdc == 2) {
        cropUnitX = 2;
        cropUnitY = 2 - (sps.frameMbsOnlyFlag ? 1 : 0);
    }

    sps.width = widthMbs * 16 - cropUnitX * (sps.frameCropLeftOffset + sps.frameCropRightOffset);
    sps.height = frameHeightInMbs * 16 - cropUnitY * (sps.frameCropTopOffset + sps.frameCropBottomOffset);
    sps.valid = !reader.hasError();
    return sps;
}

PpsInfo H264Parser::parsePps(const QByteArray &rbsp) const
{
    BitReader reader(rbsp);
    PpsInfo pps;

    auto addField = [&pps](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        pps.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readSEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };

    pps.picParameterSetId = static_cast<int>(readUEField(QStringLiteral("pic_parameter_set_id")));
    pps.seqParameterSetId = static_cast<int>(readUEField(QStringLiteral("seq_parameter_set_id")));
    pps.entropyCodingModeFlag = readBitField(QStringLiteral("entropy_coding_mode_flag"));
    pps.bottomFieldPicOrderInFramePresentFlag = readBitField(QStringLiteral("bottom_field_pic_order_in_frame_present_flag"));
    pps.numSliceGroupsMinus1 = static_cast<int>(readUEField(QStringLiteral("num_slice_groups_minus1")));

    if (pps.numSliceGroupsMinus1 > 0) {
        const quint32 sliceGroupMapType = reader.readUE();
        if (sliceGroupMapType == 0) {
            for (int i = 0; i <= pps.numSliceGroupsMinus1; ++i) {
                reader.readUE();
            }
        } else if (sliceGroupMapType == 2) {
            for (int i = 0; i < pps.numSliceGroupsMinus1; ++i) {
                reader.readUE();
                reader.readUE();
                reader.readUE();
            }
        } else if (sliceGroupMapType == 3 || sliceGroupMapType == 4 || sliceGroupMapType == 5) {
            reader.readBit();
            reader.readUE();
        } else if (sliceGroupMapType == 6) {
            const quint32 picSizeInMapUnitsMinus1 = reader.readUE();
            int bits = 0;
            while ((1U << bits) < static_cast<quint32>(pps.numSliceGroupsMinus1 + 1)) {
                ++bits;
            }
            for (quint32 i = 0; i <= picSizeInMapUnitsMinus1; ++i) {
                reader.readBits(bits);
            }
        }
    }

    pps.numRefIdxL0DefaultActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l0_default_active_minus1")));
    pps.numRefIdxL1DefaultActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l1_default_active_minus1")));
    pps.weightedPredFlag = readBitField(QStringLiteral("weighted_pred_flag"));
    pps.weightedBipredIdc = static_cast<int>(readBitsField(QStringLiteral("weighted_bipred_idc"), 2));
    pps.picInitQpMinus26 = readSEField(QStringLiteral("pic_init_qp_minus26"));
    readSEField(QStringLiteral("pic_init_qs_minus26"));
    pps.secondChromaQpIndexOffset = readSEField(QStringLiteral("chroma_qp_index_offset"));
    pps.deblockingFilterControlPresentFlag = readBitField(QStringLiteral("deblocking_filter_control_present_flag"));
    pps.constrainedIntraPredFlag = readBitField(QStringLiteral("constrained_intra_pred_flag"));
    pps.redundantPicCntPresentFlag = readBitField(QStringLiteral("redundant_pic_cnt_present_flag"));

    if (reader.moreRbspData()) {
        const SpsInfo sps = m_spsById.value(pps.seqParameterSetId);
        pps.transform8x8ModeFlag = readBitField(QStringLiteral("transform_8x8_mode_flag"));
        pps.picScalingMatrixPresentFlag = readBitField(QStringLiteral("pic_scaling_matrix_present_flag"));
        if (pps.picScalingMatrixPresentFlag) {
            const int chromaFormatIdc = sps.valid ? sps.chromaFormatIdc : 1;
            const int scalingListCount = 6 + ((chromaFormatIdc != 3) ? 2 : 6) * (pps.transform8x8ModeFlag ? 1 : 0);
            for (int i = 0; i < scalingListCount; ++i) {
                if (reader.readBit()) {
                    skipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
        }
        pps.secondChromaQpIndexOffset = readSEField(QStringLiteral("second_chroma_qp_index_offset"));
    }

    pps.valid = !reader.hasError();
    return pps;
}
