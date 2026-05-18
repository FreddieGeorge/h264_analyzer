#include "ui/PropertyTreeView.h"

#include <QTreeWidgetItem>

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

void PropertyTreeView::showFrameSyntax(const FrameSyntaxInfo &syntaxInfo)
{
    clear();

    auto *frameRoot = new QTreeWidgetItem(this, {tr("Frame"), QString::number(syntaxInfo.index)});
    addPair(frameRoot, tr("type"), syntaxInfo.frameType);
    addPair(frameRoot, tr("POC"), syntaxInfo.poc >= 0 ? QString::number(syntaxInfo.poc) : QStringLiteral("-"));
    addPair(frameRoot, tr("frame_num"), syntaxInfo.frameNum >= 0 ? QString::number(syntaxInfo.frameNum) : QStringLiteral("-"));
    addPair(frameRoot, tr("NALU count"), QString::number(syntaxInfo.nalus.size()));

    auto *nalusRoot = new QTreeWidgetItem(this, {tr("NAL Units"), QString::number(syntaxInfo.nalus.size())});
    for (int i = 0; i < syntaxInfo.nalus.size(); ++i) {
        const NaluInfo &nalu = syntaxInfo.nalus[i];
        auto *naluItem = new QTreeWidgetItem(nalusRoot, {
            tr("NALU %1").arg(i),
            tr("%1 (%2 bytes)").arg(nalu.nalUnitTypeName).arg(nalu.size)
        });
        addPair(naluItem, tr("forbidden_zero_bit"), QString::number(nalu.forbiddenZeroBit));
        addPair(naluItem, tr("nal_ref_idc"), QString::number(nalu.nalRefIdc));
        addPair(naluItem, tr("nal_unit_type"), QString::number(nalu.nalUnitType));

        if (nalu.sps.valid) {
            auto *spsItem = new QTreeWidgetItem(naluItem, {tr("SPS"), QString::number(nalu.sps.seqParameterSetId)});
            addPair(spsItem, tr("profile_idc"), QString::number(nalu.sps.profileIdc));
            addPair(spsItem, tr("level_idc"), QString::number(nalu.sps.levelIdc));
            addPair(spsItem, tr("width"), QString::number(nalu.sps.width));
            addPair(spsItem, tr("height"), QString::number(nalu.sps.height));
            addPair(spsItem, tr("pic_order_cnt_type"), QString::number(nalu.sps.picOrderCntType));
        }

        if (nalu.pps.valid) {
            auto *ppsItem = new QTreeWidgetItem(naluItem, {tr("PPS"), QString::number(nalu.pps.picParameterSetId)});
            addPair(ppsItem, tr("seq_parameter_set_id"), QString::number(nalu.pps.seqParameterSetId));
            addPair(ppsItem, tr("entropy_coding_mode_flag"), nalu.pps.entropyCodingModeFlag ? QStringLiteral("1") : QStringLiteral("0"));
            addPair(ppsItem, tr("pic_init_qp_minus26"), QString::number(nalu.pps.picInitQpMinus26));
        }
    }

    auto *slicesRoot = new QTreeWidgetItem(this, {tr("Slice Headers"), QString::number(syntaxInfo.slices.size())});
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
        addPair(sliceItem, tr("idr_pic_id"), slice.idrPicId >= 0 ? QString::number(slice.idrPicId) : QStringLiteral("-"));
        addPair(sliceItem, tr("pic_order_cnt_lsb"), slice.picOrderCntLsb >= 0 ? QString::number(slice.picOrderCntLsb) : QStringLiteral("-"));
        addPair(sliceItem, tr("slice_qp_delta"), QString::number(slice.sliceQpDelta));
        addPair(sliceItem, tr("derived QP"), QString::number(slice.derivedQp));

        auto *mbRoot = new QTreeWidgetItem(sliceItem, {tr("Macroblocks"), QString::number(slice.macroblocks.size())});
        for (const MacroblockInfo &mb : slice.macroblocks) {
            auto *mbItem = new QTreeWidgetItem(mbRoot, {tr("MB %1").arg(mb.address), mb.mbType});
            addPair(mbItem, tr("QP"), QString::number(mb.qp));
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

    expandToDepth(1);
}

QTreeWidgetItem *PropertyTreeView::addPair(QTreeWidgetItem *parent, const QString &field, const QString &value)
{
    auto *item = new QTreeWidgetItem(parent, {field, value});
    return item;
}
