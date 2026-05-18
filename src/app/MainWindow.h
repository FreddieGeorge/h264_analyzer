#pragma once

#include "core/StreamDocument.h"

#include <QMainWindow>
#include <QPointer>
#include <QString>

class QAction;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
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
    QString defaultOpenDirectory() const;

    QAction *m_openAction = nullptr;
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
};
