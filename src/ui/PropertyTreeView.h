#pragma once

#include "core/FrameAnalysis.h"

#include <QTreeWidget>

struct FrameSyntaxInfo;

class PropertyTreeView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PropertyTreeView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void showFrameAnalysis(const FrameAnalysis &analysis);

private:
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
    void addH264Details(QTreeWidgetItem *parent, const FrameSyntaxInfo &syntaxInfo);
};
