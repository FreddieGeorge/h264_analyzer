#include "ui/FrameListView.h"

#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QVariant>

namespace
{
constexpr int FrameIndexRole = Qt::UserRole + 1;
constexpr int MediaKindRole = Qt::UserRole + 2;
constexpr int AccessUnitKindRole = Qt::UserRole + 3;
constexpr int AnalysisRole = Qt::UserRole + 4;
}

FrameListView::FrameListView(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName(QStringLiteral("FrameListView"));
    setColumnCount(6);
    setHeaderLabels({tr("AU Index"), tr("Stream"), tr("Media"), tr("Type"), tr("PTS"), tr("Details")});
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    connect(this, &QTreeWidget::currentItemChanged,
            this, &FrameListView::handleCurrentItemChanged);
    showPlaceholder(tr("Open a stream to populate the frame list."));
}

void FrameListView::showPlaceholder(const QString &message)
{
    clear();
    auto *item = new QTreeWidgetItem({message, QString(), QString(), QString(), QString(), QString()});
    item->setFirstColumnSpanned(true);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    addTopLevelItem(item);
}

void FrameListView::clearFrames()
{
    clear();
}

void FrameListView::addFrameAnalysis(const FrameAnalysis &analysis)
{
    if (topLevelItemCount() == 1 && topLevelItem(0)->data(0, FrameIndexRole).isNull()) {
        clear();
    }

    const QString type = !analysis.frameType.isEmpty() ? analysis.frameType : QStringLiteral("-");
    const QString kind = mediaKindName(analysis.mediaKind);
    const QString details = analysis.accessUnitKind == AccessUnitKind::VideoFrame
        ? tr("POC %1, frame_num %2")
              .arg(analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-"))
              .arg(analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-"))
        : tr("%1 fields, %2 diagnostics")
              .arg(analysis.bitFields.size())
              .arg(analysis.diagnostics.size());

    if (analysis.accessUnitKind == AccessUnitKind::VideoFrame
        && analysis.frameIndex >= 0
        && analysis.frameIndex < topLevelItemCount()) {
        QTreeWidgetItem *existing = topLevelItem(analysis.frameIndex);
        if (existing->data(0, FrameIndexRole).toInt() == analysis.frameIndex
            && static_cast<AccessUnitKind>(existing->data(0, AccessUnitKindRole).toInt()) == AccessUnitKind::VideoFrame) {
            existing->setText(3, type);
            existing->setText(4, QString::number(analysis.pts));
            existing->setText(5, details);
            existing->setData(0, AnalysisRole, QVariant::fromValue(analysis));
            existing->setHidden(!itemMatchesFilters(existing));
            return;
        }
    }

    for (int row = 0; row < topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = topLevelItem(row);
        if (existing->data(0, FrameIndexRole).toInt() == analysis.frameIndex
            && existing->data(0, MediaKindRole).toInt() == static_cast<int>(analysis.mediaKind)
            && existing->data(0, AccessUnitKindRole).toInt() == static_cast<int>(analysis.accessUnitKind)
            && existing->data(1, Qt::DisplayRole).toInt() == analysis.streamIndex) {
            existing->setText(3, type);
            existing->setText(4, QString::number(analysis.pts));
            existing->setText(5, details);
            existing->setData(0, AnalysisRole, QVariant::fromValue(analysis));
            existing->setHidden(!itemMatchesFilters(existing));
            return;
        }
    }

    auto *item = new QTreeWidgetItem({
        QString::number(analysis.frameIndex),
        QString::number(analysis.streamIndex),
        kind,
        type,
        QString::number(analysis.pts),
        details
    });
    item->setData(0, FrameIndexRole, analysis.frameIndex);
    item->setData(0, MediaKindRole, static_cast<int>(analysis.mediaKind));
    item->setData(0, AccessUnitKindRole, static_cast<int>(analysis.accessUnitKind));
    item->setData(0, AnalysisRole, QVariant::fromValue(analysis));
    addTopLevelItem(item);
    item->setHidden(!itemMatchesFilters(item));
}

bool FrameListView::selectFrameIndex(int frameIndex, bool scrollToSelection)
{
    const int verticalScrollValue = verticalScrollBar() != nullptr ? verticalScrollBar()->value() : 0;
    const int horizontalScrollValue = horizontalScrollBar() != nullptr ? horizontalScrollBar()->value() : 0;
    auto restoreScrollPosition = [this, scrollToSelection, verticalScrollValue, horizontalScrollValue]() {
        if (scrollToSelection) {
            return;
        }
        if (verticalScrollBar() != nullptr) {
            verticalScrollBar()->setValue(verticalScrollValue);
        }
        if (horizontalScrollBar() != nullptr) {
            horizontalScrollBar()->setValue(horizontalScrollValue);
        }
    };
    auto restoreScrollPositionLater = [this, scrollToSelection, restoreScrollPosition]() {
        if (!scrollToSelection) {
            QTimer::singleShot(0, this, restoreScrollPosition);
        }
    };

    if (frameIndex >= 0 && frameIndex < topLevelItemCount()) {
        QTreeWidgetItem *item = topLevelItem(frameIndex);
        if (item->data(0, FrameIndexRole).toInt() == frameIndex
            && static_cast<AccessUnitKind>(item->data(0, AccessUnitKindRole).toInt()) == AccessUnitKind::VideoFrame
            && !item->isHidden()) {
            const QSignalBlocker blocker(this);
            setCurrentItem(item);
            if (scrollToSelection) {
                scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
            restoreScrollPosition();
            restoreScrollPositionLater();
            return true;
        }
    }

    for (int row = 0; row < topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = topLevelItem(row);
        const QVariant value = item->data(0, FrameIndexRole);
        if (!value.isValid()) {
            continue;
        }

        if (value.toInt() == frameIndex) {
            if (static_cast<AccessUnitKind>(item->data(0, AccessUnitKindRole).toInt()) != AccessUnitKind::VideoFrame) {
                continue;
            }
            if (item->isHidden()) {
                continue;
            }
            const QSignalBlocker blocker(this);
            setCurrentItem(item);
            if (scrollToSelection) {
                scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
            restoreScrollPosition();
            restoreScrollPositionLater();
            return true;
        }
    }

    return false;
}

void FrameListView::setStreamFilter(int streamIndex)
{
    if (m_streamFilter == streamIndex) {
        return;
    }
    m_streamFilter = streamIndex;
    applyFilters();
}

void FrameListView::setAccessUnitFilter(AccessUnitFilter filter)
{
    if (m_accessUnitFilter == filter) {
        return;
    }
    m_accessUnitFilter = filter;
    applyFilters();
}

void FrameListView::applyFilters()
{
    for (int row = 0; row < topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = topLevelItem(row);
        item->setHidden(!itemMatchesFilters(item));
    }
}

bool FrameListView::itemMatchesFilters(const QTreeWidgetItem *item) const
{
    if (item == nullptr) {
        return false;
    }

    const QVariant frameIndex = item->data(0, FrameIndexRole);
    if (!frameIndex.isValid()) {
        return true;
    }

    const QVariant analysisValue = item->data(0, AnalysisRole);
    if (!analysisValue.canConvert<FrameAnalysis>()) {
        return true;
    }

    const FrameAnalysis analysis = analysisValue.value<FrameAnalysis>();
    if (m_streamFilter >= 0 && analysis.streamIndex != m_streamFilter) {
        return false;
    }

    switch (m_accessUnitFilter) {
    case AccessUnitFilter::Video:
        return analysis.mediaKind == MediaKind::Video;
    case AccessUnitFilter::Audio:
        return analysis.mediaKind == MediaKind::Audio;
    case AccessUnitFilter::DiagnosticsOnly:
        return !analysis.diagnostics.isEmpty();
    case AccessUnitFilter::All:
    default:
        return true;
    }
}

void FrameListView::handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current == nullptr) {
        return;
    }

    const QVariant value = current->data(0, FrameIndexRole);
    if (value.isValid()) {
        const QVariant analysisValue = current->data(0, AnalysisRole);
        if (analysisValue.canConvert<FrameAnalysis>()) {
            const FrameAnalysis analysis = analysisValue.value<FrameAnalysis>();
            emit accessUnitSelected(analysis);
            if (analysis.accessUnitKind != AccessUnitKind::VideoFrame) {
                return;
            }
        }
        emit frameSelected(value.toInt());
    }
}
