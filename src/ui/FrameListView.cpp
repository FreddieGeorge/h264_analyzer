#include "ui/FrameListView.h"

#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QVariant>

namespace
{
constexpr int FrameIndexRole = Qt::UserRole + 1;
}

FrameListView::FrameListView(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName(QStringLiteral("FrameListView"));
    setColumnCount(4);
    setHeaderLabels({tr("Index"), tr("Type"), tr("POC"), tr("frame_num")});
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
    auto *item = new QTreeWidgetItem({message, QString(), QString(), QString()});
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

    const QString poc = analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-");
    const QString frameNum = analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-");
    const QString type = !analysis.frameType.isEmpty() ? analysis.frameType : QStringLiteral("-");

    if (analysis.frameIndex >= 0 && analysis.frameIndex < topLevelItemCount()) {
        QTreeWidgetItem *existing = topLevelItem(analysis.frameIndex);
        if (existing->data(0, FrameIndexRole).toInt() == analysis.frameIndex) {
            existing->setText(1, type);
            existing->setText(2, poc);
            existing->setText(3, frameNum);
            return;
        }
    }

    for (int row = 0; row < topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = topLevelItem(row);
        if (existing->data(0, FrameIndexRole).toInt() == analysis.frameIndex) {
            existing->setText(1, type);
            existing->setText(2, poc);
            existing->setText(3, frameNum);
            return;
        }
    }

    auto *item = new QTreeWidgetItem({
        QString::number(analysis.frameIndex),
        type,
        poc,
        frameNum
    });
    item->setData(0, FrameIndexRole, analysis.frameIndex);
    addTopLevelItem(item);
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
        if (item->data(0, FrameIndexRole).toInt() == frameIndex) {
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

void FrameListView::handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current == nullptr) {
        return;
    }

    const QVariant value = current->data(0, FrameIndexRole);
    if (value.isValid()) {
        emit frameSelected(value.toInt());
    }
}
