#pragma once

#include "core/model/FrameAnalysis.h"

#include <QByteArray>
#include <QPlainTextEdit>
#include <QPoint>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

class BitstreamHexTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit BitstreamHexTextEdit(QWidget *parent = nullptr);

    void setBytePositions(const QVector<int> &byteHexPositions, qsizetype pageStartByte);

signals:
    void byteActivated(qsizetype byteIndex, const QPoint &globalPosition);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QVector<int> m_byteHexPositions;
    qsizetype m_pageStartByte = 0;
};

class BitstreamHexView : public QWidget
{
    Q_OBJECT

public:
    explicit BitstreamHexView(QWidget *parent = nullptr);

    void showPacket(const FrameAnalysis &analysis);
    void clearPacket(const QString &message);
    void highlightBitRange(qsizetype bitOffset, qsizetype bitLength);
    void highlightBitField(const AnalysisBitField &field);

signals:
    void bitFieldActivated(const AnalysisBitField &field);

private:
    void renderPage();
    void setPageIndex(int pageIndex);
    void updatePageControls();
    void handleByteActivated(qsizetype byteIndex, const QPoint &globalPosition);
    void activateBitField(const AnalysisBitField &field);
    void updateBitPreview();
    QString highlightedRangeText() const;
    QVector<AnalysisBitField> fieldsForByte(qsizetype byteIndex) const;
    QVector<AnalysisBitField> collectBitFields(const FrameAnalysis &analysis) const;

    static constexpr int PageBytes = 4096;

    QByteArray m_bytes;
    QVector<AnalysisBitField> m_bitFields;
    QVector<int> m_byteHexPositions;
    int m_pageIndex = 0;
    int m_pageCount = 0;
    qsizetype m_highlightBitOffset = -1;
    qsizetype m_highlightBitLength = 0;
    QVector<AnalysisBitRange> m_highlightPacketRanges;

    QLabel *m_summaryLabel = nullptr;
    QLabel *m_rangeLabel = nullptr;
    QLabel *m_bitsLabel = nullptr;
    QPushButton *m_previousPageButton = nullptr;
    QPushButton *m_nextPageButton = nullptr;
    BitstreamHexTextEdit *m_textEdit = nullptr;
};
