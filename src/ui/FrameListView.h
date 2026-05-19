#pragma once

#include "core/FrameAnalysis.h"

#include <QTreeWidget>

class FrameListView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit FrameListView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void clearFrames();
    void addFrameAnalysis(const FrameAnalysis &analysis);
    bool selectFrameIndex(int frameIndex);

signals:
    void frameSelected(int frameIndex);

private:
    void handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
};
