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

namespace
{
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
}

void MainWindow::createDocks()
{
    m_videoCanvas = new VideoCanvas(this);
    setCentralWidget(m_videoCanvas);

    m_frameListView = new FrameListView;
    m_frameDock = new QDockWidget(tr("FrameListView"), this);
    m_frameDock->setObjectName(QStringLiteral("FrameListDock"));
    m_frameDock->setWidget(m_frameListView);
    addDockWidget(Qt::LeftDockWidgetArea, m_frameDock);

    m_propertyTreeView = new PropertyTreeView;
    connect(m_frameListView, &FrameListView::frameSyntaxSelected,
            m_propertyTreeView, &PropertyTreeView::showFrameSyntax);
    connect(m_frameListView, &FrameListView::frameSyntaxSelected,
            m_videoCanvas, &VideoCanvas::setAnalysisOverlay);
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
}

void MainWindow::createToolBars()
{
    auto *fileToolBar = addToolBar(tr("File"));
    fileToolBar->setObjectName(QStringLiteral("FileToolBar"));
    fileToolBar->addAction(m_openAction);
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

    connect(m_decodeWorker, &DecodeWorker::frameDecoded,
            m_videoCanvas, &VideoCanvas::setFrame,
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::frameSyntaxDecoded,
            m_frameListView, &FrameListView::addFrameSyntax,
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::frameSyntaxDecoded,
            m_videoCanvas, &VideoCanvas::setAnalysisOverlay,
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
        m_logDock->appendLine(tr("[Info] Decode thread stopped."));
    });

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
