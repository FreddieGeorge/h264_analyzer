#pragma once

#include "core/analysis/AnalysisStats.h"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

class StatsDock : public QWidget
{
    Q_OBJECT

public:
    explicit StatsDock(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void setStats(const AnalysisStats &stats);

private:
    QTreeWidgetItem *addSection(const QString &name, const QString &value = QString());
    void addMetric(QTreeWidgetItem *parent, const QString &name, const QString &value);
    void addDistributionMetric(QTreeWidgetItem *parent,
                               const QString &name,
                               int count,
                               int total);

    QTreeWidget *m_tree = nullptr;
};
