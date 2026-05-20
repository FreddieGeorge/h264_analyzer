#include "ui/BitstreamHexView.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStringList>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
bool fieldCoversByte(const AnalysisBitField &field, qsizetype byteIndex)
{
    if (!field.packetBitRanges.isEmpty()) {
        for (const AnalysisBitRange &range : field.packetBitRanges) {
            if (range.offsetBasis != QStringLiteral("packet") || range.bitLength <= 0 || range.bitOffset < 0) {
                continue;
            }
            const qsizetype fieldFirstByte = range.bitOffset / 8;
            const qsizetype fieldLastByte = (range.bitOffset + range.bitLength - 1) / 8;
            if (byteIndex >= fieldFirstByte && byteIndex <= fieldLastByte) {
                return true;
            }
        }
        return false;
    }

    if (field.offsetBasis != QStringLiteral("packet") || field.bitLength <= 0 || field.bitOffset < 0) {
        return false;
    }

    const qsizetype fieldFirstByte = field.bitOffset / 8;
    const qsizetype fieldLastByte = (field.bitOffset + field.bitLength - 1) / 8;
    return byteIndex >= fieldFirstByte && byteIndex <= fieldLastByte;
}

QString byteBits(unsigned char value)
{
    QString bits;
    bits.reserve(8);
    for (int bit = 7; bit >= 0; --bit) {
        bits += (value & (1U << bit)) != 0 ? QLatin1Char('1') : QLatin1Char('0');
    }
    return bits;
}

QString markerBits(qsizetype byteIndex, qsizetype bitOffset, qsizetype bitLength)
{
    QString marker;
    marker.reserve(8);
    const qsizetype firstBit = bitOffset;
    const qsizetype lastBit = bitOffset + bitLength - 1;
    for (int displayBit = 7; displayBit >= 0; --displayBit) {
        const qsizetype absoluteBit = byteIndex * 8 + (7 - displayBit);
        marker += absoluteBit >= firstBit && absoluteBit <= lastBit ? QLatin1Char('^') : QLatin1Char('.');
    }
    return marker;
}

QString packetRangeText(qsizetype bitOffset, qsizetype bitLength)
{
    return QObject::tr("bits %1-%2, bytes %3-%4")
        .arg(bitOffset)
        .arg(bitOffset + bitLength - 1)
        .arg(bitOffset / 8)
        .arg((bitOffset + bitLength - 1) / 8);
}
}

BitstreamHexTextEdit::BitstreamHexTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void BitstreamHexTextEdit::setBytePositions(const QVector<int> &byteHexPositions, qsizetype pageStartByte)
{
    m_byteHexPositions = byteHexPositions;
    m_pageStartByte = pageStartByte;
}

void BitstreamHexTextEdit::mousePressEvent(QMouseEvent *event)
{
    QPlainTextEdit::mousePressEvent(event);

    const int position = cursorForPosition(event->pos()).position();
    for (int i = 0; i < m_byteHexPositions.size(); ++i) {
        const int bytePosition = m_byteHexPositions.at(i);
        if (position >= bytePosition && position <= bytePosition + 1) {
            emit byteActivated(m_pageStartByte + i, event->globalPosition().toPoint());
            return;
        }
    }
}

BitstreamHexView::BitstreamHexView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("BitstreamHexView"));

    m_summaryLabel = new QLabel(this);
    m_rangeLabel = new QLabel(this);
    m_bitsLabel = new QLabel(this);
    m_previousPageButton = new QPushButton(tr("Previous"), this);
    m_nextPageButton = new QPushButton(tr("Next"), this);
    m_textEdit = new BitstreamHexTextEdit(this);
    m_bitsLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_bitsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(m_summaryLabel, 1);
    topLayout->addWidget(m_previousPageButton);
    topLayout->addWidget(m_nextPageButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(topLayout);
    layout->addWidget(m_rangeLabel);
    layout->addWidget(m_bitsLabel);
    layout->addWidget(m_textEdit, 1);

    connect(m_previousPageButton, &QPushButton::clicked, this, [this]() {
        setPageIndex(m_pageIndex - 1);
    });
    connect(m_nextPageButton, &QPushButton::clicked, this, [this]() {
        setPageIndex(m_pageIndex + 1);
    });
    connect(m_textEdit, &BitstreamHexTextEdit::byteActivated,
            this, &BitstreamHexView::handleByteActivated);

    clearPacket(tr("Select an access unit to inspect packet bytes."));
}

void BitstreamHexView::showPacket(const FrameAnalysis &analysis)
{
    m_bytes = analysis.packet.bytes;
    m_bitFields = collectBitFields(analysis);
    m_pageIndex = 0;
    m_highlightBitOffset = -1;
    m_highlightBitLength = 0;
    m_highlightPacketRanges.clear();
    m_pageCount = m_bytes.isEmpty() ? 0 : static_cast<int>((m_bytes.size() + PageBytes - 1) / PageBytes);

    if (m_bytes.isEmpty()) {
        clearPacket(tr("No raw packet bytes are available for the selected access unit."));
        return;
    }

    renderPage();
}

void BitstreamHexView::clearPacket(const QString &message)
{
    m_bytes.clear();
    m_bitFields.clear();
    m_byteHexPositions.clear();
    m_pageIndex = 0;
    m_pageCount = 0;
    m_highlightBitOffset = -1;
    m_highlightBitLength = 0;
    m_highlightPacketRanges.clear();
    m_summaryLabel->setText(message);
    m_rangeLabel->setText(QString());
    m_bitsLabel->setText(QString());
    m_textEdit->setExtraSelections({});
    m_textEdit->setPlainText(message);
    m_textEdit->setBytePositions({}, 0);
    updatePageControls();
}

void BitstreamHexView::highlightBitRange(qsizetype bitOffset, qsizetype bitLength)
{
    m_highlightBitOffset = bitOffset;
    m_highlightBitLength = bitLength;
    m_highlightPacketRanges.clear();
    if (m_bytes.isEmpty() || bitOffset < 0 || bitLength <= 0) {
        m_textEdit->setExtraSelections({});
        m_rangeLabel->setText(tr("No bit field selected."));
        m_bitsLabel->setText(QString());
        return;
    }

    const qsizetype firstByte = bitOffset / 8;
    if (firstByte < 0 || firstByte >= m_bytes.size()) {
        m_textEdit->setExtraSelections({});
        m_rangeLabel->setText(tr("Selected bit range is outside the current packet bytes."));
        m_bitsLabel->setText(QString());
        return;
    }

    setPageIndex(static_cast<int>(firstByte / PageBytes));
}

void BitstreamHexView::highlightBitField(const AnalysisBitField &field)
{
    if (!field.packetBitRanges.isEmpty()) {
        m_highlightBitOffset = field.packetBitRanges.first().bitOffset;
        m_highlightBitLength = field.packetBitRanges.first().bitLength;
        m_highlightPacketRanges = field.packetBitRanges;
        setPageIndex(static_cast<int>((m_highlightBitOffset / 8) / PageBytes));
        return;
    }

    m_highlightPacketRanges.clear();
    if (field.offsetBasis != QStringLiteral("packet")) {
        m_textEdit->setExtraSelections({});
        m_highlightBitOffset = -1;
        m_highlightBitLength = 0;
        m_rangeLabel->setText(tr("Selected field uses %1-relative bit offsets; packet-byte highlighting is not normalized yet.")
                                  .arg(field.offsetBasis.isEmpty() ? tr("unknown") : field.offsetBasis));
        m_bitsLabel->setText(QString());
        return;
    }

    highlightBitRange(field.bitOffset, field.bitLength);
}

void BitstreamHexView::renderPage()
{
    m_byteHexPositions.clear();
    m_textEdit->setExtraSelections({});

    if (m_bytes.isEmpty()) {
        return;
    }

    const qsizetype pageStart = static_cast<qsizetype>(m_pageIndex) * PageBytes;
    const qsizetype pageEnd = std::min<qsizetype>(m_bytes.size(), pageStart + PageBytes);

    QString text;
    text.reserve(static_cast<int>(((pageEnd - pageStart) / 16 + 1) * 80));
    for (qsizetype rowOffset = pageStart; rowOffset < pageEnd; rowOffset += 16) {
        text += QStringLiteral("%1  ").arg(rowOffset, 8, 16, QLatin1Char('0')).toUpper();
        const int rowByteCount = std::min<int>(16, static_cast<int>(pageEnd - rowOffset));
        for (int column = 0; column < 16; ++column) {
            if (column < rowByteCount) {
                m_byteHexPositions.append(text.size());
                const unsigned char byte = static_cast<unsigned char>(m_bytes.at(rowOffset + column));
                text += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
            } else {
                text += QStringLiteral("   ");
            }
            if (column == 7) {
                text += QLatin1Char(' ');
            }
        }

        text += QStringLiteral(" |");
        for (int column = 0; column < rowByteCount; ++column) {
            const unsigned char byte = static_cast<unsigned char>(m_bytes.at(rowOffset + column));
            text += byte >= 0x20 && byte <= 0x7e ? QLatin1Char(byte) : QLatin1Char('.');
        }
        text += QStringLiteral("|\n");
    }

    m_textEdit->setPlainText(text);
    m_textEdit->setBytePositions(m_byteHexPositions, pageStart);

    const QString pageText = m_pageCount > 1
        ? tr("Packet bytes: %1, showing page %2/%3 (%4-%5)")
              .arg(m_bytes.size())
              .arg(m_pageIndex + 1)
              .arg(m_pageCount)
              .arg(pageStart)
              .arg(pageEnd - 1)
        : tr("Packet bytes: %1").arg(m_bytes.size());
    m_summaryLabel->setText(pageText);
    updatePageControls();

    if (m_highlightBitOffset >= 0 && m_highlightBitLength > 0) {
        const qsizetype firstByte = m_highlightBitOffset / 8;
        const qsizetype lastByte = (m_highlightBitOffset + m_highlightBitLength - 1) / 8;
        Q_UNUSED(firstByte);
        Q_UNUSED(lastByte);
        m_rangeLabel->setText(highlightedRangeText());
        updateBitPreview();

        QList<QTextEdit::ExtraSelection> selections;
        const auto appendSelection = [this, pageStart, pageEnd, &selections](qsizetype rangeBitOffset, qsizetype rangeBitLength) {
            const qsizetype rangeFirstByte = rangeBitOffset / 8;
            const qsizetype rangeLastByte = (rangeBitOffset + rangeBitLength - 1) / 8;
            const qsizetype firstVisibleByte = std::max<qsizetype>(rangeFirstByte, pageStart);
            const qsizetype lastVisibleByte = std::min<qsizetype>(rangeLastByte, pageEnd - 1);
            for (qsizetype byteIndex = firstVisibleByte; byteIndex <= lastVisibleByte; ++byteIndex) {
                const int localIndex = static_cast<int>(byteIndex - pageStart);
                if (localIndex < 0 || localIndex >= m_byteHexPositions.size()) {
                    continue;
                }
                QTextEdit::ExtraSelection selection;
                selection.format.setBackground(QColor(255, 232, 128));
                selection.format.setForeground(Qt::black);
                QTextCursor cursor(m_textEdit->document());
                cursor.setPosition(m_byteHexPositions.at(localIndex));
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                selection.cursor = cursor;
                selections.append(selection);
            }
        };
        if (m_highlightPacketRanges.isEmpty()) {
            appendSelection(m_highlightBitOffset, m_highlightBitLength);
        } else {
            for (const AnalysisBitRange &range : m_highlightPacketRanges) {
                appendSelection(range.bitOffset, range.bitLength);
            }
        }
        m_textEdit->setExtraSelections(selections);

        if (firstByte >= pageStart && firstByte < pageEnd) {
            QTextCursor cursor(m_textEdit->document());
            cursor.setPosition(m_byteHexPositions.at(static_cast<int>(firstByte - pageStart)));
            m_textEdit->setTextCursor(cursor);
            m_textEdit->centerCursor();
        }
    } else {
        m_rangeLabel->setText(tr("No bit field selected."));
        m_bitsLabel->setText(QString());
    }
}

void BitstreamHexView::setPageIndex(int pageIndex)
{
    if (m_pageCount <= 0) {
        return;
    }
    const int clampedPageIndex = std::clamp(pageIndex, 0, m_pageCount - 1);
    if (clampedPageIndex == m_pageIndex && !m_textEdit->toPlainText().isEmpty()) {
        renderPage();
        return;
    }
    m_pageIndex = clampedPageIndex;
    renderPage();
}

void BitstreamHexView::updatePageControls()
{
    const bool hasPages = m_pageCount > 1;
    m_previousPageButton->setEnabled(hasPages && m_pageIndex > 0);
    m_nextPageButton->setEnabled(hasPages && m_pageIndex + 1 < m_pageCount);
}

void BitstreamHexView::handleByteActivated(qsizetype byteIndex, const QPoint &globalPosition)
{
    const QVector<AnalysisBitField> fields = fieldsForByte(byteIndex);
    if (fields.isEmpty()) {
        m_highlightBitOffset = byteIndex * 8;
        m_highlightBitLength = 8;
        renderPage();
        m_rangeLabel->setText(tr("Byte %1 selected; no syntax field covers this byte.").arg(byteIndex));
        updateBitPreview();
        return;
    }

    if (fields.size() == 1) {
        activateBitField(fields.first());
        return;
    }

    QMenu menu(this);
    for (int i = 0; i < fields.size(); ++i) {
        const AnalysisBitField &field = fields.at(i);
        const QString label = field.path.isEmpty()
            ? tr("%1: bits %2-%3")
                  .arg(field.name)
                  .arg(field.bitOffset)
                  .arg(field.bitOffset + field.bitLength - 1)
            : tr("%1: bits %2-%3 (%4)")
                  .arg(field.name)
                  .arg(field.bitOffset)
                  .arg(field.bitOffset + field.bitLength - 1)
                  .arg(field.path);
        QAction *action = menu.addAction(label);
        action->setData(i);
    }

    QAction *selected = menu.exec(globalPosition);
    if (selected == nullptr) {
        return;
    }
    const int index = selected->data().toInt();
    if (index >= 0 && index < fields.size()) {
        activateBitField(fields.at(index));
    }
}

void BitstreamHexView::activateBitField(const AnalysisBitField &field)
{
    emit bitFieldActivated(field);
    highlightBitField(field);
}

void BitstreamHexView::updateBitPreview()
{
    if (m_bytes.isEmpty() || m_highlightBitOffset < 0 || m_highlightBitLength <= 0) {
        m_bitsLabel->setText(QString());
        return;
    }

    const qsizetype previewBitOffset = !m_highlightPacketRanges.isEmpty()
        ? m_highlightPacketRanges.first().bitOffset
        : m_highlightBitOffset;
    const qsizetype previewBitLength = !m_highlightPacketRanges.isEmpty()
        ? m_highlightPacketRanges.first().bitLength
        : m_highlightBitLength;

    const qsizetype firstByte = previewBitOffset / 8;
    const qsizetype lastByte = (previewBitOffset + previewBitLength - 1) / 8;
    if (firstByte < 0 || firstByte >= m_bytes.size()) {
        m_bitsLabel->setText(QString());
        return;
    }

    const qsizetype previewLastByte = std::min<qsizetype>(lastByte, std::min<qsizetype>(m_bytes.size() - 1, firstByte + 7));
    QString bitsLine = tr("bits  ");
    QString markLine = tr("mask  ");
    for (qsizetype byteIndex = firstByte; byteIndex <= previewLastByte; ++byteIndex) {
        if (byteIndex > firstByte) {
            bitsLine += QLatin1Char(' ');
            markLine += QLatin1Char(' ');
        }
        const unsigned char byte = static_cast<unsigned char>(m_bytes.at(byteIndex));
        bitsLine += byteBits(byte);
        markLine += markerBits(byteIndex, previewBitOffset, previewBitLength);
    }
    if (previewLastByte < lastByte) {
        bitsLine += QStringLiteral(" ...");
        markLine += QStringLiteral(" ...");
    }
    if (m_highlightPacketRanges.size() > 1) {
        bitsLine += tr("  (first range)");
    }
    m_bitsLabel->setText(bitsLine + QLatin1Char('\n') + markLine);
}

QString BitstreamHexView::highlightedRangeText() const
{
    if (!m_highlightPacketRanges.isEmpty()) {
        QStringList ranges;
        for (int i = 0; i < m_highlightPacketRanges.size(); ++i) {
            const AnalysisBitRange &range = m_highlightPacketRanges.at(i);
            ranges.append(packetRangeText(range.bitOffset, range.bitLength));
            if (ranges.size() == 4 && i + 1 < m_highlightPacketRanges.size()) {
                ranges.append(tr("... %1 more").arg(m_highlightPacketRanges.size() - i - 1));
                break;
            }
        }
        return tr("Selected field maps to %1 packet range(s): %2.")
            .arg(m_highlightPacketRanges.size())
            .arg(ranges.join(QStringLiteral("; ")));
    }

    return tr("Selected %1.").arg(packetRangeText(m_highlightBitOffset, m_highlightBitLength));
}

QVector<AnalysisBitField> BitstreamHexView::fieldsForByte(qsizetype byteIndex) const
{
    QVector<AnalysisBitField> matches;
    for (const AnalysisBitField &field : m_bitFields) {
        if (fieldCoversByte(field, byteIndex)) {
            matches.append(field);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const AnalysisBitField &left, const AnalysisBitField &right) {
        if (left.bitLength != right.bitLength) {
            return left.bitLength < right.bitLength;
        }
        return left.name < right.name;
    });
    return matches;
}

QVector<AnalysisBitField> BitstreamHexView::collectBitFields(const FrameAnalysis &analysis) const
{
    QVector<AnalysisBitField> fields = analysis.bitFields;
    for (const AnalysisParameterSet &parameterSet : analysis.parameterSets) {
        fields += parameterSet.bitFields;
    }
    QVector<AnalysisBitField> packetFields;
    for (const AnalysisBitField &field : fields) {
        if (field.offsetBasis == QStringLiteral("packet") || !field.packetBitRanges.isEmpty()) {
            packetFields.append(field);
        }
    }
    return packetFields;
}
