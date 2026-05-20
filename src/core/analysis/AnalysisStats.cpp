#include "core/analysis/AnalysisStats.h"

#include "core/model/MediaTypes.h"

#include <QMap>

#include <algorithm>
#include <cmath>

namespace
{
QString normalizedFrameType(const QString &frameType)
{
    return frameType.trimmed().isEmpty() ? QStringLiteral("-") : frameType.trimmed();
}

QString normalizedDiagnosticCode(const AnalysisDiagnostic &diagnostic)
{
    return diagnostic.code.trimmed().isEmpty() ? QStringLiteral("unknown") : diagnostic.code.trimmed();
}

QString normalizedDiagnosticSeverity(const AnalysisDiagnostic &diagnostic)
{
    return diagnostic.severity.trimmed().isEmpty() ? QStringLiteral("warning") : diagnostic.severity.trimmed();
}
}

AnalysisStats calculateAnalysisStats(const QVector<FrameAnalysis> &analyses)
{
    AnalysisStats stats;
    stats.totalAccessUnits = analyses.size();

    QMap<QString, int> frameTypeCounts;
    QMap<QString, AnalysisDiagnosticSummary> diagnosticCounts;
    qint64 qpSum = 0;
    double mvMagnitudeSum = 0.0;
    double maxMvMagnitude = 0.0;

    for (const FrameAnalysis &analysis : analyses) {
        switch (analysis.mediaKind) {
        case MediaKind::Video:
            ++stats.videoAccessUnits;
            break;
        case MediaKind::Audio:
            ++stats.audioAccessUnits;
            break;
        case MediaKind::Unknown:
        case MediaKind::Subtitle:
        case MediaKind::Data:
            ++stats.otherAccessUnits;
            break;
        }

        if (analysis.accessUnitKind == AccessUnitKind::VideoFrame || analysis.hasFrame) {
            ++stats.frameCount;
            ++frameTypeCounts[normalizedFrameType(analysis.frameType)];
        }

        if (!analysis.diagnostics.isEmpty()) {
            ++stats.diagnosticAccessUnits;
            stats.diagnosticCount += analysis.diagnostics.size();
        }
        for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
            const QString code = normalizedDiagnosticCode(diagnostic);
            const QString severity = normalizedDiagnosticSeverity(diagnostic);
            const QString key = severity + QLatin1Char('|') + code;
            AnalysisDiagnosticSummary &summary = diagnosticCounts[key];
            summary.code = code;
            summary.severity = severity;
            ++summary.count;
        }

        for (const AnalysisRegion &region : analysis.regions) {
            if (region.kind != AnalysisRegionKind::Macroblock) {
                continue;
            }
            ++stats.macroblockRegionCount;
            if (region.parsed) {
                ++stats.parsedMacroblockRegionCount;
            }
            if (region.skipped) {
                ++stats.skippedMacroblockRegionCount;
            }
            if (region.qp >= 0) {
                if (stats.qpValueCount == 0) {
                    stats.minQp = region.qp;
                    stats.maxQp = region.qp;
                } else {
                    stats.minQp = std::min(stats.minQp, region.qp);
                    stats.maxQp = std::max(stats.maxQp, region.qp);
                }
                qpSum += region.qp;
                ++stats.qpValueCount;
            }
        }

        for (const AnalysisMotionVector &mv : analysis.motionVectors) {
            const double magnitude = std::hypot(static_cast<double>(mv.mvXQuarterPel),
                                                static_cast<double>(mv.mvYQuarterPel));
            mvMagnitudeSum += magnitude;
            maxMvMagnitude = std::max(maxMvMagnitude, magnitude);
            ++stats.motionVectorCount;
        }
    }

    if (stats.qpValueCount > 0) {
        stats.averageQp = static_cast<double>(qpSum) / stats.qpValueCount;
    }
    if (stats.motionVectorCount > 0) {
        stats.averageMvMagnitudeQuarterPel = mvMagnitudeSum / stats.motionVectorCount;
        stats.maxMvMagnitudeQuarterPel = static_cast<int>(std::lround(maxMvMagnitude));
    }

    for (auto it = frameTypeCounts.cbegin(); it != frameTypeCounts.cend(); ++it) {
        stats.frameTypes.append(AnalysisFrameTypeCount {it.key(), it.value()});
    }
    for (auto it = diagnosticCounts.cbegin(); it != diagnosticCounts.cend(); ++it) {
        stats.diagnostics.append(it.value());
    }

    return stats;
}
