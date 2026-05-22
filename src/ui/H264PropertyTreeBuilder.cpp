#include "ui/H264PropertyTreeBuilder.h"

#include "core/parser/video/h264/H264FrameAnalysisAdapter.h"
#include "core/parser/video/h264/H264Parser.h"

#include <QObject>
#include <QTreeWidgetItem>
#include <QVariant>

#include <algorithm>
#include <optional>

namespace
{
constexpr int MaxDisplayedMacroblocks = 256;
constexpr int BitFieldRole = Qt::UserRole + 1;

QTreeWidgetItem *addPair(QTreeWidgetItem *parent, const QString &field, const QString &value)
{
    return new QTreeWidgetItem(parent, {field, value});
}

QString boolValue(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

QString presentValue(bool value)
{
    return value ? QStringLiteral("present") : QStringLiteral("not present");
}

std::optional<SyntaxFieldInfo> findSyntaxField(const QVector<SyntaxFieldInfo> &fields, const QString &name)
{
    for (const SyntaxFieldInfo &field : fields) {
        if (field.name == name) {
            return field;
        }
    }
    return std::nullopt;
}
}

void addH264PropertyTreeDetails(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    const FrameSyntaxInfo syntaxInfo = h264SyntaxFromFrameAnalysis(analysis);
    auto attachSyntaxField = [](QTreeWidgetItem *item,
                                const QVector<SyntaxFieldInfo> &fields,
                                const QString &name) {
        if (item == nullptr) {
            return;
        }
        const std::optional<SyntaxFieldInfo> field = findSyntaxField(fields, name);
        if (!field.has_value()) {
            return;
        }
        item->setData(0, BitFieldRole, QVariant::fromValue(AnalysisBitField {
            QString {},
            field->name,
            field->bitOffset,
            field->bitLength,
            field->value,
            QStringLiteral("rbsp"),
            field->packetBitRanges
        }));
    };

    auto addSyntaxPair = [&attachSyntaxField](QTreeWidgetItem *parentItem,
                                              const QVector<SyntaxFieldInfo> &fields,
                                              const QString &name,
                                              const QString &value) {
        QTreeWidgetItem *item = addPair(parentItem, name, value);
        attachSyntaxField(item, fields, name);
        return item;
    };

    auto addSyntaxFields = [](QTreeWidgetItem *fieldParent, const QVector<SyntaxFieldInfo> &fields) {
        Q_UNUSED(fieldParent);
        Q_UNUSED(fields);
    };

    auto *frameRoot = new QTreeWidgetItem(parent, {QObject::tr("Frame"), QString::number(syntaxInfo.index)});
    addPair(frameRoot, QObject::tr("type"), syntaxInfo.frameType);
    addPair(frameRoot, QObject::tr("POC"), syntaxInfo.poc >= 0 ? QString::number(syntaxInfo.poc) : QStringLiteral("-"));
    addPair(frameRoot, QObject::tr("frame_num"), syntaxInfo.frameNum >= 0 ? QString::number(syntaxInfo.frameNum) : QStringLiteral("-"));
    addPair(frameRoot, QObject::tr("NALU count"), QString::number(syntaxInfo.nalus.size()));
    if (!syntaxInfo.diagnostics.isEmpty()) {
        auto *diagnosticsRoot = new QTreeWidgetItem(frameRoot, {QObject::tr("Parser diagnostics"), QString::number(syntaxInfo.diagnostics.size())});
        for (const ParserDiagnosticInfo &diagnostic : syntaxInfo.diagnostics) {
            addPair(diagnosticsRoot, diagnostic.code, diagnostic.message);
        }
    }

    auto *nalusRoot = new QTreeWidgetItem(parent, {QObject::tr("NAL Units"), QString::number(syntaxInfo.nalus.size())});
    for (int i = 0; i < syntaxInfo.nalus.size(); ++i) {
        const NaluInfo &nalu = syntaxInfo.nalus[i];
        auto *naluItem = new QTreeWidgetItem(nalusRoot, {
            QObject::tr("NALU %1").arg(i),
            QObject::tr("%1 (%2 bytes)").arg(nalu.nalUnitTypeName).arg(nalu.size)
        });
        addPair(naluItem, QObject::tr("offset"), QString::number(nalu.offset));
        addPair(naluItem, QObject::tr("size"), QString::number(nalu.size));
        addPair(naluItem, QObject::tr("forbidden_zero_bit"), QString::number(nalu.forbiddenZeroBit));
        addPair(naluItem, QObject::tr("nal_ref_idc"), QString::number(nalu.nalRefIdc));
        addPair(naluItem, QObject::tr("nal_unit_type"), QString::number(nalu.nalUnitType));
        if (!nalu.diagnostics.isEmpty()) {
            auto *diagnosticsRoot = new QTreeWidgetItem(naluItem, {QObject::tr("Parser diagnostics"), QString::number(nalu.diagnostics.size())});
            for (const ParserDiagnosticInfo &diagnostic : nalu.diagnostics) {
                addPair(diagnosticsRoot, diagnostic.code, diagnostic.message);
            }
        }

        if (nalu.sps.valid) {
            auto *spsItem = new QTreeWidgetItem(naluItem, {QObject::tr("SPS"), QString::number(nalu.sps.seqParameterSetId)});
            addSyntaxPair(spsItem, nalu.sps.fields, QStringLiteral("profile_idc"), QString::number(nalu.sps.profileIdc));
            auto *constraintsItem = new QTreeWidgetItem(spsItem, {QObject::tr("constraint flags"), QString()});
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set0_flag"), boolValue(nalu.sps.constraintSet0Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set1_flag"), boolValue(nalu.sps.constraintSet1Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set2_flag"), boolValue(nalu.sps.constraintSet2Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set3_flag"), boolValue(nalu.sps.constraintSet3Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set4_flag"), boolValue(nalu.sps.constraintSet4Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("constraint_set5_flag"), boolValue(nalu.sps.constraintSet5Flag));
            addSyntaxPair(constraintsItem, nalu.sps.fields, QStringLiteral("reserved_zero_2bits"), QString::number(nalu.sps.reservedZero2Bits));
            addSyntaxPair(spsItem, nalu.sps.fields, QStringLiteral("level_idc"), QString::number(nalu.sps.levelIdc));
            addPair(spsItem, QObject::tr("width"), QString::number(nalu.sps.width));
            addPair(spsItem, QObject::tr("height"), QString::number(nalu.sps.height));
            addPair(spsItem, QObject::tr("pic_order_cnt_type"), QString::number(nalu.sps.picOrderCntType));
            addPair(spsItem, QObject::tr("vui_parameters_present_flag"), boolValue(nalu.sps.vuiParametersPresentFlag));

            if (nalu.sps.vuiParametersPresentFlag) {
                auto *vuiItem = new QTreeWidgetItem(spsItem, {QObject::tr("VUI"), QString()});
                addPair(vuiItem, QObject::tr("aspect_ratio_info_present_flag"), boolValue(nalu.sps.aspectRatioInfoPresentFlag));
                if (nalu.sps.aspectRatioInfoPresentFlag) {
                    addPair(vuiItem, QObject::tr("aspect_ratio_idc"), QString::number(nalu.sps.aspectRatioIdc));
                    if (nalu.sps.aspectRatioIdc == 255) {
                        addPair(vuiItem, QObject::tr("sar_width"), QString::number(nalu.sps.sarWidth));
                        addPair(vuiItem, QObject::tr("sar_height"), QString::number(nalu.sps.sarHeight));
                    }
                }
                addPair(vuiItem, QObject::tr("timing_info_present_flag"), boolValue(nalu.sps.timingInfoPresentFlag));
                if (nalu.sps.timingInfoPresentFlag) {
                    addPair(vuiItem, QObject::tr("num_units_in_tick"), QString::number(nalu.sps.numUnitsInTick));
                    addPair(vuiItem, QObject::tr("time_scale"), QString::number(nalu.sps.timeScale));
                    addPair(vuiItem, QObject::tr("fixed_frame_rate_flag"), boolValue(nalu.sps.fixedFrameRateFlag));
                }
                addPair(vuiItem, QObject::tr("bitstream_restriction_flag"), boolValue(nalu.sps.bitstreamRestrictionFlag));
                if (nalu.sps.bitstreamRestrictionFlag) {
                    auto *restrictionItem = new QTreeWidgetItem(vuiItem, {QObject::tr("bitstream restriction"), QString()});
                    addPair(restrictionItem, QObject::tr("motion_vectors_over_pic_boundaries_flag"), boolValue(nalu.sps.motionVectorsOverPicBoundariesFlag));
                    addPair(restrictionItem, QObject::tr("max_bytes_per_pic_denom"), QString::number(nalu.sps.maxBytesPerPicDenom));
                    addPair(restrictionItem, QObject::tr("max_bits_per_mb_denom"), QString::number(nalu.sps.maxBitsPerMbDenom));
                    addPair(restrictionItem, QObject::tr("log2_max_mv_length_horizontal"), QString::number(nalu.sps.log2MaxMvLengthHorizontal));
                    addPair(restrictionItem, QObject::tr("log2_max_mv_length_vertical"), QString::number(nalu.sps.log2MaxMvLengthVertical));
                    addPair(restrictionItem, QObject::tr("max_num_reorder_frames"), QString::number(nalu.sps.maxNumReorderFrames));
                    addPair(restrictionItem, QObject::tr("max_dec_frame_buffering"), QString::number(nalu.sps.maxDecFrameBuffering));
                }
            }
            addSyntaxFields(spsItem, nalu.sps.fields);
        }

        if (nalu.pps.valid) {
            auto *ppsItem = new QTreeWidgetItem(naluItem, {QObject::tr("PPS"), QString::number(nalu.pps.picParameterSetId)});
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("seq_parameter_set_id"), QString::number(nalu.pps.seqParameterSetId));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("entropy_coding_mode_flag"), boolValue(nalu.pps.entropyCodingModeFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("weighted_pred_flag"), boolValue(nalu.pps.weightedPredFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("weighted_bipred_idc"), QString::number(nalu.pps.weightedBipredIdc));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("transform_8x8_mode_flag"), boolValue(nalu.pps.transform8x8ModeFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("pic_init_qp_minus26"), QString::number(nalu.pps.picInitQpMinus26));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("deblocking_filter_control_present_flag"), boolValue(nalu.pps.deblockingFilterControlPresentFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("constrained_intra_pred_flag"), boolValue(nalu.pps.constrainedIntraPredFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("redundant_pic_cnt_present_flag"), boolValue(nalu.pps.redundantPicCntPresentFlag));
            addSyntaxPair(ppsItem, nalu.pps.fields, QStringLiteral("second_chroma_qp_index_offset"), QString::number(nalu.pps.secondChromaQpIndexOffset));
            addSyntaxFields(ppsItem, nalu.pps.fields);
        }
    }

    auto *slicesRoot = new QTreeWidgetItem(parent, {QObject::tr("Slice Headers"), QString::number(syntaxInfo.slices.size())});
    for (int i = 0; i < syntaxInfo.slices.size(); ++i) {
        const SliceInfo &slice = syntaxInfo.slices[i];
        auto *sliceItem = new QTreeWidgetItem(slicesRoot, {
            QObject::tr("Slice %1").arg(i),
            slice.sliceTypeName
        });
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("first_mb_in_slice"), QString::number(slice.firstMbInSlice));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("slice_type"), QString::number(slice.sliceType));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("pic_parameter_set_id"), QString::number(slice.picParameterSetId));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("frame_num"), QString::number(slice.frameNum));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("field_pic_flag"), boolValue(slice.fieldPicFlag));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("bottom_field_flag"), boolValue(slice.bottomFieldFlag));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("idr_pic_id"), slice.idrPicId >= 0 ? QString::number(slice.idrPicId) : QStringLiteral("-"));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("pic_order_cnt_lsb"), slice.picOrderCntLsb >= 0 ? QString::number(slice.picOrderCntLsb) : QStringLiteral("-"));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("direct_spatial_mv_pred_flag"), boolValue(slice.directSpatialMvPredFlag));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("num_ref_idx_active_override_flag"), boolValue(slice.numRefIdxActiveOverrideFlag));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("num_ref_idx_l0_active_minus1"), QString::number(slice.numRefIdxL0ActiveMinus1));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("num_ref_idx_l1_active_minus1"), QString::number(slice.numRefIdxL1ActiveMinus1));
        addPair(sliceItem, QObject::tr("ref_pic_list_modification"), slice.refPicListModificationSummary.isEmpty() ? QStringLiteral("-") : slice.refPicListModificationSummary);
        addPair(sliceItem, QObject::tr("pred_weight_table"), slice.predWeightTablePresent ? slice.predWeightTableSummary : presentValue(false));
        addPair(sliceItem, QObject::tr("dec_ref_pic_marking"), slice.decRefPicMarkingPresent ? slice.decRefPicMarkingSummary : presentValue(false));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("cabac_init_idc"), slice.cabacInitIdc >= 0 ? QString::number(slice.cabacInitIdc) : QStringLiteral("-"));
        addSyntaxPair(sliceItem, slice.fields, QStringLiteral("slice_qp_delta"), QString::number(slice.sliceQpDelta));
        addPair(sliceItem, QObject::tr("derived QP"), QString::number(slice.derivedQp));
        addPair(sliceItem, QObject::tr("macroblocks_parsed"), boolValue(slice.macroblocksParsed));
        if (!slice.diagnostics.isEmpty()) {
            auto *diagnosticsRoot = new QTreeWidgetItem(sliceItem, {QObject::tr("Parser diagnostics"), QString::number(slice.diagnostics.size())});
            for (int diagnosticIndex = 0; diagnosticIndex < slice.diagnostics.size(); ++diagnosticIndex) {
                const ParserDiagnosticInfo &diagnostic = slice.diagnostics[diagnosticIndex];
                addPair(diagnosticsRoot, diagnostic.code, diagnostic.message);
            }
        }
        if (!slice.macroblockParseWarnings.isEmpty()) {
            auto *warningsRoot = new QTreeWidgetItem(sliceItem, {QObject::tr("Macroblock parse warnings"), QString::number(slice.macroblockParseWarnings.size())});
            for (int warningIndex = 0; warningIndex < slice.macroblockParseWarnings.size(); ++warningIndex) {
                addPair(warningsRoot, QObject::tr("warning %1").arg(warningIndex), slice.macroblockParseWarnings[warningIndex]);
            }
        }
        addSyntaxFields(sliceItem, slice.fields);

        auto *mbRoot = new QTreeWidgetItem(sliceItem, {QObject::tr("Macroblocks"), QString::number(slice.macroblocks.size())});
        const int displayedMacroblocks = std::min(MaxDisplayedMacroblocks, static_cast<int>(slice.macroblocks.size()));
        if (slice.macroblocks.size() > displayedMacroblocks) {
            addPair(mbRoot,
                    QObject::tr("display limit"),
                    QObject::tr("Showing first %1 of %2 macroblocks to keep the UI responsive.")
                        .arg(displayedMacroblocks)
                        .arg(slice.macroblocks.size()));
        }
        for (int mbIndex = 0; mbIndex < displayedMacroblocks; ++mbIndex) {
            const MacroblockInfo &mb = slice.macroblocks[mbIndex];
            auto *mbItem = new QTreeWidgetItem(mbRoot, {QObject::tr("MB %1").arg(mb.address), mb.mbType});
            addPair(mbItem, QObject::tr("parsed"), boolValue(mb.parsed));
            addPair(mbItem, QObject::tr("skipped"), boolValue(mb.skipped));
            addPair(mbItem, QObject::tr("prediction mode"), mb.predictionMode.isEmpty() ? QStringLiteral("-") : mb.predictionMode);
            addSyntaxPair(mbItem, mb.fields, QStringLiteral("mb_type"), mb.mbType);
            addSyntaxPair(mbItem, mb.fields, QStringLiteral("coded_block_pattern"), mb.codedBlockPattern >= 0 ? QString::number(mb.codedBlockPattern) : QStringLiteral("-"));
            addPair(mbItem, QObject::tr("coded_block_pattern_luma"), mb.codedBlockPatternLuma >= 0 ? QString::number(mb.codedBlockPatternLuma) : QStringLiteral("-"));
            addPair(mbItem, QObject::tr("coded_block_pattern_chroma"), mb.codedBlockPatternChroma >= 0 ? QString::number(mb.codedBlockPatternChroma) : QStringLiteral("-"));
            addSyntaxPair(mbItem, mb.fields, QStringLiteral("mb_qp_delta"), QString::number(mb.mbQpDelta));
            addPair(mbItem, QObject::tr("QP"), QString::number(mb.qp));
            addPair(mbItem, QObject::tr("residual parsed"), boolValue(mb.residualParsed));
            addPair(mbItem, QObject::tr("residual blocks"), QString::number(mb.residualBlockCount));
            addPair(mbItem, QObject::tr("residual coefficients"), QString::number(mb.residualCoefficientCount));
            addPair(mbItem, QObject::tr("note"), mb.note);

            if (!mb.residualBlocks.isEmpty()) {
                auto *residualRoot = new QTreeWidgetItem(mbItem, {
                    QObject::tr("Residual Blocks"),
                    QString::number(mb.residualBlocks.size())
                });
                for (int blockIndex = 0; blockIndex < mb.residualBlocks.size(); ++blockIndex) {
                    const ResidualBlockInfo &block = mb.residualBlocks[blockIndex];
                    auto *blockItem = new QTreeWidgetItem(residualRoot, {
                        QObject::tr("Block %1").arg(blockIndex),
                        QObject::tr("%1 coeffs %2/%3")
                            .arg(block.kind)
                            .arg(block.totalCoefficientCount)
                            .arg(block.maxCoefficientCount)
                    });
                    addPair(blockItem, QObject::tr("kind"), block.kind);
                    addPair(blockItem, QObject::tr("component"), QString::number(block.component));
                    addPair(blockItem, QObject::tr("block_index"), QString::number(block.blockIndex));
                    addPair(blockItem, QObject::tr("predicted_non_zero_count"), QString::number(block.predictedNonZeroCount));
                    addPair(blockItem, QObject::tr("max_coefficient_count"), QString::number(block.maxCoefficientCount));
                    addPair(blockItem, QObject::tr("total_coefficient_count"), QString::number(block.totalCoefficientCount));
                    addPair(blockItem, QObject::tr("trailing_ones"), QString::number(block.trailingOnes));
                    addPair(blockItem, QObject::tr("total_zeros"), QString::number(block.totalZeros));
                    if (!block.coefficients.isEmpty()) {
                        auto *coeffRoot = new QTreeWidgetItem(blockItem, {
                            QObject::tr("Coefficients"),
                            QString::number(block.coefficients.size())
                        });
                        for (int coeffIndex = 0; coeffIndex < block.coefficients.size(); ++coeffIndex) {
                            const ResidualBlockInfo::Coefficient &coefficient = block.coefficients[coeffIndex];
                            auto *coeffItem = new QTreeWidgetItem(coeffRoot, {
                                QObject::tr("Coeff %1").arg(coeffIndex),
                                QObject::tr("scan %1 level %2")
                                    .arg(coefficient.scanIndex)
                                    .arg(coefficient.level)
                            });
                            addPair(coeffItem, QObject::tr("scan_index"), QString::number(coefficient.scanIndex));
                            addPair(coeffItem, QObject::tr("level"), QString::number(coefficient.level));
                            addPair(coeffItem, QObject::tr("run_before"), QString::number(coefficient.runBefore));
                        }
                    }
                }
            }

            if (!mb.motionVectors.isEmpty()) {
                auto *mvRoot = new QTreeWidgetItem(mbItem, {QObject::tr("Motion Vectors"), QString::number(mb.motionVectors.size())});
                for (int mvIndex = 0; mvIndex < mb.motionVectors.size(); ++mvIndex) {
                    const MotionVectorInfo &mv = mb.motionVectors[mvIndex];
                    auto *mvItem = new QTreeWidgetItem(mvRoot, {
                        QObject::tr("MV %1").arg(mvIndex),
                        QObject::tr("L%1 ref %2").arg(mv.list).arg(mv.referenceIndex)
                    });
                    addPair(mvItem, QObject::tr("mv_x quarter-pel"), QString::number(mv.mvXQuarterPel));
                    addPair(mvItem, QObject::tr("mv_y quarter-pel"), QString::number(mv.mvYQuarterPel));
                    addPair(mvItem, QObject::tr("reference_x"), mv.referenceX >= 0 ? QString::number(mv.referenceX) : QStringLiteral("co-located"));
                    addPair(mvItem, QObject::tr("reference_y"), mv.referenceY >= 0 ? QString::number(mv.referenceY) : QStringLiteral("co-located"));
                }
            }
        }
    }
}
