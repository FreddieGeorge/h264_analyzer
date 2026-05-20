#include "app/MainWindow.h"

#include "core/AnalysisExportWriter.h"
#include "core/DecodeWorker.h"
#include "ui/BitstreamHexView.h"
#include "ui/FrameListView.h"
#include "ui/LogDock.h"
#include "ui/PropertyTreeView.h"
#include "ui/VideoCanvas.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMetaType>
#include <QMimeData>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QSignalBlocker>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QToolBar>
#include <QSlider>
#include <QTextStream>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QUrlQuery>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
constexpr int MaxCachedFrames = 80;

struct ReleaseAsset
{
    QString name;
    QString downloadUrl;
};

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

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QVector<int> parseVersionParts(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }

    const int suffixIndex = version.indexOf(QRegularExpression(QStringLiteral("[+-]")));
    if (suffixIndex >= 0) {
        version.truncate(suffixIndex);
    }

    QVector<int> parts;
    const QStringList tokens = version.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("^(\\d+)")).match(token);
        parts.append(match.hasMatch() ? match.captured(1).toInt() : 0);
    }
    return parts;
}

int compareVersions(const QString &left, const QString &right)
{
    const QVector<int> leftParts = parseVersionParts(left);
    const QVector<int> rightParts = parseVersionParts(right);
    const int partCount = std::max(leftParts.size(), rightParts.size());
    for (int i = 0; i < partCount; ++i) {
        const int leftValue = i < leftParts.size() ? leftParts.at(i) : 0;
        const int rightValue = i < rightParts.size() ? rightParts.at(i) : 0;
        if (leftValue != rightValue) {
            return leftValue < rightValue ? -1 : 1;
        }
    }
    return 0;
}

QString compactReleaseNotes(QString body)
{
    body = body.trimmed();
    if (body.size() > 1200) {
        body = body.left(1200).trimmed() + QStringLiteral("...");
    }
    return body;
}

ReleaseAsset releaseAsset(const QJsonObject &release, const QRegularExpression &namePattern)
{
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetValue : assets) {
        if (!assetValue.isObject()) {
            continue;
        }

        const QJsonObject asset = assetValue.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!namePattern.match(name).hasMatch()) {
            continue;
        }

        const QString downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
        if (!downloadUrl.isEmpty()) {
            return ReleaseAsset { name, downloadUrl };
        }
    }

    return ReleaseAsset {};
}

QString sha256FromChecksumText(const QString &checksumText)
{
    const QRegularExpression hashPattern(QStringLiteral("\\b([A-Fa-f0-9]{64})\\b"));
    const QRegularExpressionMatch match = hashPattern.match(checksumText);
    if (!match.hasMatch()) {
        return QString {};
    }
    return match.captured(1).toLower();
}

QString sha256ForFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return QString {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to read file for SHA256 hashing.");
        }
        return QString {};
    }

    return QString::fromLatin1(hash.result().toHex());
}

int frameAnalysisQpValueCount(const FrameAnalysis &analysis, int *minQp = nullptr, int *maxQp = nullptr)
{
    int count = 0;
    int localMin = 0;
    int localMax = 0;
    for (const AnalysisRegion &region : analysis.regions) {
        if (region.kind != AnalysisRegionKind::Macroblock || region.qp < 0) {
            continue;
        }
        if (count == 0) {
            localMin = region.qp;
            localMax = region.qp;
        } else {
            localMin = std::min(localMin, region.qp);
            localMax = std::max(localMax, region.qp);
        }
        ++count;
    }

    if (minQp != nullptr) {
        *minQp = localMin;
    }
    if (maxQp != nullptr) {
        *maxQp = localMax;
    }
    return count;
}

bool frameAnalysisHasDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

QString motionVectorStatusText(const FrameAnalysis &analysis)
{
    if (analysis.codecKind != CodecKind::H264) {
        return QObject::tr("MV unsupported for this codec.");
    }

    if (!analysis.motionVectors.isEmpty()) {
        return QObject::tr("Motion vectors: %1 parsed").arg(analysis.motionVectors.size());
    }

    if (analysis.frameType == QStringLiteral("I")) {
        return QObject::tr("MV: none expected for I-frame.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("cabac_unsupported"))) {
        return QObject::tr("MV: frame uses CABAC; CABAC parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b_direct_macroblock_unsupported"))) {
        return QObject::tr("MV: B_Direct parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b8x8_sub_macroblock_unsupported"))) {
        return QObject::tr("MV: B_8x8 parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b_slice_macroblock_unsupported"))) {
        return QObject::tr("MV: unsupported B-slice macroblock type.");
    }

    if (analysis.frameType == QStringLiteral("B")) {
        return QObject::tr("MV: no supported B-slice vectors parsed.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_unsupported"))
        || frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_type_unsupported"))) {
        return QObject::tr("MV: unsupported P_8x8 sub-macroblock type.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("interlaced_or_fmo_unsupported"))) {
        return QObject::tr("MV: MBAFF/FMO parsing not implemented.");
    }

    return QObject::tr("MV: no supported vectors parsed for this frame.");
}

QString streamSelectorText(const MediaStreamInfo &stream)
{
    const QString codec = stream.codecName.isEmpty()
        ? codecKindName(stream.codecKind)
        : stream.codecName;
    QString detail;
    if (stream.mediaKind == MediaKind::Video) {
        detail = QObject::tr("%1x%2").arg(stream.width).arg(stream.height);
    } else if (stream.mediaKind == MediaKind::Audio) {
        detail = QObject::tr("%1 Hz, %2 ch").arg(stream.sampleRate).arg(stream.channels);
    }

    return detail.isEmpty()
        ? QObject::tr("Stream #%1 %2 %3")
              .arg(stream.streamIndex)
              .arg(mediaKindName(stream.mediaKind))
              .arg(codec)
        : QObject::tr("Stream #%1 %2 %3 (%4)")
              .arg(stream.streamIndex)
              .arg(mediaKindName(stream.mediaKind))
              .arg(codec)
              .arg(detail);
}

QString safeInstallerFileName(QString fileName)
{
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("ZStreamEye-update-setup.exe");
    }
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    if (!fileName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        fileName.append(QStringLiteral(".exe"));
    }
    return fileName;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<StreamInfo>("StreamInfo");
    qRegisterMetaType<DecodedVideoFramePtr>("DecodedVideoFramePtr");
    qRegisterMetaType<FrameSyntaxInfo>("FrameSyntaxInfo");
    qRegisterMetaType<FrameAnalysis>("FrameAnalysis");
    qRegisterMetaType<PacketRawData>("PacketRawData");
    qRegisterMetaType<FrameSeekCheckpoint>("FrameSeekCheckpoint");

    setWindowTitle(tr("ZStreamEye"));
    resize(1440, 900);
    setAcceptDrops(true);
    setDockOptions(QMainWindow::AllowTabbedDocks
                   | QMainWindow::AllowNestedDocks
                   | QMainWindow::AnimatedDocks);

    m_lastOpenDirectory = firstWritableLocation(QStandardPaths::DocumentsLocation);
    m_lastExportDirectory = m_lastOpenDirectory;

    createActions();
    createDocks();
    createMenus();
    createToolBars();
    loadSettings();
    updateExportActionState();

    statusBar()->showMessage(tr("Ready"));
    m_logDock->appendLine(tr("[Info] Application started."));
}

MainWindow::~MainWindow()
{
    saveSettings();
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

    m_exportFrameSyntaxJsonAction = new QAction(tr("Export Selected Access Unit JSON"), this);
    connect(m_exportFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportFrameSyntaxJson);

    m_exportAllFrameSyntaxJsonAction = new QAction(tr("Export All Decoded Access Units JSON"), this);
    connect(m_exportAllFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportAllFrameSyntaxJson);

    m_exportFrameListCsvAction = new QAction(tr("Export Access Unit List CSV"), this);
    connect(m_exportFrameListCsvAction, &QAction::triggered, this, &MainWindow::exportFrameListCsv);

    m_exportScreenshotAction = new QAction(tr("Export Screenshot"), this);
    connect(m_exportScreenshotAction, &QAction::triggered, this, &MainWindow::exportScreenshot);

    m_checkForUpdatesAction = new QAction(tr("Check for Updates"), this);
    connect(m_checkForUpdatesAction, &QAction::triggered, this, &MainWindow::checkForUpdates);

    m_showGridAction = new QAction(tr("Show Macroblock Grid"), this);
    m_showGridAction->setCheckable(true);
    m_showGridAction->setChecked(true);

    m_showQpHeatmapAction = new QAction(tr("Show QP Heatmap"), this);
    m_showQpHeatmapAction->setCheckable(true);
    m_showQpHeatmapAction->setChecked(false);

    m_showMotionVectorsAction = new QAction(tr("Show Motion Vectors"), this);
    m_showMotionVectorsAction->setCheckable(true);
    m_showMotionVectorsAction->setChecked(false);

    setPlaybackControlsEnabled(false);
    updatePlaybackActionState();
    updateExportActionState();
}

void MainWindow::createDocks()
{
    m_videoCanvas = new VideoCanvas(this);
    setCentralWidget(m_videoCanvas);
    connect(m_showGridAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowGrid);
    connect(m_showQpHeatmapAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowQpHeatmap);
    connect(m_showMotionVectorsAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowMotionVectors);
    connect(m_showQpHeatmapAction, &QAction::toggled,
            this, &MainWindow::updateCurrentOverlayStatusHint);
    connect(m_showMotionVectorsAction, &QAction::toggled,
            this, &MainWindow::updateCurrentOverlayStatusHint);
    m_videoCanvas->setShowGrid(m_showGridAction->isChecked());
    m_videoCanvas->setShowQpHeatmap(m_showQpHeatmapAction->isChecked());
    m_videoCanvas->setShowMotionVectors(m_showMotionVectorsAction->isChecked());

    m_frameListView = new FrameListView;
    m_frameDock = new QDockWidget(tr("AccessUnitListView"), this);
    m_frameDock->setObjectName(QStringLiteral("FrameListDock"));
    m_frameDock->setWidget(m_frameListView);
    addDockWidget(Qt::LeftDockWidgetArea, m_frameDock);

    m_propertyTreeView = new PropertyTreeView;
    connect(m_frameListView, &FrameListView::frameSelected,
            this, &MainWindow::handleFrameListSelection);
    connect(m_frameListView, &FrameListView::accessUnitSelected,
            this, [this](const FrameAnalysis &analysis) {
                if (analysis.accessUnitKind == AccessUnitKind::VideoFrame) {
                    return;
                }
                m_currentAnalysis = analysis;
                m_hasCurrentAnalysis = true;
                m_propertyTreeView->showFrameAnalysis(analysis);
                if (m_hexView != nullptr) {
                    m_hexView->showPacket(analysis);
                }
                updateExportActionState();
                statusBar()->showMessage(
                    tr("Selected %1 access unit %2 on stream %3")
                        .arg(mediaKindName(analysis.mediaKind))
                        .arg(analysis.frameIndex)
                        .arg(analysis.streamIndex),
                    2000);
            });
    m_propertyDock = new QDockWidget(tr("PropertyTreeView"), this);
    m_propertyDock->setObjectName(QStringLiteral("PropertyDock"));
    m_propertyDock->setWidget(m_propertyTreeView);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);

    m_hexView = new BitstreamHexView;
    m_hexDock = new QDockWidget(tr("Bitstream Hex"), this);
    m_hexDock->setObjectName(QStringLiteral("BitstreamHexDock"));
    m_hexDock->setWidget(m_hexView);
    addDockWidget(Qt::BottomDockWidgetArea, m_hexDock);
    connect(m_propertyTreeView, &PropertyTreeView::bitFieldSelected,
            m_hexView, [this](const AnalysisBitField &field) {
                if (m_hexView != nullptr) {
                    m_hexView->highlightBitField(field);
                }
            });
    connect(m_hexView, &BitstreamHexView::bitFieldActivated,
            m_propertyTreeView, &PropertyTreeView::selectBitField);

    m_logDock = new LogDock;
    m_logDockWidget = new QDockWidget(tr("LogDock"), this);
    m_logDockWidget->setObjectName(QStringLiteral("LogDockWidget"));
    m_logDockWidget->setWidget(m_logDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDockWidget);

    m_videoCanvas->setOverlayMessage(tr("Open a video or H.264 stream to begin."));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportAllFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportFrameListCsvAction);
    fileMenu->addAction(m_exportScreenshotAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_docksMenu = viewMenu->addMenu(tr("&Docks"));
    m_docksMenu->addAction(m_frameDock->toggleViewAction());
    m_docksMenu->addAction(m_hexDock->toggleViewAction());
    m_docksMenu->addAction(m_propertyDock->toggleViewAction());
    m_docksMenu->addAction(m_logDockWidget->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_showGridAction);
    viewMenu->addAction(m_showQpHeatmapAction);
    viewMenu->addAction(m_showMotionVectorsAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_checkForUpdatesAction);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About ZStreamEye"), this, &MainWindow::showAboutDialog);
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

    auto *overlayToolBar = addToolBar(tr("Overlays"));
    overlayToolBar->setObjectName(QStringLiteral("OverlayToolBar"));
    overlayToolBar->addAction(m_showGridAction);
    overlayToolBar->addAction(m_showQpHeatmapAction);
    overlayToolBar->addAction(m_showMotionVectorsAction);
    overlayToolBar->addSeparator();
    overlayToolBar->addWidget(new QLabel(tr("Opacity"), overlayToolBar));

    m_overlayOpacitySlider = new QSlider(Qt::Horizontal, overlayToolBar);
    m_overlayOpacitySlider->setRange(0, 100);
    m_overlayOpacitySlider->setValue(100);
    m_overlayOpacitySlider->setFixedWidth(120);
    overlayToolBar->addWidget(m_overlayOpacitySlider);
    connect(m_overlayOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_videoCanvas != nullptr) {
            m_videoCanvas->setOverlayOpacity(value / 100.0f);
        }
    });

    auto *accessUnitToolBar = addToolBar(tr("Access Units"));
    accessUnitToolBar->setObjectName(QStringLiteral("AccessUnitToolBar"));
    accessUnitToolBar->addWidget(new QLabel(tr("Stream"), accessUnitToolBar));
    m_streamSelector = new QComboBox(accessUnitToolBar);
    m_streamSelector->setMinimumWidth(220);
    accessUnitToolBar->addWidget(m_streamSelector);
    accessUnitToolBar->addSeparator();
    accessUnitToolBar->addWidget(new QLabel(tr("Show"), accessUnitToolBar));
    m_accessUnitFilterSelector = new QComboBox(accessUnitToolBar);
    m_accessUnitFilterSelector->addItem(tr("All access units"), static_cast<int>(FrameListView::AccessUnitFilter::All));
    m_accessUnitFilterSelector->addItem(tr("Video only"), static_cast<int>(FrameListView::AccessUnitFilter::Video));
    m_accessUnitFilterSelector->addItem(tr("Audio only"), static_cast<int>(FrameListView::AccessUnitFilter::Audio));
    m_accessUnitFilterSelector->addItem(tr("Diagnostics only"), static_cast<int>(FrameListView::AccessUnitFilter::DiagnosticsOnly));
    accessUnitToolBar->addWidget(m_accessUnitFilterSelector);

    connect(m_streamSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_frameListView == nullptr || m_streamSelector == nullptr || index < 0) {
            return;
        }
        m_frameListView->setStreamFilter(m_streamSelector->itemData(index).toInt());
    });
    connect(m_accessUnitFilterSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_frameListView == nullptr || m_accessUnitFilterSelector == nullptr || index < 0) {
            return;
        }
        m_frameListView->setAccessUnitFilter(
            static_cast<FrameListView::AccessUnitFilter>(m_accessUnitFilterSelector->itemData(index).toInt()));
    });
    resetAccessUnitFilters();
}

void MainWindow::openStream()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Video or Bitstream"),
        defaultOpenDirectory(),
        tr("Video and bitstream files (*.264 *.h264 *.avc *.h265 *.hevc *.mp4 *.m4v *.mov *.mkv *.avi *.ts *.m2ts *.mts *.webm *.flv *.wmv *.mpg *.mpeg *.3gp *.mp5 *.es *.bin);;"
           "H.264 bitstream (*.264 *.h264 *.avc *.es *.bin);;"
           "MP4 / MOV (*.mp4 *.m4v *.mov);;"
           "All Files (*)"));

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
    m_videoCanvas->setAnalysisOverlay(FrameAnalysis {});
    if (m_hexView != nullptr) {
        m_hexView->clearPacket(tr("Select an access unit to inspect packet bytes."));
    }
    m_frameListView->clearFrames();
    resetAccessUnitFilters();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_frameCache.clear();
    m_frameAnalysisByIndex.clear();
    m_accessUnitAnalyses.clear();
    m_seekCheckpoints.clear();
    m_rebufferState.reset();
    m_currentFrameIndex = -1;
    m_latestFrameIndex = -1;
    m_hasCurrentAnalysis = false;
    m_preserveFrameListScroll = false;
    m_playbackPaused = false;
    updateFrameIndexDisplay();
    updateExportActionState();
    updatePlaybackActionState();

    startDecoder(stream.absoluteFilePath);
}

void MainWindow::startDecoder(const QString &filePath,
                              int startFrameIndex,
                              bool pauseAfterFirstFrame,
                              const FrameSeekCheckpoint &seekCheckpoint)
{
    const int generation = ++m_decoderGeneration;
    stopDecoder();

    m_decodeThread = new QThread(this);
    m_decodeWorker = new DecodeWorker;
    m_decodeWorker->moveToThread(m_decodeThread);

    connect(m_decodeThread, &QThread::started, m_decodeWorker, [worker = m_decodeWorker.data(), filePath, startFrameIndex, pauseAfterFirstFrame, seekCheckpoint]() {
        if (seekCheckpoint.frameIndex >= 0) {
            worker->decodeFileFromCheckpoint(filePath, startFrameIndex, pauseAfterFirstFrame, seekCheckpoint);
        } else {
            worker->decodeFileFromFrame(filePath, startFrameIndex, pauseAfterFirstFrame);
        }
    });

    connect(m_decodeWorker, &DecodeWorker::streamOpened, this, [this, generation](const StreamInfo &streamInfo) {
        if (generation != m_decoderGeneration) {
            return;
        }
        m_document.updateStreamInfo(streamInfo);
        populateStreamSelector(streamInfo);
        const QString streamSummary = tr("Codec: %1, %2x%3, %4 fps, %5")
                                          .arg(streamInfo.codecName)
                                          .arg(streamInfo.width)
                                          .arg(streamInfo.height)
                                          .arg(streamInfo.frameRate, 0, 'f', 3)
                                          .arg(streamInfo.pixelFormatName);
        if (streamInfo.codecKind != CodecKind::H264 && streamInfo.codecKind != CodecKind::HEVC) {
            const QString unsupportedMessage =
                tr("%1\n\nPlayback is available, but bitstream syntax analysis is not implemented for this codec yet.")
                    .arg(streamSummary);
            statusBar()->showMessage(
                tr("Playback only: %1 bitstream analysis is not supported yet.")
                    .arg(codecKindName(streamInfo.codecKind)),
                7000);
            m_logDock->appendLine(
                tr("[Info] %1 bitstream analysis is not supported yet; playback will continue without syntax overlays.")
                    .arg(codecKindName(streamInfo.codecKind)));
            m_propertyTreeView->showPlaceholder(unsupportedMessage);
            return;
        }

        statusBar()->showMessage(
            tr("Decoding %1 (%2x%3, %4 fps)")
                .arg(streamInfo.fileName)
                .arg(streamInfo.width)
                .arg(streamInfo.height)
                .arg(streamInfo.frameRate, 0, 'f', 3),
            5000);
        m_propertyTreeView->showPlaceholder(streamSummary);
    });

    connect(m_decodeWorker, &DecodeWorker::frameReady,
            this, [this, generation](int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis) {
                if (generation == m_decoderGeneration) {
                    handleFrameReady(frameIndex, frame, analysis);
                }
            },
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::accessUnitAnalysisDecoded,
            this, [this, generation](const FrameAnalysis &analysis) {
                if (generation == m_decoderGeneration) {
                    handleAccessUnitAnalysis(analysis);
                }
            },
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::seekCheckpointReady,
            this, [this, generation](const FrameSeekCheckpoint &checkpoint) {
                if (generation == m_decoderGeneration) {
                    handleSeekCheckpoint(checkpoint);
                }
            },
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::bufferingProgress,
            this, [this, generation](int startFrameIndex, int currentFrameIndex, int targetFrameIndex) {
                if (generation == m_decoderGeneration) {
                    handleBufferingProgress(startFrameIndex, currentFrameIndex, targetFrameIndex);
                }
            },
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::logMessage,
            this, [this, generation](const QString &message) {
                if (generation == m_decoderGeneration) {
                    m_logDock->appendLine(message);
                }
            },
            Qt::QueuedConnection);
    connect(m_decodeWorker, &DecodeWorker::errorOccurred, this, [this, generation](const QString &message) {
        if (generation != m_decoderGeneration) {
            return;
        }
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Error] %1").arg(message));
    });
    connect(m_decodeWorker, &DecodeWorker::finished, m_decodeThread, &QThread::quit);
    connect(m_decodeWorker, &DecodeWorker::finished, m_decodeWorker, &QObject::deleteLater);
    connect(m_decodeThread, &QThread::finished, m_decodeThread, &QObject::deleteLater);
    connect(m_decodeThread, &QThread::finished, this, [this, generation]() {
        if (generation != m_decoderGeneration) {
            return;
        }
        m_decodeThread = nullptr;
        m_decodeWorker = nullptr;
        m_rebufferState.reset();
        m_playbackPaused = false;
        updatePlaybackActionState();
        setPlaybackControlsEnabled(hasOpenStream());
        m_logDock->appendLine(tr("[Info] Decode thread stopped."));
    });

    m_playbackPaused = pauseAfterFirstFrame;
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
        if (hasOpenStream()) {
            replayFromBeginning();
        }
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
    m_preserveFrameListScroll = false;
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
    const int targetFrame = m_currentFrameIndex - 1;
    if (!showFrameFromCache(targetFrame, true, true)) {
        seekToFrame(targetFrame);
    }
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

    if (nextIndex <= m_latestFrameIndex) {
        seekToFrame(nextIndex);
    } else if (!m_decodeWorker.isNull()) {
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

void MainWindow::replayFromBeginning()
{
    if (!hasOpenStream()) {
        return;
    }

    m_frameListView->clearFrames();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_videoCanvas->setAnalysisOverlay(FrameAnalysis {});
    m_frameCache.clear();
    m_frameAnalysisByIndex.clear();
    m_accessUnitAnalyses.clear();
    m_rebufferState.reset();
    m_currentFrameIndex = -1;
    m_latestFrameIndex = -1;
    m_hasCurrentAnalysis = false;
    m_playbackPaused = false;
    updateFrameIndexDisplay();
    updateExportActionState();

    const StreamInfo &stream = m_document.streamInfo();
    statusBar()->showMessage(tr("Replaying %1").arg(stream.fileName), 3000);
    m_logDock->appendLine(tr("[Info] Replaying from the beginning."));
    startDecoder(stream.absoluteFilePath);
}

void MainWindow::exportFrameSyntaxJson()
{
    if (!m_hasCurrentAnalysis) {
        const QString message = tr("No selected access unit is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString defaultPrefix = m_currentAnalysis.accessUnitKind == AccessUnitKind::AudioFrame
        ? QStringLiteral("audio-access-unit")
        : QStringLiteral("frame");
    const QString defaultName = QStringLiteral("%1-%2-syntax.json")
                                    .arg(defaultPrefix)
                                    .arg(m_currentAnalysis.frameIndex, 5, 10, QLatin1Char('0'));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Selected Access Unit JSON"),
        QDir(defaultExportDirectory()).filePath(defaultName),
        tr("JSON Files (*.json);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = tr("Failed to export JSON: %1").arg(file.errorString());
        m_logDock->appendLine(tr("[Error] %1").arg(message));
        statusBar()->showMessage(message, 5000);
        return;
    }

    const QJsonDocument document(selectedFrameExportToJson(m_document.streamInfo(),
                                                           m_currentAnalysis,
                                                           QCoreApplication::applicationName(),
                                                           QCoreApplication::applicationVersion()));
    file.write(document.toJson(QJsonDocument::Indented));
    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported access-unit syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported access-unit syntax JSON"), 3000);
}

void MainWindow::exportAllFrameSyntaxJson()
{
    if (m_accessUnitAnalyses.isEmpty()) {
        const QString message = tr("No decoded access-unit syntax is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export All Decoded Access Units JSON"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("decoded-access-unit-syntax.json")),
        tr("JSON Files (*.json);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = tr("Failed to export JSON: %1").arg(file.errorString());
        m_logDock->appendLine(tr("[Error] %1").arg(message));
        statusBar()->showMessage(message, 5000);
        return;
    }

    const QJsonDocument document(allFramesExportToJson(m_document.streamInfo(),
                                                       m_accessUnitAnalyses,
                                                       QCoreApplication::applicationName(),
                                                       QCoreApplication::applicationVersion()));
    file.write(document.toJson(QJsonDocument::Indented));
    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported all decoded access-unit syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported decoded access-unit syntax JSON"), 3000);
}

void MainWindow::exportFrameListCsv()
{
    if (m_accessUnitAnalyses.isEmpty()) {
        const QString message = tr("No access-unit list is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Access Unit List CSV"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("access-unit-list.csv")),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString message = tr("Failed to export CSV: %1").arg(file.errorString());
        m_logDock->appendLine(tr("[Error] %1").arg(message));
        statusBar()->showMessage(message, 5000);
        return;
    }

    QTextStream out(&file);
    out << "index,stream_index,media_kind,access_unit_kind,type,pts,dts,poc,frame_num,"
           "stream_packet_index,container_packet_index,packet_pos,packet_size,packet_duration,keyframe,raw_bytes_size\n";
    for (const FrameAnalysis &analysis : std::as_const(m_accessUnitAnalyses)) {
        if (analysis.frameIndex < 0) {
            continue;
        }
        out << csvEscape(QString::number(analysis.frameIndex)) << ','
            << csvEscape(QString::number(analysis.streamIndex)) << ','
            << csvEscape(mediaKindName(analysis.mediaKind)) << ','
            << csvEscape(accessUnitKindName(analysis.accessUnitKind)) << ','
            << csvEscape(analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType) << ','
            << csvEscape(QString::number(analysis.pts)) << ','
            << csvEscape(QString::number(analysis.dts)) << ','
            << csvEscape(analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-")) << ','
            << csvEscape(analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-")) << ','
            << csvEscape(QString::number(analysis.packet.streamPacketIndex)) << ','
            << csvEscape(QString::number(analysis.packet.containerPacketIndex)) << ','
            << csvEscape(QString::number(analysis.packet.position)) << ','
            << csvEscape(QString::number(analysis.packet.size)) << ','
            << csvEscape(QString::number(analysis.packet.duration)) << ','
            << csvEscape(analysis.packet.keyframe ? QStringLiteral("true") : QStringLiteral("false")) << ','
            << csvEscape(QString::number(analysis.packet.bytes.size())) << '\n';
    }

    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported access-unit list CSV: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported access-unit list CSV"), 3000);
}

void MainWindow::exportScreenshot()
{
    if (m_videoCanvas == nullptr) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Screenshot"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("zstreameye-screenshot.png")),
        tr("PNG Images (*.png);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    const QImage image = m_videoCanvas->grabFramebuffer();
    if (image.isNull() || !image.save(filePath)) {
        const QString message = tr("Failed to export screenshot: %1").arg(QDir::toNativeSeparators(filePath));
        m_logDock->appendLine(tr("[Error] %1").arg(message));
        statusBar()->showMessage(message, 5000);
        return;
    }

    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported screenshot: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported screenshot"), 3000);
}

void MainWindow::checkForUpdates()
{
    if (m_updateNetworkManager == nullptr) {
        m_updateNetworkManager = new QNetworkAccessManager(this);
    }

    if (m_checkForUpdatesAction != nullptr) {
        m_checkForUpdatesAction->setEnabled(false);
    }

    const QUrl latestReleaseUrl(QStringLiteral("https://api.github.com/repos/FreddieGeorge/ZStreamEye/releases/latest"));
    QNetworkRequest request(latestReleaseUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    statusBar()->showMessage(tr("Checking for updates..."), 3000);
    m_logDock->appendLine(tr("[Info] Checking GitHub Releases for updates."));

    QNetworkReply *reply = m_updateNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_checkForUpdatesAction != nullptr) {
            m_checkForUpdatesAction->setEnabled(true);
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = tr("Update check failed: %1").arg(reply->errorString());
            statusBar()->showMessage(message, 5000);
            m_logDock->appendLine(tr("[Warning] %1").arg(message));
            QMessageBox::warning(this, tr("Check for Updates"), message);
            return;
        }

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            const QString message = tr("GitHub returned an invalid release response.");
            statusBar()->showMessage(message, 5000);
            m_logDock->appendLine(tr("[Warning] %1").arg(message));
            QMessageBox::warning(this, tr("Check for Updates"), message);
            return;
        }

        const QJsonObject release = document.object();
        const QString tagName = release.value(QStringLiteral("tag_name")).toString().trimmed();
        const QString releasePage = release.value(QStringLiteral("html_url")).toString().trimmed();
        const QString releaseNotes = compactReleaseNotes(release.value(QStringLiteral("body")).toString());
        const ReleaseAsset installerAsset = releaseAsset(
            release,
            QRegularExpression(QStringLiteral("^ZStreamEye-.+-windows-ucrt64-setup\\.exe$")));
        const ReleaseAsset portableAsset = releaseAsset(
            release,
            QRegularExpression(QStringLiteral("^ZStreamEye-.+-windows-ucrt64\\.zip$")));
        const ReleaseAsset installerChecksumAsset = installerAsset.name.isEmpty()
            ? ReleaseAsset {}
            : releaseAsset(
                  release,
                  QRegularExpression(QStringLiteral("^%1\\.sha256$")
                                         .arg(QRegularExpression::escape(installerAsset.name))));
        const QString currentVersion = QCoreApplication::applicationVersion();

        if (tagName.isEmpty() || releasePage.isEmpty()) {
            const QString message = tr("GitHub release response did not include a release tag or page URL.");
            statusBar()->showMessage(message, 5000);
            m_logDock->appendLine(tr("[Warning] %1").arg(message));
            QMessageBox::warning(this, tr("Check for Updates"), message);
            return;
        }

        if (compareVersions(tagName, currentVersion) <= 0) {
            statusBar()->showMessage(tr("ZStreamEye is up to date."), 4000);
            m_logDock->appendLine(tr("[Info] No update available. Current version: %1, latest release: %2.")
                                      .arg(currentVersion, tagName));
            QMessageBox::information(
                this,
                tr("Check for Updates"),
                tr("You are using the latest version.\n\nCurrent version: %1\nLatest release: %2")
                    .arg(currentVersion, tagName));
            return;
        }

        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Update Available"));
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setText(tr("A new ZStreamEye release is available."));
        messageBox.setInformativeText(
            tr("Current version: %1\nLatest release: %2\n\nDownload the installer, verify SHA256, then close ZStreamEye and start the installer.")
                .arg(currentVersion, tagName));
        if (!releaseNotes.isEmpty()) {
            messageBox.setDetailedText(releaseNotes);
        }

        QPushButton *downloadInstallerButton = nullptr;
        QPushButton *downloadPortableButton = nullptr;
        if (!installerAsset.downloadUrl.isEmpty() && !installerChecksumAsset.downloadUrl.isEmpty()) {
            downloadInstallerButton = messageBox.addButton(tr("Download and Install"), QMessageBox::AcceptRole);
        } else if (!installerAsset.downloadUrl.isEmpty()) {
            messageBox.setInformativeText(
                messageBox.informativeText()
                + tr("\n\nThe installer is available, but its SHA256 checksum asset is missing. Open the release page to download it manually."));
        }
        if (!portableAsset.downloadUrl.isEmpty()) {
            downloadPortableButton = messageBox.addButton(tr("Download Portable ZIP"), QMessageBox::ActionRole);
        }
        QPushButton *openButton = messageBox.addButton(tr("Open Release Page"), QMessageBox::AcceptRole);
        messageBox.addButton(QMessageBox::Close);
        messageBox.exec();

        if (messageBox.clickedButton() == downloadInstallerButton) {
            downloadAndInstallUpdate(QUrl(installerAsset.downloadUrl),
                                     QUrl(installerChecksumAsset.downloadUrl),
                                     installerAsset.name,
                                     tagName);
        } else if (messageBox.clickedButton() == downloadPortableButton) {
            QDesktopServices::openUrl(QUrl(portableAsset.downloadUrl));
        } else if (messageBox.clickedButton() == openButton) {
            QDesktopServices::openUrl(QUrl(releasePage));
        }

        statusBar()->showMessage(tr("Update available: %1").arg(tagName), 5000);
        m_logDock->appendLine(tr("[Info] Update available. Current version: %1, latest release: %2.")
                                  .arg(currentVersion, tagName));
    });
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About ZStreamEye"));
    aboutBox.setIconPixmap(windowIcon().pixmap(64, 64));
    aboutBox.setText(tr("ZStreamEye"));
    aboutBox.setInformativeText(
        tr("Version %1\n\n"
           "A desktop tool for inspecting video bitstreams, decoded frames, and codec syntax.\n\n"
           "GitHub: https://github.com/FreddieGeorge/ZStreamEye\n\n"
           "Copyright 2026 ZStreamEye contributors.\n"
           "Open source project. See the repository for license and release information.")
            .arg(QCoreApplication::applicationVersion()));
    QPushButton *githubButton = aboutBox.addButton(tr("Open GitHub"), QMessageBox::ActionRole);
    QPushButton *closeButton = aboutBox.addButton(QMessageBox::Close);
    aboutBox.setEscapeButton(closeButton);
    aboutBox.exec();
    if (aboutBox.clickedButton() == githubButton) {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/FreddieGeorge/ZStreamEye")));
    }
}

void MainWindow::downloadAndInstallUpdate(const QUrl &installerUrl,
                                          const QUrl &checksumUrl,
                                          const QString &installerFileName,
                                          const QString &tagName)
{
    if (m_updateNetworkManager == nullptr) {
        m_updateNetworkManager = new QNetworkAccessManager(this);
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir updateDir(QDir(tempRoot).filePath(QStringLiteral("ZStreamEye-updates")));
    if (!updateDir.exists() && !QDir().mkpath(updateDir.absolutePath())) {
        QMessageBox::warning(this,
                             tr("Update ZStreamEye"),
                             tr("Failed to create update download directory:\n%1").arg(updateDir.absolutePath()));
        return;
    }

    const QString safeFileName = safeInstallerFileName(installerFileName);
    const QString installerPath = updateDir.filePath(safeFileName);
    if (QFile::exists(installerPath) && !QFile::remove(installerPath)) {
        QMessageBox::warning(this,
                             tr("Update ZStreamEye"),
                             tr("Failed to replace existing installer:\n%1").arg(installerPath));
        return;
    }

    if (m_checkForUpdatesAction != nullptr) {
        m_checkForUpdatesAction->setEnabled(false);
    }

    auto canceled = std::make_shared<bool>(false);
    auto *progress = new QProgressDialog(tr("Downloading checksum..."), tr("Cancel"), 0, 0, this);
    progress->setWindowTitle(tr("Update ZStreamEye"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->show();

    QNetworkRequest checksumRequest(checksumUrl);
    checksumRequest.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
    QNetworkReply *checksumReply = m_updateNetworkManager->get(checksumRequest);
    connect(progress, &QProgressDialog::canceled, checksumReply, [checksumReply, canceled]() {
        *canceled = true;
        checksumReply->abort();
    });

    connect(checksumReply, &QNetworkReply::finished, this, [this, checksumReply, installerUrl, installerPath, tagName, progress, canceled]() {
        checksumReply->deleteLater();

        const auto finishWithWarning = [this, progress](const QString &message) {
            if (m_checkForUpdatesAction != nullptr) {
                m_checkForUpdatesAction->setEnabled(true);
            }
            progress->close();
            progress->deleteLater();
            statusBar()->showMessage(message, 5000);
            m_logDock->appendLine(tr("[Warning] %1").arg(message));
            QMessageBox::warning(this, tr("Update ZStreamEye"), message);
        };
        const auto finishCanceled = [this, progress]() {
            if (m_checkForUpdatesAction != nullptr) {
                m_checkForUpdatesAction->setEnabled(true);
            }
            progress->close();
            progress->deleteLater();
            statusBar()->showMessage(tr("Update canceled."), 3000);
            m_logDock->appendLine(tr("[Info] Update canceled by user."));
        };

        if (checksumReply->error() != QNetworkReply::NoError) {
            if (*canceled) {
                finishCanceled();
            } else {
                finishWithWarning(tr("Failed to download SHA256 checksum. Please try again.\n\n%1")
                                      .arg(checksumReply->errorString()));
            }
            return;
        }

        const QString expectedSha256 = sha256FromChecksumText(QString::fromUtf8(checksumReply->readAll()));
        if (expectedSha256.isEmpty()) {
            finishWithWarning(tr("The release checksum file did not contain a valid SHA256 hash. Open the release page and download the installer manually."));
            return;
        }

        progress->setLabelText(tr("Downloading installer..."));
        progress->setRange(0, 0);
        progress->setValue(0);

        auto *installerFile = new QFile(installerPath, progress);
        if (!installerFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            finishWithWarning(tr("Failed to write installer: %1").arg(installerFile->errorString()));
            installerFile->deleteLater();
            return;
        }

        QNetworkRequest installerRequest(installerUrl);
        installerRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                   QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
        QNetworkReply *installerReply = m_updateNetworkManager->get(installerRequest);
        connect(progress, &QProgressDialog::canceled, installerReply, [installerReply, canceled]() {
            *canceled = true;
            installerReply->abort();
        });
        connect(installerReply, &QNetworkReply::readyRead, installerFile, [installerReply, installerFile]() {
            installerFile->write(installerReply->readAll());
        });
        connect(installerReply, &QNetworkReply::downloadProgress, progress, [progress](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                progress->setRange(0, 100);
                progress->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
            } else {
                progress->setRange(0, 0);
            }
        });

        connect(installerReply, &QNetworkReply::finished, this, [this, installerReply, installerFile, installerPath, expectedSha256, tagName, progress, canceled]() {
            installerReply->deleteLater();
            installerFile->write(installerReply->readAll());
            installerFile->flush();
            installerFile->close();
            installerFile->deleteLater();

            const auto finishWithWarning = [this, progress, installerPath](const QString &message) {
                if (m_checkForUpdatesAction != nullptr) {
                    m_checkForUpdatesAction->setEnabled(true);
                }
                QFile::remove(installerPath);
                progress->close();
                progress->deleteLater();
                statusBar()->showMessage(message, 5000);
                m_logDock->appendLine(tr("[Warning] %1").arg(message));
                QMessageBox::warning(this, tr("Update ZStreamEye"), message);
            };
            const auto finishCanceled = [this, progress, installerPath]() {
                if (m_checkForUpdatesAction != nullptr) {
                    m_checkForUpdatesAction->setEnabled(true);
                }
                QFile::remove(installerPath);
                progress->close();
                progress->deleteLater();
                statusBar()->showMessage(tr("Update canceled."), 3000);
                m_logDock->appendLine(tr("[Info] Update canceled by user."));
            };

            if (installerReply->error() != QNetworkReply::NoError) {
                if (*canceled) {
                    finishCanceled();
                } else {
                    finishWithWarning(tr("Failed to download installer. Please try again.\n\n%1")
                                          .arg(installerReply->errorString()));
                }
                return;
            }

            progress->setLabelText(tr("Verifying SHA256..."));
            progress->setRange(0, 0);
            QString hashError;
            const QString actualSha256 = sha256ForFile(installerPath, &hashError);
            if (actualSha256.isEmpty()) {
                finishWithWarning(tr("Failed to verify installer: %1").arg(hashError));
                return;
            }
            if (actualSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
                finishWithWarning(tr("Installer SHA256 verification failed. The downloaded file was removed."));
                return;
            }

            progress->close();
            progress->deleteLater();

            const QMessageBox::StandardButton choice = QMessageBox::question(
                this,
                tr("Install Update"),
                tr("ZStreamEye %1 was downloaded and verified.\n\nThe installer will now start, and ZStreamEye will close. Continue?")
                    .arg(tagName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (choice != QMessageBox::Yes) {
                if (m_checkForUpdatesAction != nullptr) {
                    m_checkForUpdatesAction->setEnabled(true);
                }
                statusBar()->showMessage(tr("Update installer downloaded and verified."), 5000);
                m_logDock->appendLine(tr("[Info] Update installer downloaded and verified: %1")
                                          .arg(QDir::toNativeSeparators(installerPath)));
                return;
            }

            const bool started = QProcess::startDetached(
                QDir::toNativeSeparators(installerPath),
                QStringList {},
                QFileInfo(installerPath).absolutePath());
            if (!started) {
                if (m_checkForUpdatesAction != nullptr) {
                    m_checkForUpdatesAction->setEnabled(true);
                }
                const QString message = tr("Failed to start installer: %1").arg(QDir::toNativeSeparators(installerPath));
                statusBar()->showMessage(message, 5000);
                m_logDock->appendLine(tr("[Warning] %1").arg(message));
                QMessageBox::warning(this, tr("Update ZStreamEye"), message);
                return;
            }

            m_logDock->appendLine(tr("[Info] Started verified update installer: %1")
                                      .arg(QDir::toNativeSeparators(installerPath)));
            close();
        });
    });
}

void MainWindow::handleFrameReady(int frameIndex,
                                  const DecodedVideoFramePtr &frame,
                                  const FrameAnalysis &analysis)
{
    CachedFrame cached;
    cached.index = frameIndex;
    cached.frame = frame;
    cached.analysis = analysis;
    m_frameCache.append(cached);
    while (m_frameCache.size() > MaxCachedFrames) {
        m_frameCache.removeFirst();
    }

    m_latestFrameIndex = std::max(m_latestFrameIndex, frameIndex);
    showFrameFromCache(frameIndex, true, m_playbackPaused);
    if (m_rebufferState.complete(m_decoderGeneration, frameIndex)) {
        statusBar()->showMessage(tr("Buffered frame %1").arg(frameIndex + 1), 2000);
    }
    updateExportActionState();
}

void MainWindow::handleAccessUnitAnalysis(const FrameAnalysis &analysis)
{
    if (analysis.frameIndex < 0) {
        return;
    }

    if (analysis.accessUnitKind == AccessUnitKind::VideoFrame) {
        if (analysis.frameIndex >= m_frameAnalysisByIndex.size()) {
            m_frameAnalysisByIndex.resize(analysis.frameIndex + 1);
        }
        m_frameAnalysisByIndex[analysis.frameIndex] = analysis;
    }

    bool replaced = false;
    for (FrameAnalysis &existing : m_accessUnitAnalyses) {
        if (existing.frameIndex == analysis.frameIndex
            && existing.streamIndex == analysis.streamIndex
            && existing.mediaKind == analysis.mediaKind
            && existing.accessUnitKind == analysis.accessUnitKind) {
            existing = analysis;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_accessUnitAnalyses.append(analysis);
    }
    m_frameListView->addFrameAnalysis(analysis);
    if (analysis.accessUnitKind != AccessUnitKind::VideoFrame && !m_hasCurrentAnalysis) {
        m_currentAnalysis = analysis;
        m_hasCurrentAnalysis = true;
        m_propertyTreeView->showFrameAnalysis(analysis);
        if (m_hexView != nullptr) {
            m_hexView->showPacket(analysis);
        }
    }
    updateExportActionState();
}

void MainWindow::handleSeekCheckpoint(const FrameSeekCheckpoint &checkpoint)
{
    if (checkpoint.frameIndex < 0) {
        return;
    }

    for (FrameSeekCheckpoint &existing : m_seekCheckpoints) {
        if (existing.frameIndex == checkpoint.frameIndex) {
            existing = checkpoint;
            return;
        }
    }

    m_seekCheckpoints.append(checkpoint);
    std::sort(m_seekCheckpoints.begin(), m_seekCheckpoints.end(), [](const FrameSeekCheckpoint &a, const FrameSeekCheckpoint &b) {
        return a.frameIndex < b.frameIndex;
    });
}

void MainWindow::handleBufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex)
{
    if (!m_rebufferState.accepts(m_decoderGeneration, targetFrameIndex)) {
        return;
    }

    const RebufferState::Progress progress = RebufferState::progress(startFrameIndex,
                                                                     currentFrameIndex,
                                                                     targetFrameIndex);
    statusBar()->showMessage(
        tr("Buffering frame %1 from checkpoint %2: %3/%4 (%5%)")
            .arg(targetFrameIndex + 1)
            .arg(startFrameIndex + 1)
            .arg(progress.bufferedFrames)
            .arg(progress.totalFrames)
            .arg(progress.percent));
}

void MainWindow::handleFrameListSelection(int frameIndex)
{
    if (!m_decodeWorker.isNull()) {
        m_decodeWorker->pause();
        m_playbackPaused = true;
        m_preserveFrameListScroll = true;
        updatePlaybackActionState();
    }
    if (!showFrameFromCache(frameIndex, false, true)) {
        seekToFrame(frameIndex);
    }
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
        m_currentAnalysis = cached.analysis;
        m_hasCurrentAnalysis = true;
        m_videoCanvas->setFrame(cached.frame);
        m_videoCanvas->setAnalysisOverlay(cached.analysis);
        if (m_hexView != nullptr) {
            m_hexView->showPacket(cached.analysis);
        }
        if (updatePropertyTree) {
            m_propertyTreeView->showFrameAnalysis(cached.analysis);
            updateOverlayStatusHint(cached.analysis);
        }
        if (selectInList) {
            const bool scrollSelection = !m_playbackPaused && !m_preserveFrameListScroll;
            m_frameListView->selectFrameIndex(frameIndex, scrollSelection);
            if (!scrollSelection) {
                m_preserveFrameListScroll = false;
            }
        }
        updateFrameIndexDisplay();
        updateExportActionState();
        return true;
    }

    return false;
}

void MainWindow::seekToFrame(int frameIndex)
{
    if (frameIndex < 0 || !m_document.streamInfo().isValid) {
        return;
    }

    m_playbackPaused = true;
    m_preserveFrameListScroll = true;
    updatePlaybackActionState();
    setPlaybackControlsEnabled(false);
    m_videoCanvas->setOverlayMessage(QString());
    const RebufferState::StartResult rebufferStart = m_rebufferState.start(frameIndex, m_decoderGeneration + 1);
    if (rebufferStart.canceledPrevious) {
        m_logDock->appendLine(tr("[Info] Canceling pending rebuffer for frame %1.")
                                  .arg(rebufferStart.canceledTargetFrameIndex));
    }
    statusBar()->showMessage(tr("Buffering frame %1...").arg(frameIndex + 1));

    FrameSeekCheckpoint checkpoint;
    for (const FrameSeekCheckpoint &candidate : std::as_const(m_seekCheckpoints)) {
        if (candidate.frameIndex <= frameIndex
            && candidate.frameIndex >= checkpoint.frameIndex
            && (candidate.keyframe || candidate.idr || candidate.frameIndex == 0)) {
            checkpoint = candidate;
        }
    }

    if (checkpoint.frameIndex >= 0) {
        m_logDock->appendLine(tr("[Info] Frame %1 is outside the recent cache; seeking from checkpoint frame %2.")
                                  .arg(frameIndex)
                                  .arg(checkpoint.frameIndex));
        startDecoder(m_document.streamInfo().absoluteFilePath, frameIndex, true, checkpoint);
    } else {
        m_logDock->appendLine(tr("[Info] Frame %1 is outside the recent cache; no checkpoint is available yet, decoding from the beginning.")
                                  .arg(frameIndex));
        startDecoder(m_document.streamInfo().absoluteFilePath, frameIndex, true);
    }
}

const MainWindow::CachedFrame *MainWindow::currentCachedFrame() const
{
    for (const CachedFrame &cached : m_frameCache) {
        if (cached.index == m_currentFrameIndex) {
            return &cached;
        }
    }
    return nullptr;
}

void MainWindow::setPlaybackControlsEnabled(bool enabled)
{
    if (m_playPauseAction != nullptr) {
        m_playPauseAction->setEnabled(enabled);
    }
    setNavigationControlsEnabled(enabled);
    if (m_stopAction != nullptr) {
        m_stopAction->setEnabled(enabled && !m_decodeWorker.isNull());
    }
}

void MainWindow::setNavigationControlsEnabled(bool enabled)
{
    if (m_previousFrameAction != nullptr) {
        m_previousFrameAction->setEnabled(enabled);
    }
    if (m_nextFrameAction != nullptr) {
        m_nextFrameAction->setEnabled(enabled);
    }
}

void MainWindow::updatePlaybackActionState()
{
    if (m_playPauseAction == nullptr) {
        return;
    }

    if (m_decodeWorker.isNull()) {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseAction->setText(hasOpenStream() ? tr("Replay") : tr("Play"));
        if (m_stopAction != nullptr) {
            m_stopAction->setEnabled(false);
        }
        return;
    }

    if (m_playbackPaused) {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseAction->setText(tr("Play"));
    } else {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        m_playPauseAction->setText(tr("Pause"));
    }
    if (m_stopAction != nullptr) {
        m_stopAction->setEnabled(true);
    }
}

void MainWindow::updateExportActionState()
{
    const bool hasCurrentFrame = currentCachedFrame() != nullptr;
    const bool hasDecodedSyntax = !m_accessUnitAnalyses.isEmpty();

    if (m_exportFrameSyntaxJsonAction != nullptr) {
        m_exportFrameSyntaxJsonAction->setEnabled(m_hasCurrentAnalysis);
    }
    if (m_exportScreenshotAction != nullptr) {
        m_exportScreenshotAction->setEnabled(hasCurrentFrame);
    }
    if (m_exportFrameListCsvAction != nullptr) {
        m_exportFrameListCsvAction->setEnabled(hasDecodedSyntax);
    }
    if (m_exportAllFrameSyntaxJsonAction != nullptr) {
        m_exportAllFrameSyntaxJsonAction->setEnabled(hasDecodedSyntax);
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

void MainWindow::updateCurrentOverlayStatusHint()
{
    const CachedFrame *cached = currentCachedFrame();
    if (cached == nullptr) {
        return;
    }

    updateOverlayStatusHint(cached->analysis);
}

void MainWindow::updateOverlayStatusHint(const FrameAnalysis &analysis)
{
    if (m_showMotionVectorsAction != nullptr && m_showMotionVectorsAction->isChecked()) {
        statusBar()->showMessage(motionVectorStatusText(analysis),
                                 analysis.motionVectors.isEmpty() ? 5000 : 3000);
        return;
    }

    if (m_showQpHeatmapAction != nullptr && m_showQpHeatmapAction->isChecked()) {
        int minQp = 0;
        int maxQp = 0;
        const int qpCount = frameAnalysisQpValueCount(analysis, &minQp, &maxQp);
        if (qpCount == 0) {
            statusBar()->showMessage(tr("QP heatmap has no parsed QP values for this frame."), 5000);
            return;
        }

        if (minQp == maxQp) {
            statusBar()->showMessage(
                tr("QP heatmap: %1 values, constant QP %2.")
                    .arg(qpCount)
                    .arg(minQp),
                4000);
            return;
        }

        statusBar()->showMessage(
            tr("QP heatmap: %1 values, range %2 - %3.")
                .arg(qpCount)
                .arg(minQp)
                .arg(maxQp),
            4000);
    }
}

void MainWindow::resetAccessUnitFilters()
{
    if (m_streamSelector != nullptr) {
        const QSignalBlocker blocker(m_streamSelector);
        m_streamSelector->clear();
        m_streamSelector->addItem(tr("All streams"), -1);
        m_streamSelector->setCurrentIndex(0);
        m_streamSelector->setEnabled(false);
    }
    if (m_accessUnitFilterSelector != nullptr) {
        const QSignalBlocker blocker(m_accessUnitFilterSelector);
        m_accessUnitFilterSelector->setCurrentIndex(0);
        m_accessUnitFilterSelector->setEnabled(false);
    }
    if (m_frameListView != nullptr) {
        m_frameListView->setStreamFilter(-1);
        m_frameListView->setAccessUnitFilter(FrameListView::AccessUnitFilter::All);
    }
}

void MainWindow::populateStreamSelector(const StreamInfo &streamInfo)
{
    if (m_streamSelector == nullptr || m_accessUnitFilterSelector == nullptr) {
        return;
    }

    const QVariant currentFilter = m_streamSelector->currentData();
    const int previousStreamFilter = currentFilter.isValid() ? currentFilter.toInt() : -1;

    const QSignalBlocker blocker(m_streamSelector);
    m_streamSelector->clear();
    m_streamSelector->addItem(tr("All streams"), -1);
    int selectedIndex = 0;
    for (const MediaStreamInfo &stream : streamInfo.streams) {
        m_streamSelector->addItem(streamSelectorText(stream), stream.streamIndex);
        if (previousStreamFilter >= 0 && stream.streamIndex == previousStreamFilter) {
            selectedIndex = m_streamSelector->count() - 1;
        }
    }
    m_streamSelector->setCurrentIndex(selectedIndex);
    m_streamSelector->setEnabled(!streamInfo.streams.isEmpty());
    m_accessUnitFilterSelector->setEnabled(!streamInfo.streams.isEmpty());

    if (m_frameListView != nullptr) {
        m_frameListView->setStreamFilter(m_streamSelector->itemData(selectedIndex).toInt());
    }
}

QString MainWindow::defaultOpenDirectory() const
{
    if (!m_lastOpenDirectory.isEmpty()) {
        return m_lastOpenDirectory;
    }
    return firstWritableLocation(QStandardPaths::DocumentsLocation);
}

QString MainWindow::defaultExportDirectory() const
{
    if (!m_lastExportDirectory.isEmpty()) {
        return m_lastExportDirectory;
    }
    return defaultOpenDirectory();
}

bool MainWindow::hasOpenStream() const
{
    const StreamInfo &stream = m_document.streamInfo();
    return stream.isValid && !stream.absoluteFilePath.isEmpty();
}

void MainWindow::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray state = settings.value(QStringLiteral("ui/windowState")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    m_lastOpenDirectory = settings.value(QStringLiteral("paths/lastOpenDirectory"), m_lastOpenDirectory).toString();
    m_lastExportDirectory = settings.value(QStringLiteral("paths/lastExportDirectory"), m_lastExportDirectory).toString();

    if (m_showGridAction != nullptr) {
        m_showGridAction->setChecked(settings.value(QStringLiteral("overlays/showGrid"), m_showGridAction->isChecked()).toBool());
    }
    if (m_showQpHeatmapAction != nullptr) {
        m_showQpHeatmapAction->setChecked(settings.value(QStringLiteral("overlays/showQpHeatmap"), m_showQpHeatmapAction->isChecked()).toBool());
    }
    if (m_showMotionVectorsAction != nullptr) {
        m_showMotionVectorsAction->setChecked(settings.value(QStringLiteral("overlays/showMotionVectors"), m_showMotionVectorsAction->isChecked()).toBool());
    }
    if (m_overlayOpacitySlider != nullptr) {
        m_overlayOpacitySlider->setValue(settings.value(QStringLiteral("overlays/opacity"), m_overlayOpacitySlider->value()).toInt());
    }
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/windowState"), saveState());
    settings.setValue(QStringLiteral("paths/lastOpenDirectory"), m_lastOpenDirectory);
    settings.setValue(QStringLiteral("paths/lastExportDirectory"), m_lastExportDirectory);
    if (m_showGridAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showGrid"), m_showGridAction->isChecked());
    }
    if (m_showQpHeatmapAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showQpHeatmap"), m_showQpHeatmapAction->isChecked());
    }
    if (m_showMotionVectorsAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showMotionVectors"), m_showMotionVectorsAction->isChecked());
    }
    if (m_overlayOpacitySlider != nullptr) {
        settings.setValue(QStringLiteral("overlays/opacity"), m_overlayOpacitySlider->value());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
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
