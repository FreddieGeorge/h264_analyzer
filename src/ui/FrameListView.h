#pragma once

#include "core/H264Parser.h"

#include <QTreeWidget>

class FrameListView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit FrameListView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void clearFrames();
    void addFrameSyntax(const FrameSyntaxInfo &syntaxInfo);
    bool selectFrameIndex(int frameIndex);

signals:
    void frameSyntaxSelected(const FrameSyntaxInfo &syntaxInfo);
    void frameSelected(int frameIndex, const FrameSyntaxInfo &syntaxInfo);

private:
    void handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
};
