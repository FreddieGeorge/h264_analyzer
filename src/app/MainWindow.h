#pragma once

#include "core/FFmpegDecoder.h"
#include "core/StreamDocument.h"

#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QVector>

class QAction;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QMenu;
class QThread;

class DecodeWorker;
class FrameListView;
class LogDock;
class PropertyTreeView;
class VideoCanvas;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDocks();
    void openStream();
    void openStreamFile(const QString &filePath);
    void startDecoder(const QString &filePath);
    void stopDecoder();
    void togglePlayback();
    void pausePlayback();
    void resumePlayback();
    void stepToPreviousFrame();
    void stepToNextFrame();
    void stopPlayback();
    void handleFrameReady(int frameIndex, const DecodedVideoFramePtr &frame, const FrameSyntaxInfo &syntaxInfo);
    void handleFrameListSelection(int frameIndex);
    bool showFrameFromCache(int frameIndex, bool selectInList = true, bool updatePropertyTree = true);
    void setPlaybackControlsEnabled(bool enabled);
    void updatePlaybackActionState();
    void updateFrameIndexDisplay();
    QString defaultOpenDirectory() const;

    struct CachedFrame
    {
        int index = -1;
        DecodedVideoFramePtr frame;
        FrameSyntaxInfo syntaxInfo;
    };

    QAction *m_openAction = nullptr;
    QAction *m_playPauseAction = nullptr;
    QAction *m_previousFrameAction = nullptr;
    QAction *m_nextFrameAction = nullptr;
    QAction *m_stopAction = nullptr;
    QLabel *m_frameIndexLabel = nullptr;
    QMenu *m_docksMenu = nullptr;

    QDockWidget *m_frameDock = nullptr;
    QDockWidget *m_propertyDock = nullptr;
    QDockWidget *m_logDockWidget = nullptr;

    FrameListView *m_frameListView = nullptr;
    PropertyTreeView *m_propertyTreeView = nullptr;
    LogDock *m_logDock = nullptr;
    VideoCanvas *m_videoCanvas = nullptr;

    StreamDocument m_document;
    QPointer<QThread> m_decodeThread;
    QPointer<DecodeWorker> m_decodeWorker;
    QString m_lastOpenDirectory;
    QVector<CachedFrame> m_frameCache;
    int m_currentFrameIndex = -1;
    int m_latestFrameIndex = -1;
    bool m_playbackPaused = false;
};
