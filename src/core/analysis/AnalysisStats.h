#pragma once

#include "core/model/FrameAnalysis.h"

#include <QString>
#include <QVector>

struct AnalysisDiagnosticSummary
{
    QString code;
    QString severity;
    int count = 0;
};

struct AnalysisFrameTypeCount
{
    QString type;
    int count = 0;
};

struct AnalysisStats
{
    int totalAccessUnits = 0;
    int videoAccessUnits = 0;
    int audioAccessUnits = 0;
    int otherAccessUnits = 0;
    int frameCount = 0;

    int diagnosticAccessUnits = 0;
    int diagnosticCount = 0;
    QVector<AnalysisDiagnosticSummary> diagnostics;

    int macroblockRegionCount = 0;
    int parsedMacroblockRegionCount = 0;
    int skippedMacroblockRegionCount = 0;

    int qpValueCount = 0;
    int minQp = -1;
    int maxQp = -1;
    double averageQp = 0.0;

    int motionVectorCount = 0;
    double averageMvMagnitudeQuarterPel = 0.0;
    int maxMvMagnitudeQuarterPel = 0;

    QVector<AnalysisFrameTypeCount> frameTypes;
};

AnalysisStats calculateAnalysisStats(const QVector<FrameAnalysis> &analyses);
