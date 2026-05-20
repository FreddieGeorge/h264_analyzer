#pragma once

#include "core/model/FrameAnalysis.h"

#include <QTreeWidget>

class FrameListView : public QTreeWidget
{
    Q_OBJECT

public:
    enum class AccessUnitFilter
    {
        All,
        Video,
        Audio,
        DiagnosticsOnly
    };

    explicit FrameListView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void clearFrames();
    void addFrameAnalysis(const FrameAnalysis &analysis);
    bool selectFrameIndex(int frameIndex, bool scrollToSelection = true);
    void setStreamFilter(int streamIndex);
    void setAccessUnitFilter(AccessUnitFilter filter);

signals:
    void frameSelected(int frameIndex);
    void accessUnitSelected(const FrameAnalysis &analysis);

private:
    void handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void applyFilters();
    bool itemMatchesFilters(const QTreeWidgetItem *item) const;

    int m_streamFilter = -1;
    AccessUnitFilter m_accessUnitFilter = AccessUnitFilter::All;
};
