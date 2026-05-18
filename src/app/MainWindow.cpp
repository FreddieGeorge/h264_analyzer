#include "app/MainWindow.h"

#include "core/DecodeWorker.h"
#include "ui/FrameListView.h"
#include "ui/LogDock.h"
#include "ui/PropertyTreeView.h"
#include "ui/VideoCanvas.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMetaType>
#include <QMimeData>
#include <QStandardPaths>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QToolBar>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace
{
constexpr int MaxCachedFrames = 80;

QString firstWritableLocation(QStandardPaths::StandardLocation location)
{
    const QStringList locations = QStandardPaths::standardLocations(location);
    for (const QString &entry : locations) {
        if (!entry.isEmpty()) {
            return entry;
        }
    }
    return QDir::homePath();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<StreamInfo>("StreamInfo");
    qRegisterMetaType<DecodedVideoFramePtr>("DecodedVideoFramePtr");
    qRegisterMetaType<FrameSyntaxInfo>("FrameSyntaxInfo");

    setWindowTitle(tr("H.264 Analyzer"));
    resize(1440, 900);
    setAcceptDrops(true);
    setDockOptions(QMainWindow::AllowTabbedDocks
                   | QMainWindow::AllowNestedDocks
                   | QMainWindow::AnimatedDocks);

    m_lastOpenDirectory = firstWritableLocation(QStandardPaths::DocumentsLocation);

    createActions();
    createDocks();
    createMenus();
    createToolBars();

    statusBar()->showMessage(tr("Ready"));
    m_logDock->appendLine(tr("[Info] Application started."));
}

MainWindow::~MainWindow()
{
    stopDecoder();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openStream);

    m_playPauseAction = new QAction(style()->standardIcon(QStyle::SP_MediaPause), tr("Pause"), this);
    m_playPauseAction->setShortcut(Qt::Key_Space);
    connect(m_playPauseAction, &QAction::triggered, this, &MainWindow::togglePlayback);

    m_previousFrameAction = new QAction(style()->standardIcon(QStyle::SP_MediaSkipBackward), tr("Previous Frame"), this);
    m_previousFrameAction->setShortcut(QKeySequence(Qt::Key_Left));
    connect(m_previousFrameAction, &QAction::triggered, this, &MainWindow::stepToPreviousFrame);

    m_nextFrameAction = new QAction(style()->standardIcon(QStyle::SP_MediaSkipForward), tr("Next Frame"), this);
    m_nextFrameAction->setShortcut(QKeySequence(Qt::Key_Right));
    connect(m_nextFrameAction, &QAction::triggered, this, &MainWindow::stepToNextFrame);

    m_stopAction = new QAction(style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"), this);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_showMotionVectorsAction = new QAction(tr("Show Motion Vectors"), this);
    m_showMotionVectorsAction->setCheckable(true);
    m_showMotionVectorsAction->setChecked(true);

    setPlaybackControlsEnabled(false);
}

void MainWindow::createDocks()
{
    m_videoCanvas = new VideoCanvas(this);
    setCentralWidget(m_videoCanvas);
    connect(m_showMotionVectorsAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowMotionVectors);
    m_videoCanvas->setShowMotionVectors(m_showMotionVectorsAction->isChecked());

    m_frameListView = new FrameListView;
    m_frameDock = new QDockWidget(tr("FrameListView"), this);
    m_frameDock->setObjectName(QStringLiteral("FrameListDock"));
    m_frameDock->setWidget(m_frameListView);
    addDockWidget(Qt::LeftDockWidgetArea, m_frameDock);

    m_propertyTreeView = new PropertyTreeView;
    connect(m_frameListView, &FrameListView::frameSelected,
            this, &MainWindow::handleFrameListSelection);
    m_propertyDock = new QDockWidget(tr("PropertyTreeView"), this);
    m_propertyDock->setObjectName(QStringLiteral("PropertyDock"));
    m_propertyDock->setWidget(m_propertyTreeView);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);

    m_logDock = new LogDock;
    m_logDockWidget = new QDockWidget(tr("LogDock"), this);
    m_logDockWidget->setObjectName(QStringLiteral("LogDockWidget"));
    m_logDockWidget->setWidget(m_logDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDockWidget);

    m_videoCanvas->setOverlayMessage(tr("Open an H.264 stream to begin."));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_docksMenu = viewMenu->addMenu(tr("&Docks"));
    m_docksMenu->addAction(m_frameDock->toggleViewAction());
    m_docksMenu->addAction(m_propertyDock->toggleViewAction());
    m_docksMenu->addAction(m_logDockWidget->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_showMotionVectorsAction);
}

void MainWindow::createToolBars()
{
    auto *fileToolBar = addToolBar(tr("File"));
    fileToolBar->setObjectName(QStringLiteral("FileToolBar"));
    fileToolBar->addAction(m_openAction);

    auto *playbackToolBar = addToolBar(tr("Playback"));
    playbackToolBar->setObjectName(QStringLiteral("PlaybackToolBar"));
    playbackToolBar->addAction(m_previousFrameAction);
    playbackToolBar->addAction(m_playPauseAction);
    playbackToolBar->addAction(m_nextFrameAction);
    playbackToolBar->addAction(m_stopAction);
    playbackToolBar->addSeparator();

    m_frameIndexLabel = new QLabel(tr("Frame - / -"), playbackToolBar);
    m_frameIndexLabel->setMinimumWidth(110);
    playbackToolBar->addWidget(m_frameIndexLabel);
}

void MainWindow::openStream()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open H.264 Stream"),
        defaultOpenDirectory(),
        tr("H.264 bitstream (*.264 *.h264 *.es *.bin);;All Files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    openStreamFile(filePath);
}

void MainWindow::openStreamFile(const QString &filePath)
{
    QString errorMessage;
    if (!m_document.openFile(filePath, &errorMessage)) {
        statusBar()->showMessage(errorMessage, 5000);
        m_logDock->appendLine(tr("[Error] %1").arg(errorMessage));
        return;
    }

    const StreamInfo &stream = m_document.streamInfo();
    m_lastOpenDirectory = stream.directory;

    statusBar()->showMessage(tr("Opening %1").arg(stream.fileName), 5000);
    m_logDock->appendLine(tr("[Info] Opened %1").arg(QDir::toNativeSeparators(stream.absoluteFilePath)));
    m_logDock->appendLine(tr("[Info] File size: %1 bytes").arg(stream.sizeBytes));

    m_videoCanvas->setOverlayMessage(
        tr("%1\n\nDecoding video stream...")
            .arg(stream.fileName));
    m_videoCanvas->setAnalysisOverlay(FrameSyntaxInfo {});
    m_frameListView->clearFrames();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_frameCache.clear();
    m_currentFrameIndex = -1;
    m_latestFrameIndex = -1;
    m_playbackPaused = false;
    updateFrameIndexDisplay();

    startDecoder(stream.absoluteFilePath);
}

void MainWindow::startDecoder(const QString &filePath)
{
    stopDecoder();

    m_decodeThread = new QThread(this);
    m_decodeWorker = new DecodeWorker;
    m_decodeWorker->moveToThread(m_decodeThread);

    connect(m_decodeThread, &QThread::started, m_decodeWorker, [worker = m_decodeWorker.data(), filePath]() {
        worker->decodeFile(filePath);
    });

    connect(m_decodeWorker, &DecodeWorker::streamOpened, this, [this](const StreamInfo &streamInfo) {
        statusBar()->showMessage(
            tr("Decoding %1 (%2x%3, %4 fps)")
                .arg(streamInfo.fileName)
                .arg(streamInfo.width)
                .arg(streamInfo.height)
                .arg(streamInfo.frameRate, 0, 'f', 3),
            5000);
        m_propertyTreeView->showPlaceholder(
            tr("Codec: %1, %2x%3, %4 fps, %5")
                .arg(streamInfo.codecName)
                .arg(streamInfo.width)
                .arg(streamInfo.height)
                .arg(streamInfo.frameRate, 0, 'f', 3)
                .arg(streamInfo.pixelFormatName));
    });

    connect(m_decodeWorker, &DecodeWorker::frameReady,
            this, &MainWindow::handleFrameReady,
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::logMessage,
            m_logDock, &LogDock::appendLine,
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Error] %1").arg(message));
    });
    connect(m_decodeWorker, &DecodeWorker::finished, m_decodeThread, &QThread::quit);
    connect(m_decodeWorker, &DecodeWorker::finished, m_decodeWorker, &QObject::deleteLater);
    connect(m_decodeThread, &QThread::finished, m_decodeThread, &QObject::deleteLater);
    connect(m_decodeThread, &QThread::finished, this, [this]() {
        m_decodeThread = nullptr;
        m_decodeWorker = nullptr;
        m_playbackPaused = false;
        updatePlaybackActionState();
        setPlaybackControlsEnabled(false);
        m_logDock->appendLine(tr("[Info] Decode thread stopped."));
    });

    m_playbackPaused = false;
    setPlaybackControlsEnabled(true);
    updatePlaybackActionState();
    m_decodeThread->start();
}

void MainWindow::stopDecoder()
{
    if (!m_decodeWorker.isNull()) {
        m_decodeWorker->stop();
    }

    if (!m_decodeThread.isNull()) {
        m_decodeThread->quit();
        m_decodeThread->wait();
        m_decodeThread = nullptr;
        m_decodeWorker = nullptr;
    }
}

void MainWindow::togglePlayback()
{
    if (m_decodeWorker.isNull()) {
        return;
    }

    if (m_playbackPaused) {
        resumePlayback();
    } else {
        pausePlayback();
    }
}

void MainWindow::pausePlayback()
{
    if (m_decodeWorker.isNull()) {
        return;
    }

    m_decodeWorker->pause();
    m_playbackPaused = true;
    updatePlaybackActionState();
    showFrameFromCache(m_currentFrameIndex, true, true);
    statusBar()->showMessage(tr("Paused"), 2000);
}

void MainWindow::resumePlayback()
{
    if (m_decodeWorker.isNull()) {
        return;
    }

    m_decodeWorker->play();
    m_playbackPaused = false;
    updatePlaybackActionState();
    statusBar()->showMessage(tr("Playing"), 2000);
}

void MainWindow::stepToPreviousFrame()
{
    if (m_currentFrameIndex <= 0) {
        return;
    }

    if (!m_decodeWorker.isNull()) {
        pausePlayback();
    }
    showFrameFromCache(m_currentFrameIndex - 1, true, true);
}

void MainWindow::stepToNextFrame()
{
    if (!m_decodeWorker.isNull()) {
        pausePlayback();
    }

    const int nextIndex = m_currentFrameIndex + 1;
    if (nextIndex <= m_latestFrameIndex && showFrameFromCache(nextIndex, true, true)) {
        return;
    }

    if (!m_decodeWorker.isNull()) {
        m_decodeWorker->stepForward();
        m_playbackPaused = true;
        updatePlaybackActionState();
    }
}

void MainWindow::stopPlayback()
{
    stopDecoder();
    m_playbackPaused = false;
    updatePlaybackActionState();
    setPlaybackControlsEnabled(false);
    statusBar()->showMessage(tr("Stopped"), 2000);
}

void MainWindow::handleFrameReady(int frameIndex,
                                  const DecodedVideoFramePtr &frame,
                                  const FrameSyntaxInfo &syntaxInfo)
{
    CachedFrame cached;
    cached.index = frameIndex;
    cached.frame = frame;
    cached.syntaxInfo = syntaxInfo;
    m_frameCache.append(cached);
    while (m_frameCache.size() > MaxCachedFrames) {
        m_frameCache.removeFirst();
    }

    m_latestFrameIndex = std::max(m_latestFrameIndex, frameIndex);
    m_frameListView->addFrameSyntax(syntaxInfo);
    showFrameFromCache(frameIndex, true, m_playbackPaused);
}

void MainWindow::handleFrameListSelection(int frameIndex)
{
    if (!m_decodeWorker.isNull()) {
        pausePlayback();
    }
    showFrameFromCache(frameIndex, false, true);
}

bool MainWindow::showFrameFromCache(int frameIndex, bool selectInList, bool updatePropertyTree)
{
    if (frameIndex < 0) {
        return false;
    }

    for (const CachedFrame &cached : std::as_const(m_frameCache)) {
        if (cached.index != frameIndex) {
            continue;
        }

        m_currentFrameIndex = frameIndex;
        m_videoCanvas->setFrame(cached.frame);
        m_videoCanvas->setAnalysisOverlay(cached.syntaxInfo);
        if (updatePropertyTree) {
            m_propertyTreeView->showFrameSyntax(cached.syntaxInfo);
        }
        if (selectInList) {
            m_frameListView->selectFrameIndex(frameIndex);
        }
        updateFrameIndexDisplay();
        return true;
    }

    m_logDock->appendLine(tr("[Warning] Frame %1 is no longer in the recent frame cache.").arg(frameIndex));
    return false;
}

void MainWindow::setPlaybackControlsEnabled(bool enabled)
{
    if (m_playPauseAction != nullptr) {
        m_playPauseAction->setEnabled(enabled);
    }
    if (m_previousFrameAction != nullptr) {
        m_previousFrameAction->setEnabled(enabled);
    }
    if (m_nextFrameAction != nullptr) {
        m_nextFrameAction->setEnabled(enabled);
    }
    if (m_stopAction != nullptr) {
        m_stopAction->setEnabled(enabled);
    }
}

void MainWindow::updatePlaybackActionState()
{
    if (m_playPauseAction == nullptr) {
        return;
    }

    if (m_playbackPaused) {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseAction->setText(tr("Play"));
    } else {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        m_playPauseAction->setText(tr("Pause"));
    }
}

void MainWindow::updateFrameIndexDisplay()
{
    if (m_frameIndexLabel == nullptr) {
        return;
    }

    if (m_currentFrameIndex < 0) {
        m_frameIndexLabel->setText(tr("Frame - / -"));
        return;
    }

    m_frameIndexLabel->setText(tr("Frame %1 / %2")
                                   .arg(m_currentFrameIndex + 1)
                                   .arg(m_latestFrameIndex + 1));
}

QString MainWindow::defaultOpenDirectory() const
{
    if (!m_lastOpenDirectory.isEmpty()) {
        return m_lastOpenDirectory;
    }
    return firstWritableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        openStreamFile(url.toLocalFile());
        event->acceptProposedAction();
        return;
    }

    QMainWindow::dropEvent(event);
}
