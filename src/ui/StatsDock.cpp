#include "ui/StatsDock.h"

#include <QHeaderView>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QString percentText(int part, int total)
{
    if (total <= 0) {
        return QObject::tr("-");
    }
    return QObject::tr("%1%").arg(static_cast<double>(part) * 100.0 / total, 0, 'f', 1);
}

QString averageText(double value)
{
    return QString::number(value, 'f', 2);
}

QString distributionBar(int part, int total)
{
    constexpr int BarWidth = 12;
    if (part <= 0 || total <= 0) {
        return QString(BarWidth, QLatin1Char('.'));
    }

    const int filled = std::max(1, static_cast<int>((static_cast<double>(part) / total) * BarWidth + 0.5));
    return QString(std::min(filled, BarWidth), QLatin1Char('#'))
        + QString(BarWidth - std::min(filled, BarWidth), QLatin1Char('.'));
}
}

StatsDock::StatsDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("AnalysisStatsTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Metric"), tr("Value")});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    layout->addWidget(m_tree);
    showPlaceholder(tr("Open a stream to populate analysis statistics."));
}

void StatsDock::showPlaceholder(const QString &message)
{
    m_tree->clear();
    auto *item = new QTreeWidgetItem({message, QString()});
    item->setFirstColumnSpanned(true);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    m_tree->addTopLevelItem(item);
}

void StatsDock::setStats(const AnalysisStats &stats)
{
    m_tree->clear();

    QTreeWidgetItem *overview = addSection(tr("Overview"));
    addMetric(overview, tr("Access units"), QString::number(stats.totalAccessUnits));
    addMetric(overview, tr("Video access units"),
              tr("%1 (%2)").arg(stats.videoAccessUnits).arg(percentText(stats.videoAccessUnits, stats.totalAccessUnits)));
    addMetric(overview, tr("Audio access units"),
              tr("%1 (%2)").arg(stats.audioAccessUnits).arg(percentText(stats.audioAccessUnits, stats.totalAccessUnits)));
    addMetric(overview, tr("Other access units"),
              tr("%1 (%2)").arg(stats.otherAccessUnits).arg(percentText(stats.otherAccessUnits, stats.totalAccessUnits)));
    addMetric(overview, tr("Decoded frames"), QString::number(stats.frameCount));

    QTreeWidgetItem *regions = addSection(tr("Regions"));
    addMetric(regions, tr("Macroblocks"), QString::number(stats.macroblockRegionCount));
    addMetric(regions, tr("Parsed macroblocks"),
              tr("%1 (%2)").arg(stats.parsedMacroblockRegionCount).arg(percentText(stats.parsedMacroblockRegionCount,
                                                                                   stats.macroblockRegionCount)));
    addMetric(regions, tr("Skipped macroblocks"),
              tr("%1 (%2)").arg(stats.skippedMacroblockRegionCount).arg(percentText(stats.skippedMacroblockRegionCount,
                                                                                    stats.macroblockRegionCount)));

    QTreeWidgetItem *qp = addSection(tr("QP"));
    addMetric(qp, tr("Values"), QString::number(stats.qpValueCount));
    addMetric(qp, tr("Average"), stats.qpValueCount > 0 ? averageText(stats.averageQp) : tr("-"));
    addMetric(qp, tr("Range"), stats.qpValueCount > 0
              ? tr("%1 - %2").arg(stats.minQp).arg(stats.maxQp)
              : tr("-"));
    QTreeWidgetItem *qpDistribution = addSection(tr("QP distribution"));
    if (stats.qpBuckets.isEmpty()) {
        addMetric(qpDistribution, tr("-"), tr("-"));
    } else {
        for (const AnalysisQpBucket &bucket : stats.qpBuckets) {
            addDistributionMetric(qpDistribution,
                                  tr("%1-%2").arg(bucket.minQp).arg(bucket.maxQp),
                                  bucket.count,
                                  stats.qpValueCount);
        }
    }

    QTreeWidgetItem *motion = addSection(tr("Motion vectors"));
    addMetric(motion, tr("Vectors"), QString::number(stats.motionVectorCount));
    addMetric(motion, tr("Average magnitude"), stats.motionVectorCount > 0
              ? averageText(stats.averageMvMagnitudeQuarterPel)
              : tr("-"));
    addMetric(motion, tr("Max magnitude"), stats.motionVectorCount > 0
              ? QString::number(stats.maxMvMagnitudeQuarterPel)
              : tr("-"));

    QTreeWidgetItem *frameTypes = addSection(tr("Frame types"));
    if (stats.frameTypes.isEmpty()) {
        addMetric(frameTypes, tr("-"), tr("-"));
    } else {
        for (const AnalysisFrameTypeCount &type : stats.frameTypes) {
            addDistributionMetric(frameTypes, type.type, type.count, stats.frameCount);
        }
    }

    QTreeWidgetItem *diagnostics = addSection(tr("Diagnostics"));
    addMetric(diagnostics, tr("Access units with diagnostics"), QString::number(stats.diagnosticAccessUnits));
    addMetric(diagnostics, tr("Diagnostics"), QString::number(stats.diagnosticCount));
    QTreeWidgetItem *diagnosticDistribution = addSection(tr("Diagnostic distribution"));
    if (stats.diagnostics.isEmpty()) {
        addMetric(diagnosticDistribution, tr("-"), tr("-"));
    }
    for (const AnalysisDiagnosticSummary &summary : stats.diagnostics) {
        addDistributionMetric(diagnosticDistribution,
                              tr("%1 / %2").arg(summary.severity, summary.code),
                              summary.count,
                              stats.diagnosticCount);
    }

    m_tree->expandAll();
}

QTreeWidgetItem *StatsDock::addSection(const QString &name, const QString &value)
{
    auto *item = new QTreeWidgetItem({name, value});
    m_tree->addTopLevelItem(item);
    return item;
}

void StatsDock::addMetric(QTreeWidgetItem *parent, const QString &name, const QString &value)
{
    parent->addChild(new QTreeWidgetItem({name, value}));
}

void StatsDock::addDistributionMetric(QTreeWidgetItem *parent,
                                      const QString &name,
                                      int count,
                                      int total)
{
    parent->addChild(new QTreeWidgetItem({
        name,
        tr("%1 (%2) %3").arg(count).arg(percentText(count, total), distributionBar(count, total))
    }));
}
