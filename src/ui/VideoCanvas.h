#pragma once

#include "core/FFmpegDecoder.h"
#include "core/H264Parser.h"

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

class QColor;
class QPainter;
struct SwsContext;

class VideoCanvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit VideoCanvas(QWidget *parent = nullptr);
    ~VideoCanvas() override;

    void setOverlayMessage(const QString &message);

public slots:
    void setFrame(const DecodedVideoFramePtr &frame);
    void setAnalysisOverlay(const FrameSyntaxInfo &syntaxInfo);
    void setShowMotionVectors(bool enabled);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

private:
    void ensureTexture();
    bool uploadCurrentFrame();
    void paintTexture();
    void drawAnalysisOverlay();
    void drawQpHeatmap(QPainter &painter, const QRectF &videoRect);
    void drawMacroblockGrid(QPainter &painter, const QRectF &videoRect);
    void drawMotionVectors(QPainter &painter, const QRectF &videoRect);
    void paintOverlayText();
    QRectF videoDisplayRect() const;
    QPointF mapVideoPointToWidget(const QPointF &videoPoint, const QRectF &videoRect) const;
    QRectF macroblockWidgetRect(int macroblockAddress, int picWidthInMbs, const QRectF &videoRect) const;
    QColor qpHeatColor(int qp) const;

    QString m_overlayMessage;
    DecodedVideoFramePtr m_currentFrame;
    FrameSyntaxInfo m_currentSyntaxInfo;
    bool m_showMotionVectors = true;
    SwsContext *m_swsContext = nullptr;
    QByteArray m_rgbaBuffer;
    QSize m_textureSize;
    GLuint m_textureId = 0;
    QOpenGLShaderProgram *m_program = nullptr;
};
