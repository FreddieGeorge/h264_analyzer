#pragma once

#include "core/model/FrameAnalysis.h"

#include <QTreeWidget>

class PropertyTreeView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PropertyTreeView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void showFrameAnalysis(const FrameAnalysis &analysis);
    void selectBitField(const AnalysisBitField &field);

signals:
    void bitFieldSelected(const AnalysisBitField &field);

private:
    void handleCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    bool selectBitFieldRecursive(QTreeWidgetItem *item, const AnalysisBitField &field);
    void showContextMenu(const QPoint &position);
    QString itemRowText(const QTreeWidgetItem *item) const;
    QString itemSubtreeText(const QTreeWidgetItem *item, int depth = 0) const;
    QTreeWidgetItem *addPair(QTreeWidgetItem *parent, const QString &field, const QString &value);
    void addFrameAnalysisSummary(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addOverlayAvailability(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisUnits(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisParameterSets(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisRegions(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisMotionVectors(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisDiagnostics(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addFrameAnalysisBitFields(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
    void addCodecSpecificDetails(QTreeWidgetItem *parent, const FrameAnalysis &analysis);
};
