#include "ui/PropertyTreeView.h"

#include "core/H264FrameAnalysisAdapter.h"
#include "core/H264Parser.h"

#include <QTreeWidgetItem>

#include <algorithm>

namespace
{
constexpr int MaxDisplayedMacroblocks = 256;

QString boolValue(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

QString presentValue(bool value)
{
    return value ? QStringLiteral("present") : QStringLiteral("not present");
}
}

PropertyTreeView::PropertyTreeView(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName(QStringLiteral("PropertyTreeView"));
    setColumnCount(2);
    setHeaderLabels({tr("Field"), tr("Value")});
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    showPlaceholder(tr("Open a stream to inspect syntax properties."));
}

void PropertyTreeView::showPlaceholder(const QString &message)
{
    clear();
    auto *item = new QTreeWidgetItem({message, QString()});
    item->setFirstColumnSpanned(true);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    addTopLevelItem(item);
}

void PropertyTreeView::showFrameAnalysis(const FrameAnalysis &analysis)
{
    clear();

    auto *analysisRoot = new QTreeWidgetItem(this, {tr("FrameAnalysis"), codecKindName(analysis.codecKind)});
    addFrameAnalysisSummary(analysisRoot, analysis);
    addFrameAnalysisUnits(analysisRoot, analysis);
    addFrameAnalysisParameterSets(analysisRoot, analysis);
    addFrameAnalysisRegions(analysisRoot, analysis);
    addFrameAnalysisMotionVectors(analysisRoot, analysis);
    addFrameAnalysisDiagnostics(analysisRoot, analysis);
    addFrameAnalysisBitFields(analysisRoot, analysis);
    addCodecSpecificDetails(analysisRoot, analysis);
    analysisRoot->setExpanded(true);
    expandToDepth(1);
}

void PropertyTreeView::addFrameAnalysisSummary(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *summaryRoot = new QTreeWidgetItem(parent, {tr("Summary"), analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType});
    addPair(summaryRoot, tr("frame_index"), QString::number(analysis.frameIndex));
    addPair(summaryRoot, tr("frame_type"), analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType);
    addPair(summaryRoot, tr("has_frame"), boolValue(analysis.hasFrame));
    addPair(summaryRoot, tr("pts"), QString::number(analysis.pts));
    addPair(summaryRoot, tr("dts"), QString::number(analysis.dts));
    addPair(summaryRoot, tr("POC"), analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-"));
    addPair(summaryRoot, tr("frame_num"), analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-"));
    addPair(summaryRoot, tr("units"), QString::number(analysis.units.size()));
    addPair(summaryRoot, tr("parameter_sets"), QString::number(analysis.parameterSets.size()));
    addPair(summaryRoot, tr("regions"), QString::number(analysis.regions.size()));
    addPair(summaryRoot, tr("motion_vectors"), QString::number(analysis.motionVectors.size()));
    addPair(summaryRoot, tr("diagnostics"), QString::number(analysis.diagnostics.size()));
    addPair(summaryRoot, tr("bit_fields"), QString::number(analysis.bitFields.size()));
}

void PropertyTreeView::addFrameAnalysisUnits(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    if (analysis.units.isEmpty()) {
        return;
    }

    auto *unitsRoot = new QTreeWidgetItem(parent, {tr("Units"), QString::number(analysis.units.size())});
    for (int i = 0; i < analysis.units.size(); ++i) {
        const AnalysisUnit &unit = analysis.units[i];
        auto *unitItem = new QTreeWidgetItem(unitsRoot, {
            tr("Unit %1").arg(i),
            tr("%1 %2").arg(analysisUnitKindName(unit.kind), unit.typeName)
        });
        addPair(unitItem, tr("offset"), QString::number(unit.offset));
        addPair(unitItem, tr("size"), QString::number(unit.size));
        addPair(unitItem, tr("type"), QString::number(unit.type));
    }
}

void PropertyTreeView::addFrameAnalysisParameterSets(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    if (analysis.parameterSets.isEmpty()) {
        return;
    }

    auto *setsRoot = new QTreeWidgetItem(parent, {tr("Parameter Sets"), QString::number(analysis.parameterSets.size())});
    for (const AnalysisParameterSet &parameterSet : analysis.parameterSets) {
        auto *setItem = new QTreeWidgetItem(setsRoot, {parameterSet.kind, QString::number(parameterSet.id)});
        addPair(setItem, tr("summary"), parameterSet.summary);
        addPair(setItem, tr("bit_fields"), QString::number(parameterSet.bitFields.size()));
    }
}

void PropertyTreeView::addFrameAnalysisRegions(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *regionsRoot = new QTreeWidgetItem(parent, {tr("Regions"), QString::number(analysis.regions.size())});
    const int displayedRegions = std::min(MaxDisplayedMacroblocks, static_cast<int>(analysis.regions.size()));
    if (analysis.regions.size() > displayedRegions) {
        addPair(regionsRoot,
                tr("display limit"),
                tr("Showing first %1 of %2 regions to keep the UI responsive.")
                    .arg(displayedRegions)
                    .arg(analysis.regions.size()));
    }
    for (int i = 0; i < displayedRegions; ++i) {
        const AnalysisRegion &region = analysis.regions[i];
        auto *regionItem = new QTreeWidgetItem(regionsRoot, {
            tr("Region %1").arg(region.address),
            tr("%1 %2").arg(analysisRegionKindName(region.kind), region.type)
        });
        addPair(regionItem, tr("x"), QString::number(region.x));
        addPair(regionItem, tr("y"), QString::number(region.y));
        addPair(regionItem, tr("width"), QString::number(region.width));
        addPair(regionItem, tr("height"), QString::number(region.height));
        addPair(regionItem, tr("QP"), region.qp >= 0 ? QString::number(region.qp) : QStringLiteral("-"));
        addPair(regionItem, tr("prediction mode"), region.predictionMode.isEmpty() ? QStringLiteral("-") : region.predictionMode);
        addPair(regionItem, tr("parsed"), boolValue(region.parsed));
        addPair(regionItem, tr("skipped"), boolValue(region.skipped));
        addPair(regionItem, tr("note"), region.note);
    }
}

void PropertyTreeView::addFrameAnalysisMotionVectors(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    if (analysis.motionVectors.isEmpty()) {
        return;
    }

    auto *mvRoot = new QTreeWidgetItem(parent, {tr("Motion Vectors"), QString::number(analysis.motionVectors.size())});
    for (int i = 0; i < analysis.motionVectors.size(); ++i) {
        const AnalysisMotionVector &mv = analysis.motionVectors[i];
        auto *mvItem = new QTreeWidgetItem(mvRoot, {
            tr("MV %1").arg(i),
            tr("region %1 L%2 ref %3").arg(mv.regionAddress).arg(mv.list).arg(mv.referenceIndex)
        });
        addPair(mvItem, tr("source_x"), QString::number(mv.sourceX));
        addPair(mvItem, tr("source_y"), QString::number(mv.sourceY));
        addPair(mvItem, tr("mv_x quarter-pel"), QString::number(mv.mvXQuarterPel));
        addPair(mvItem, tr("mv_y quarter-pel"), QString::number(mv.mvYQuarterPel));
        addPair(mvItem, tr("reference_x"), mv.referenceX >= 0 ? QString::number(mv.referenceX) : QStringLiteral("co-located"));
        addPair(mvItem, tr("reference_y"), mv.referenceY >= 0 ? QString::number(mv.referenceY) : QStringLiteral("co-located"));
    }
}

void PropertyTreeView::addFrameAnalysisDiagnostics(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    if (analysis.diagnostics.isEmpty()) {
        return;
    }

    auto *diagnosticsRoot = new QTreeWidgetItem(parent, {tr("Diagnostics"), QString::number(analysis.diagnostics.size())});
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        auto *diagnosticItem = new QTreeWidgetItem(diagnosticsRoot, {diagnostic.code, diagnostic.severity});
        addPair(diagnosticItem, tr("path"), diagnostic.path);
        addPair(diagnosticItem, tr("message"), diagnostic.message);
    }
}

void PropertyTreeView::addFrameAnalysisBitFields(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    if (analysis.bitFields.isEmpty()) {
        return;
    }

    auto *fieldsRoot = new QTreeWidgetItem(parent, {tr("Bit Fields"), QString::number(analysis.bitFields.size())});
    for (const AnalysisBitField &field : analysis.bitFields) {
        auto *fieldItem = new QTreeWidgetItem(fieldsRoot, {field.name, field.value});
        addPair(fieldItem, tr("path"), field.path);
        addPair(fieldItem, tr("bit_offset"), QString::number(field.bitOffset));
        addPair(fieldItem, tr("bit_length"), QString::number(field.bitLength));
    }
}

void PropertyTreeView::addCodecSpecificDetails(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *detailsRoot = new QTreeWidgetItem(parent, {tr("Codec Details"), codecKindName(analysis.codecKind)});
    if (analysis.codecKind == CodecKind::H264) {
        addH264Details(detailsRoot, h264SyntaxFromFrameAnalysis(analysis));
        return;
    }

    addPair(detailsRoot, tr("details"), tr("No codec-specific property view is available for this codec."));
}

void PropertyTreeView::addH264Details(QTreeWidgetItem *parent, const FrameSyntaxInfo &syntaxInfo)
{
    auto addSyntaxFields = [this](QTreeWidgetItem *fieldParent, const QVector<SyntaxFieldInfo> &fields) {
        if (fields.isEmpty()) {
            return;
        }

        auto *fieldsRoot = new QTreeWidgetItem(fieldParent, {tr("Bit positions"), QString::number(fields.size())});
        for (const SyntaxFieldInfo &field : fields) {
            const QString value = tr("%1 (bit %2, len %3)")
                .arg(field.value)
                .arg(field.bitOffset)
                .arg(field.bitLength);
            addPair(fieldsRoot, field.name, value);
        }
    };

    auto *frameRoot = new QTreeWidgetItem(parent, {tr("Frame"), QString::number(syntaxInfo.index)});
    addPair(frameRoot, tr("type"), syntaxInfo.frameType);
    addPair(frameRoot, tr("POC"), syntaxInfo.poc >= 0 ? QString::number(syntaxInfo.poc) : QStringLiteral("-"));
    addPair(frameRoot, tr("frame_num"), syntaxInfo.frameNum >= 0 ? QString::number(syntaxInfo.frameNum) : QStringLiteral("-"));
    addPair(frameRoot, tr("NALU count"), QString::number(syntaxInfo.nalus.size()));
    if (!syntaxInfo.diagnostics.isEmpty()) {
        auto *diagnosticsRoot = new QTreeWidgetItem(frameRoot, {tr("Parser diagnostics"), QString::number(syntaxInfo.diagnostics.size())});
        for (const ParserDiagnosticInfo &diagnostic : syntaxInfo.diagnostics) {
            addPair(diagnosticsRoot, diagnostic.code, diagnostic.message);
        }
    }

    auto *nalusRoot = new QTreeWidgetItem(parent, {tr("NAL Units"), QString::number(syntaxInfo.nalus.size())});
    for (int i = 0; i < syntaxInfo.nalus.size(); ++i) {
        const NaluInfo &nalu = syntaxInfo.nalus[i];
        auto *naluItem = new QTreeWidgetItem(nalusRoot, {
            tr("NALU %1").arg(i),
            tr("%1 (%2 bytes)").arg(nalu.nalUnitTypeName).arg(nalu.size)
        });
        addPair(naluItem, tr("offset"), QString::number(nalu.offset));
        addPair(naluItem, tr("size"), QString::number(nalu.size));
        addPair(naluItem, tr("forbidden_zero_bit"), QString::number(nalu.forbiddenZeroBit));
        addPair(naluItem, tr("nal_ref_idc"), QString::number(nalu.nalRefIdc));
        addPair(naluItem, tr("nal_unit_type"), QString::number(nalu.nalUnitType));

        if (nalu.sps.valid) {
            auto *spsItem = new QTreeWidgetItem(naluItem, {tr("SPS"), QString::number(nalu.sps.seqParameterSetId)});
            addPair(spsItem, tr("profile_idc"), QString::number(nalu.sps.profileIdc));
            auto *constraintsItem = new QTreeWidgetItem(spsItem, {tr("constraint flags"), QString()});
            addPair(constraintsItem, tr("constraint_set0_flag"), boolValue(nalu.sps.constraintSet0Flag));
            addPair(constraintsItem, tr("constraint_set1_flag"), boolValue(nalu.sps.constraintSet1Flag));
            addPair(constraintsItem, tr("constraint_set2_flag"), boolValue(nalu.sps.constraintSet2Flag));
            addPair(constraintsItem, tr("constraint_set3_flag"), boolValue(nalu.sps.constraintSet3Flag));
            addPair(constraintsItem, tr("constraint_set4_flag"), boolValue(nalu.sps.constraintSet4Flag));
            addPair(constraintsItem, tr("constraint_set5_flag"), boolValue(nalu.sps.constraintSet5Flag));
            addPair(constraintsItem, tr("reserved_zero_2bits"), QString::number(nalu.sps.reservedZero2Bits));
            addPair(spsItem, tr("level_idc"), QString::number(nalu.sps.levelIdc));
            addPair(spsItem, tr("width"), QString::number(nalu.sps.width));
            addPair(spsItem, tr("height"), QString::number(nalu.sps.height));
            addPair(spsItem, tr("pic_order_cnt_type"), QString::number(nalu.sps.picOrderCntType));
            addPair(spsItem, tr("vui_parameters_present_flag"), boolValue(nalu.sps.vuiParametersPresentFlag));

            if (nalu.sps.vuiParametersPresentFlag) {
                auto *vuiItem = new QTreeWidgetItem(spsItem, {tr("VUI"), QString()});
                addPair(vuiItem, tr("aspect_ratio_info_present_flag"), boolValue(nalu.sps.aspectRatioInfoPresentFlag));
                if (nalu.sps.aspectRatioInfoPresentFlag) {
                    addPair(vuiItem, tr("aspect_ratio_idc"), QString::number(nalu.sps.aspectRatioIdc));
                    if (nalu.sps.aspectRatioIdc == 255) {
                        addPair(vuiItem, tr("sar_width"), QString::number(nalu.sps.sarWidth));
                        addPair(vuiItem, tr("sar_height"), QString::number(nalu.sps.sarHeight));
                    }
                }
                addPair(vuiItem, tr("timing_info_present_flag"), boolValue(nalu.sps.timingInfoPresentFlag));
                if (nalu.sps.timingInfoPresentFlag) {
                    addPair(vuiItem, tr("num_units_in_tick"), QString::number(nalu.sps.numUnitsInTick));
                    addPair(vuiItem, tr("time_scale"), QString::number(nalu.sps.timeScale));
                    addPair(vuiItem, tr("fixed_frame_rate_flag"), boolValue(nalu.sps.fixedFrameRateFlag));
                }
                addPair(vuiItem, tr("bitstream_restriction_flag"), boolValue(nalu.sps.bitstreamRestrictionFlag));
                if (nalu.sps.bitstreamRestrictionFlag) {
                    auto *restrictionItem = new QTreeWidgetItem(vuiItem, {tr("bitstream restriction"), QString()});
                    addPair(restrictionItem, tr("motion_vectors_over_pic_boundaries_flag"), boolValue(nalu.sps.motionVectorsOverPicBoundariesFlag));
                    addPair(restrictionItem, tr("max_bytes_per_pic_denom"), QString::number(nalu.sps.maxBytesPerPicDenom));
                    addPair(restrictionItem, tr("max_bits_per_mb_denom"), QString::number(nalu.sps.maxBitsPerMbDenom));
                    addPair(restrictionItem, tr("log2_max_mv_length_horizontal"), QString::number(nalu.sps.log2MaxMvLengthHorizontal));
                    addPair(restrictionItem, tr("log2_max_mv_length_vertical"), QString::number(nalu.sps.log2MaxMvLengthVertical));
                    addPair(restrictionItem, tr("max_num_reorder_frames"), QString::number(nalu.sps.maxNumReorderFrames));
                    addPair(restrictionItem, tr("max_dec_frame_buffering"), QString::number(nalu.sps.maxDecFrameBuffering));
                }
            }
            addSyntaxFields(spsItem, nalu.sps.fields);
        }

        if (nalu.pps.valid) {
            auto *ppsItem = new QTreeWidgetItem(naluItem, {tr("PPS"), QString::number(nalu.pps.picParameterSetId)});
            addPair(ppsItem, tr("seq_parameter_set_id"), QString::number(nalu.pps.seqParameterSetId));
            addPair(ppsItem, tr("entropy_coding_mode_flag"), boolValue(nalu.pps.entropyCodingModeFlag));
            addPair(ppsItem, tr("weighted_pred_flag"), boolValue(nalu.pps.weightedPredFlag));
            addPair(ppsItem, tr("weighted_bipred_idc"), QString::number(nalu.pps.weightedBipredIdc));
            addPair(ppsItem, tr("transform_8x8_mode_flag"), boolValue(nalu.pps.transform8x8ModeFlag));
            addPair(ppsItem, tr("pic_init_qp_minus26"), QString::number(nalu.pps.picInitQpMinus26));
            addPair(ppsItem, tr("deblocking_filter_control_present_flag"), boolValue(nalu.pps.deblockingFilterControlPresentFlag));
            addPair(ppsItem, tr("constrained_intra_pred_flag"), boolValue(nalu.pps.constrainedIntraPredFlag));
            addPair(ppsItem, tr("redundant_pic_cnt_present_flag"), boolValue(nalu.pps.redundantPicCntPresentFlag));
            addPair(ppsItem, tr("second_chroma_qp_index_offset"), QString::number(nalu.pps.secondChromaQpIndexOffset));
            addSyntaxFields(ppsItem, nalu.pps.fields);
        }
    }

    auto *slicesRoot = new QTreeWidgetItem(parent, {tr("Slice Headers"), QString::number(syntaxInfo.slices.size())});
    for (int i = 0; i < syntaxInfo.slices.size(); ++i) {
        const SliceInfo &slice = syntaxInfo.slices[i];
        auto *sliceItem = new QTreeWidgetItem(slicesRoot, {
            tr("Slice %1").arg(i),
            slice.sliceTypeName
        });
        addPair(sliceItem, tr("first_mb_in_slice"), QString::number(slice.firstMbInSlice));
        addPair(sliceItem, tr("slice_type"), QString::number(slice.sliceType));
        addPair(sliceItem, tr("pic_parameter_set_id"), QString::number(slice.picParameterSetId));
        addPair(sliceItem, tr("frame_num"), QString::number(slice.frameNum));
        addPair(sliceItem, tr("field_pic_flag"), boolValue(slice.fieldPicFlag));
        addPair(sliceItem, tr("bottom_field_flag"), boolValue(slice.bottomFieldFlag));
        addPair(sliceItem, tr("idr_pic_id"), slice.idrPicId >= 0 ? QString::number(slice.idrPicId) : QStringLiteral("-"));
        addPair(sliceItem, tr("pic_order_cnt_lsb"), slice.picOrderCntLsb >= 0 ? QString::number(slice.picOrderCntLsb) : QStringLiteral("-"));
        addPair(sliceItem, tr("direct_spatial_mv_pred_flag"), boolValue(slice.directSpatialMvPredFlag));
        addPair(sliceItem, tr("num_ref_idx_active_override_flag"), boolValue(slice.numRefIdxActiveOverrideFlag));
        addPair(sliceItem, tr("num_ref_idx_l0_active_minus1"), QString::number(slice.numRefIdxL0ActiveMinus1));
        addPair(sliceItem, tr("num_ref_idx_l1_active_minus1"), QString::number(slice.numRefIdxL1ActiveMinus1));
        addPair(sliceItem, tr("ref_pic_list_modification"), slice.refPicListModificationSummary.isEmpty() ? QStringLiteral("-") : slice.refPicListModificationSummary);
        addPair(sliceItem, tr("pred_weight_table"), slice.predWeightTablePresent ? slice.predWeightTableSummary : presentValue(false));
        addPair(sliceItem, tr("dec_ref_pic_marking"), slice.decRefPicMarkingPresent ? slice.decRefPicMarkingSummary : presentValue(false));
        addPair(sliceItem, tr("slice_qp_delta"), QString::number(slice.sliceQpDelta));
        addPair(sliceItem, tr("derived QP"), QString::number(slice.derivedQp));
        addPair(sliceItem, tr("macroblocks_parsed"), boolValue(slice.macroblocksParsed));
        if (!slice.diagnostics.isEmpty()) {
            auto *diagnosticsRoot = new QTreeWidgetItem(sliceItem, {tr("Parser diagnostics"), QString::number(slice.diagnostics.size())});
            for (int diagnosticIndex = 0; diagnosticIndex < slice.diagnostics.size(); ++diagnosticIndex) {
                const ParserDiagnosticInfo &diagnostic = slice.diagnostics[diagnosticIndex];
                addPair(diagnosticsRoot, diagnostic.code, diagnostic.message);
            }
        }
        if (!slice.macroblockParseWarnings.isEmpty()) {
            auto *warningsRoot = new QTreeWidgetItem(sliceItem, {tr("Macroblock parse warnings"), QString::number(slice.macroblockParseWarnings.size())});
            for (int warningIndex = 0; warningIndex < slice.macroblockParseWarnings.size(); ++warningIndex) {
                addPair(warningsRoot, tr("warning %1").arg(warningIndex), slice.macroblockParseWarnings[warningIndex]);
            }
        }
        addSyntaxFields(sliceItem, slice.fields);

        auto *mbRoot = new QTreeWidgetItem(sliceItem, {tr("Macroblocks"), QString::number(slice.macroblocks.size())});
        const int displayedMacroblocks = std::min(MaxDisplayedMacroblocks, static_cast<int>(slice.macroblocks.size()));
        if (slice.macroblocks.size() > displayedMacroblocks) {
            addPair(mbRoot,
                    tr("display limit"),
                    tr("Showing first %1 of %2 macroblocks to keep the UI responsive.")
                        .arg(displayedMacroblocks)
                        .arg(slice.macroblocks.size()));
        }
        for (int mbIndex = 0; mbIndex < displayedMacroblocks; ++mbIndex) {
            const MacroblockInfo &mb = slice.macroblocks[mbIndex];
            auto *mbItem = new QTreeWidgetItem(mbRoot, {tr("MB %1").arg(mb.address), mb.mbType});
            addPair(mbItem, tr("parsed"), boolValue(mb.parsed));
            addPair(mbItem, tr("skipped"), boolValue(mb.skipped));
            addPair(mbItem, tr("prediction mode"), mb.predictionMode.isEmpty() ? QStringLiteral("-") : mb.predictionMode);
            addPair(mbItem, tr("coded_block_pattern"), mb.codedBlockPattern >= 0 ? QString::number(mb.codedBlockPattern) : QStringLiteral("-"));
            addPair(mbItem, tr("coded_block_pattern_luma"), mb.codedBlockPatternLuma >= 0 ? QString::number(mb.codedBlockPatternLuma) : QStringLiteral("-"));
            addPair(mbItem, tr("coded_block_pattern_chroma"), mb.codedBlockPatternChroma >= 0 ? QString::number(mb.codedBlockPatternChroma) : QStringLiteral("-"));
            addPair(mbItem, tr("mb_qp_delta"), QString::number(mb.mbQpDelta));
            addPair(mbItem, tr("QP"), QString::number(mb.qp));
            addPair(mbItem, tr("residual parsed"), boolValue(mb.residualParsed));
            addPair(mbItem, tr("residual blocks"), QString::number(mb.residualBlockCount));
            addPair(mbItem, tr("residual coefficients"), QString::number(mb.residualCoefficientCount));
            addPair(mbItem, tr("note"), mb.note);

            if (!mb.motionVectors.isEmpty()) {
                auto *mvRoot = new QTreeWidgetItem(mbItem, {tr("Motion Vectors"), QString::number(mb.motionVectors.size())});
                for (int mvIndex = 0; mvIndex < mb.motionVectors.size(); ++mvIndex) {
                    const MotionVectorInfo &mv = mb.motionVectors[mvIndex];
                    auto *mvItem = new QTreeWidgetItem(mvRoot, {
                        tr("MV %1").arg(mvIndex),
                        tr("L%1 ref %2").arg(mv.list).arg(mv.referenceIndex)
                    });
                    addPair(mvItem, tr("mv_x quarter-pel"), QString::number(mv.mvXQuarterPel));
                    addPair(mvItem, tr("mv_y quarter-pel"), QString::number(mv.mvYQuarterPel));
                    addPair(mvItem, tr("reference_x"), mv.referenceX >= 0 ? QString::number(mv.referenceX) : QStringLiteral("co-located"));
                    addPair(mvItem, tr("reference_y"), mv.referenceY >= 0 ? QString::number(mv.referenceY) : QStringLiteral("co-located"));
                }
            }
        }
    }

}

QTreeWidgetItem *PropertyTreeView::addPair(QTreeWidgetItem *parent, const QString &field, const QString &value)
{
    auto *item = new QTreeWidgetItem(parent, {field, value});
    return item;
}
