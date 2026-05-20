#pragma once

#include "core/decode/FFmpegDecoder.h"

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
    void setAnalysisOverlay(const FrameAnalysis &analysis);
    void setShowGrid(bool enabled);
    void setShowQpHeatmap(bool enabled);
    void setShowMotionVectors(bool enabled);
    void setOverlayOpacity(float opacity);

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
    QRectF analysisRegionWidgetRect(const AnalysisRegion &region, const QRectF &videoRect) const;
    QColor qpHeatColor(int qp) const;

    QString m_overlayMessage;
    DecodedVideoFramePtr m_currentFrame;
    FrameAnalysis m_currentAnalysis;
    bool m_showGrid = true;
    bool m_showQpHeatmap = false;
    bool m_showMotionVectors = false;
    float m_overlayOpacity = 1.0f;
    SwsContext *m_swsContext = nullptr;
    QByteArray m_rgbaBuffer;
    QSize m_textureSize;
    GLuint m_textureId = 0;
    QOpenGLShaderProgram *m_program = nullptr;
};
