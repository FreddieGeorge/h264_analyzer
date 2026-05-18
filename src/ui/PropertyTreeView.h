#pragma once

#include "core/H264Parser.h"

#include <QTreeWidget>

class PropertyTreeView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PropertyTreeView(QWidget *parent = nullptr);

    void showPlaceholder(const QString &message);
    void showFrameSyntax(const FrameSyntaxInfo &syntaxInfo);

private:
    QTreeWidgetItem *addPair(QTreeWidgetItem *parent, const QString &field, const QString &value);
    void addSyntaxFields(QTreeWidgetItem *parent, const QVector<SyntaxFieldInfo> &fields);
};
