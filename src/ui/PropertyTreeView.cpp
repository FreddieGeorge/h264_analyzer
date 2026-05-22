#include "ui/PropertyTreeView.h"

#include "ui/H264PropertyTreeBuilder.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QClipboard>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QVariant>

#include <algorithm>
#include <optional>

namespace
{
constexpr int MaxDisplayedMacroblocks = 256;
constexpr int BitFieldRole = Qt::UserRole + 1;

QString packetBitRangesText(const QVector<AnalysisBitRange> &ranges)
{
    if (ranges.isEmpty()) {
        return QObject::tr("-");
    }

    QStringList parts;
    for (const AnalysisBitRange &range : ranges) {
        parts.append(QObject::tr("%1+%2").arg(range.bitOffset).arg(range.bitLength));
    }
    return parts.join(QStringLiteral(", "));
}

QString boolValue(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

bool supportsBitstreamAnalysis(const FrameAnalysis &analysis)
{
    return analysis.codecKind == CodecKind::H264 || analysis.codecKind == CodecKind::HEVC;
}

int parsedMacroblockCount(const FrameAnalysis &analysis)
{
    int count = 0;
    for (const AnalysisRegion &region : analysis.regions) {
        if (region.kind == AnalysisRegionKind::Macroblock && region.parsed) {
            ++count;
        }
    }
    return count;
}

int macroblockRegionCount(const FrameAnalysis &analysis)
{
    int count = 0;
    for (const AnalysisRegion &region : analysis.regions) {
        if (region.kind == AnalysisRegionKind::Macroblock) {
            ++count;
        }
    }
    return count;
}

struct QpSummary
{
    int count = 0;
    int min = 0;
    int max = 0;
};

std::optional<QpSummary> summarizeQpValues(const FrameAnalysis &analysis)
{
    QpSummary summary;
    bool haveValue = false;
    for (const AnalysisRegion &region : analysis.regions) {
        if (region.kind != AnalysisRegionKind::Macroblock || region.qp < 0) {
            continue;
        }
        if (!haveValue) {
            summary.min = region.qp;
            summary.max = region.qp;
            haveValue = true;
        } else {
            summary.min = std::min(summary.min, region.qp);
            summary.max = std::max(summary.max, region.qp);
        }
        ++summary.count;
    }

    if (!haveValue) {
        return std::nullopt;
    }
    return summary;
}

bool hasDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

QString qpAvailabilityText(const FrameAnalysis &analysis)
{
    const std::optional<QpSummary> summary = summarizeQpValues(analysis);
    if (!summary.has_value()) {
        return QObject::tr("No parsed QP values for this frame.");
    }

    if (summary->min == summary->max) {
        return QObject::tr("%1 values, constant QP %2. Flat color is expected.")
            .arg(summary->count)
            .arg(summary->min);
    }

    return QObject::tr("%1 values, range %2 - %3. Lower QP is greener; higher QP is redder.")
        .arg(summary->count)
        .arg(summary->min)
        .arg(summary->max);
}

QString motionVectorAvailabilityText(const FrameAnalysis &analysis)
{
    if (!supportsBitstreamAnalysis(analysis)) {
        return QObject::tr("Motion vector analysis is not supported for this codec yet.");
    }

    if (!analysis.motionVectors.isEmpty()) {
        return QObject::tr("%1 parsed motion vectors")
            .arg(analysis.motionVectors.size());
    }

    if (analysis.frameType == QStringLiteral("I")) {
        return QObject::tr("No motion vectors are expected for this I-frame.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("cabac_unsupported"))) {
        return QObject::tr("This frame uses CABAC; CABAC macroblock and motion vector parsing is not implemented yet.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("b_direct_macroblock_unsupported"))) {
        return QObject::tr("This frame uses B_Direct motion vectors, which are not implemented yet.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("b8x8_sub_macroblock_unsupported"))) {
        return QObject::tr("This frame uses B_8x8 sub-macroblock motion vectors, which are not implemented yet.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("b_slice_macroblock_unsupported"))) {
        return QObject::tr("This frame uses an unsupported B-slice macroblock type.");
    }

    if (analysis.frameType == QStringLiteral("B")) {
        return QObject::tr("No supported B-slice motion vectors were parsed for this frame.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_unsupported"))
        || hasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_type_unsupported"))) {
        return QObject::tr("This frame uses an unsupported P_8x8 sub-macroblock type.");
    }

    if (hasDiagnosticCode(analysis, QStringLiteral("interlaced_or_fmo_unsupported"))) {
        return QObject::tr("Interlaced/MBAFF or FMO motion vector parsing is not implemented yet.");
    }

    return QObject::tr("No supported motion vectors were parsed for this frame. Current parser mainly exposes H.264 P-slice L0 vectors.");
}

QString gridAvailabilityText(const FrameAnalysis &analysis)
{
    if (analysis.regions.isEmpty()) {
        return QObject::tr("Available from frame dimensions; no parsed macroblock regions for this frame.");
    }

    return QObject::tr("Available; %1 macroblock regions in analysis data.")
        .arg(macroblockRegionCount(analysis));
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
    setUniformRowHeights(false);
    setWordWrap(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTreeWidget::currentItemChanged,
            this, &PropertyTreeView::handleCurrentItemChanged);
    connect(this, &QTreeWidget::customContextMenuRequested,
            this, &PropertyTreeView::showContextMenu);
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
    if (analysis.mediaKind == MediaKind::Video) {
        addOverlayAvailability(analysisRoot, analysis);
    }
    addFrameAnalysisUnits(analysisRoot, analysis);
    addFrameAnalysisParameterSets(analysisRoot, analysis);
    addFrameAnalysisRegions(analysisRoot, analysis);
    addFrameAnalysisMotionVectors(analysisRoot, analysis);
    addFrameAnalysisDiagnostics(analysisRoot, analysis);
    if (analysis.codecKind != CodecKind::H264) {
        addFrameAnalysisBitFields(analysisRoot, analysis);
    }
    addCodecSpecificDetails(analysisRoot, analysis);
    analysisRoot->setExpanded(true);
    expandToDepth(1);
}

void PropertyTreeView::handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current == nullptr) {
        return;
    }

    const QVariant value = current->data(0, BitFieldRole);
    if (value.canConvert<AnalysisBitField>()) {
        emit bitFieldSelected(value.value<AnalysisBitField>());
    }
}

void PropertyTreeView::selectBitField(const AnalysisBitField &field)
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (selectBitFieldRecursive(topLevelItem(i), field)) {
            return;
        }
    }
}

bool PropertyTreeView::selectBitFieldRecursive(QTreeWidgetItem *item, const AnalysisBitField &field)
{
    if (item == nullptr) {
        return false;
    }

    const QVariant value = item->data(0, BitFieldRole);
    if (value.canConvert<AnalysisBitField>()) {
        const AnalysisBitField candidate = value.value<AnalysisBitField>();
        if (candidate.bitOffset == field.bitOffset
            && candidate.bitLength == field.bitLength
            && (candidate.name == field.name || candidate.path == field.path)) {
            setCurrentItem(item);
            scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return true;
        }
    }

    for (int i = 0; i < item->childCount(); ++i) {
        if (selectBitFieldRecursive(item->child(i), field)) {
            item->setExpanded(true);
            return true;
        }
    }
    return false;
}

void PropertyTreeView::addFrameAnalysisSummary(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *summaryRoot = new QTreeWidgetItem(parent, {tr("Summary"), analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType});
    addPair(summaryRoot, tr("frame_index"), QString::number(analysis.frameIndex));
    addPair(summaryRoot, tr("stream_index"), QString::number(analysis.streamIndex));
    addPair(summaryRoot, tr("media_kind"), mediaKindName(analysis.mediaKind));
    addPair(summaryRoot, tr("access_unit_kind"), accessUnitKindName(analysis.accessUnitKind));
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

    if (analysis.packet.streamPacketIndex >= 0 || analysis.packet.containerPacketIndex >= 0 || analysis.packet.size > 0) {
        auto *packetRoot = new QTreeWidgetItem(parent, {tr("Packet"), QString()});
        addPair(packetRoot, tr("stream_packet_index"), QString::number(analysis.packet.streamPacketIndex));
        addPair(packetRoot, tr("container_packet_index"), QString::number(analysis.packet.containerPacketIndex));
        addPair(packetRoot, tr("stream_index"), QString::number(analysis.packet.streamIndex));
        addPair(packetRoot, tr("media_kind"), mediaKindName(analysis.packet.mediaKind));
        addPair(packetRoot, tr("codec"), codecKindName(analysis.packet.codecKind));
        addPair(packetRoot, tr("pts"), QString::number(analysis.packet.pts));
        addPair(packetRoot, tr("dts"), QString::number(analysis.packet.dts));
        addPair(packetRoot, tr("duration"), QString::number(analysis.packet.duration));
        addPair(packetRoot, tr("pos"), QString::number(analysis.packet.position));
        addPair(packetRoot, tr("size"), QString::number(analysis.packet.size));
        addPair(packetRoot, tr("keyframe"), boolValue(analysis.packet.keyframe));
        addPair(packetRoot, tr("raw_bytes"), tr("%1 bytes available").arg(analysis.packet.bytes.size()));
    }
}

void PropertyTreeView::addOverlayAvailability(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *overlayRoot = new QTreeWidgetItem(parent, {tr("Overlay Availability"), codecKindName(analysis.codecKind)});
    addPair(overlayRoot, tr("bitstream analysis"),
            supportsBitstreamAnalysis(analysis)
                ? tr("%1 supported").arg(codecKindName(analysis.codecKind))
                : tr("not supported for this codec yet"));
    addPair(overlayRoot, tr("macroblock grid"), gridAvailabilityText(analysis));
    addPair(overlayRoot, tr("macroblock regions"), QString::number(macroblockRegionCount(analysis)));
    addPair(overlayRoot, tr("fully parsed macroblocks"), tr("%1 / %2")
        .arg(parsedMacroblockCount(analysis))
        .arg(macroblockRegionCount(analysis)));
    addPair(overlayRoot, tr("QP heatmap"), qpAvailabilityText(analysis));
    addPair(overlayRoot, tr("motion vectors"), motionVectorAvailabilityText(analysis));
    addPair(overlayRoot, tr("diagnostics"), QString::number(analysis.diagnostics.size()));
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
        if (analysis.codecKind == CodecKind::H264) {
            continue;
        }
        addPair(setItem, tr("bit_fields"), QString::number(parameterSet.bitFields.size()));
        for (const AnalysisBitField &field : parameterSet.bitFields) {
            auto *fieldItem = new QTreeWidgetItem(setItem, {field.name, field.value});
            fieldItem->setData(0, BitFieldRole, QVariant::fromValue(field));
            addPair(fieldItem, tr("path"), field.path);
            addPair(fieldItem, tr("bit_offset"), QString::number(field.bitOffset));
            addPair(fieldItem, tr("bit_length"), QString::number(field.bitLength));
            addPair(fieldItem, tr("offset_basis"), field.offsetBasis);
            addPair(fieldItem, tr("packet_bit_ranges"), packetBitRangesText(field.packetBitRanges));
        }
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
    auto *mvRoot = new QTreeWidgetItem(parent, {tr("Motion Vectors"), QString::number(analysis.motionVectors.size())});
    if (analysis.motionVectors.isEmpty()) {
        addPair(mvRoot, tr("availability"), motionVectorAvailabilityText(analysis));
        return;
    }

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
        fieldItem->setData(0, BitFieldRole, QVariant::fromValue(field));
        addPair(fieldItem, tr("path"), field.path);
        addPair(fieldItem, tr("bit_offset"), QString::number(field.bitOffset));
        addPair(fieldItem, tr("bit_length"), QString::number(field.bitLength));
        addPair(fieldItem, tr("offset_basis"), field.offsetBasis);
        addPair(fieldItem, tr("packet_bit_ranges"), packetBitRangesText(field.packetBitRanges));
    }
}

void PropertyTreeView::addCodecSpecificDetails(QTreeWidgetItem *parent, const FrameAnalysis &analysis)
{
    auto *detailsRoot = new QTreeWidgetItem(parent, {tr("Codec Details"), codecKindName(analysis.codecKind)});
    if (analysis.codecKind == CodecKind::H264) {
        addH264PropertyTreeDetails(detailsRoot, analysis);
        return;
    }

    addPair(detailsRoot, tr("details"), tr("No codec-specific property view is available for this codec."));
}

QTreeWidgetItem *PropertyTreeView::addPair(QTreeWidgetItem *parent, const QString &field, const QString &value)
{
    auto *item = new QTreeWidgetItem(parent, {field, value});
    return item;
}

void PropertyTreeView::showContextMenu(const QPoint &position)
{
    QTreeWidgetItem *item = itemAt(position);
    if (item == nullptr) {
        return;
    }

    const int column = columnAt(position.x());
    QMenu menu(this);
    QAction *copyCellAction = menu.addAction(tr("Copy Cell"));
    QAction *copyRowAction = menu.addAction(tr("Copy Row"));
    QAction *copySubtreeAction = menu.addAction(tr("Copy Row and Children"));

    QAction *selected = menu.exec(viewport()->mapToGlobal(position));
    if (selected == nullptr) {
        return;
    }

    QString text;
    if (selected == copyCellAction) {
        text = item->text(column >= 0 ? column : 0);
    } else if (selected == copyRowAction) {
        text = itemRowText(item);
    } else if (selected == copySubtreeAction) {
        text = itemSubtreeText(item);
    }

    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

QString PropertyTreeView::itemRowText(const QTreeWidgetItem *item) const
{
    if (item == nullptr) {
        return {};
    }

    const QString field = item->text(0);
    const QString value = item->text(1);
    return value.isEmpty() ? field : QStringLiteral("%1\t%2").arg(field, value);
}

QString PropertyTreeView::itemSubtreeText(const QTreeWidgetItem *item, int depth) const
{
    if (item == nullptr) {
        return {};
    }

    QString text = QString(depth * 2, QLatin1Char(' ')) + itemRowText(item);
    for (int i = 0; i < item->childCount(); ++i) {
        text += QLatin1Char('\n') + itemSubtreeText(item->child(i), depth + 1);
    }
    return text;
}
