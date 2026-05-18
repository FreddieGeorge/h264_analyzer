#include "ui/FrameListView.h"

#include <QSignalBlocker>
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

void FrameListView::addFrameSyntax(const FrameSyntaxInfo &syntaxInfo)
{
    if (topLevelItemCount() == 1 && topLevelItem(0)->data(0, FrameIndexRole).isNull()) {
        clear();
    }

    const QString poc = syntaxInfo.poc >= 0 ? QString::number(syntaxInfo.poc) : QStringLiteral("-");
    const QString frameNum = syntaxInfo.frameNum >= 0 ? QString::number(syntaxInfo.frameNum) : QStringLiteral("-");
    const QString type = !syntaxInfo.frameType.isEmpty() ? syntaxInfo.frameType : QStringLiteral("-");

    if (syntaxInfo.index >= 0 && syntaxInfo.index < topLevelItemCount()) {
        QTreeWidgetItem *existing = topLevelItem(syntaxInfo.index);
        if (existing->data(0, FrameIndexRole).toInt() == syntaxInfo.index) {
            existing->setText(1, type);
            existing->setText(2, poc);
            existing->setText(3, frameNum);
            return;
        }
    }

    for (int row = 0; row < topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = topLevelItem(row);
        if (existing->data(0, FrameIndexRole).toInt() == syntaxInfo.index) {
            existing->setText(1, type);
            existing->setText(2, poc);
            existing->setText(3, frameNum);
            return;
        }
    }

    auto *item = new QTreeWidgetItem({
        QString::number(syntaxInfo.index),
        type,
        poc,
        frameNum
    });
    item->setData(0, FrameIndexRole, syntaxInfo.index);
    addTopLevelItem(item);
}

bool FrameListView::selectFrameIndex(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < topLevelItemCount()) {
        QTreeWidgetItem *item = topLevelItem(frameIndex);
        if (item->data(0, FrameIndexRole).toInt() == frameIndex) {
            const QSignalBlocker blocker(this);
            setCurrentItem(item);
            scrollToItem(item, QAbstractItemView::PositionAtCenter);
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
            scrollToItem(item, QAbstractItemView::PositionAtCenter);
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
