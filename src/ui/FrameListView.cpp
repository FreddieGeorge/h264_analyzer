#include "ui/FrameListView.h"

#include <QTreeWidgetItem>
#include <QVariant>

namespace
{
constexpr int FrameSyntaxRole = Qt::UserRole + 1;
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
    if (topLevelItemCount() == 1 && topLevelItem(0)->data(0, FrameSyntaxRole).isNull()) {
        clear();
    }

    const QString poc = syntaxInfo.poc >= 0 ? QString::number(syntaxInfo.poc) : QStringLiteral("-");
    const QString frameNum = syntaxInfo.frameNum >= 0 ? QString::number(syntaxInfo.frameNum) : QStringLiteral("-");
    auto *item = new QTreeWidgetItem({
        QString::number(syntaxInfo.index),
        syntaxInfo.frameType,
        poc,
        frameNum
    });
    item->setData(0, FrameSyntaxRole, QVariant::fromValue(syntaxInfo));
    addTopLevelItem(item);
}

void FrameListView::handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current == nullptr) {
        return;
    }

    const QVariant value = current->data(0, FrameSyntaxRole);
    if (value.isValid() && value.canConvert<FrameSyntaxInfo>()) {
        emit frameSyntaxSelected(value.value<FrameSyntaxInfo>());
    }
}
