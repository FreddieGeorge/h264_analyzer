#include "app/MainWindow.h"

#include "core/DecodeWorker.h"
#include "ui/FrameListView.h"
#include "ui/LogDock.h"
#include "ui/PropertyTreeView.h"
#include "ui/VideoCanvas.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
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
#include <QStandardPaths>
#include <QSettings>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QToolBar>
#include <QSlider>
#include <QTextStream>
#include <QTreeWidgetItem>
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

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QJsonObject motionVectorToJson(const MotionVectorInfo &mv)
{
    return {
        {QStringLiteral("list"), mv.list},
        {QStringLiteral("reference_index"), mv.referenceIndex},
        {QStringLiteral("mv_x_quarter_pel"), mv.mvXQuarterPel},
        {QStringLiteral("mv_y_quarter_pel"), mv.mvYQuarterPel},
        {QStringLiteral("reference_x"), mv.referenceX},
        {QStringLiteral("reference_y"), mv.referenceY}
    };
}

QJsonObject diagnosticToJson(const ParserDiagnosticInfo &diagnostic)
{
    return {
        {QStringLiteral("code"), diagnostic.code},
        {QStringLiteral("message"), diagnostic.message}
    };
}

QJsonObject macroblockToJson(const MacroblockInfo &mb)
{
    QJsonArray motionVectors;
    for (const MotionVectorInfo &mv : mb.motionVectors) {
        motionVectors.append(motionVectorToJson(mv));
    }

    return {
        {QStringLiteral("address"), mb.address},
        {QStringLiteral("mb_type"), mb.mbType},
        {QStringLiteral("prediction_mode"), mb.predictionMode},
        {QStringLiteral("coded_block_pattern"), mb.codedBlockPattern},
        {QStringLiteral("coded_block_pattern_luma"), mb.codedBlockPatternLuma},
        {QStringLiteral("coded_block_pattern_chroma"), mb.codedBlockPatternChroma},
        {QStringLiteral("qp"), mb.qp},
        {QStringLiteral("mb_qp_delta"), mb.mbQpDelta},
        {QStringLiteral("residual_block_count"), mb.residualBlockCount},
        {QStringLiteral("residual_coefficient_count"), mb.residualCoefficientCount},
        {QStringLiteral("residual_parsed"), mb.residualParsed},
        {QStringLiteral("skipped"), mb.skipped},
        {QStringLiteral("parsed"), mb.parsed},
        {QStringLiteral("note"), mb.note},
        {QStringLiteral("motion_vectors"), motionVectors}
    };
}

QJsonObject spsToJson(const SpsInfo &sps)
{
    return {
        {QStringLiteral("valid"), sps.valid},
        {QStringLiteral("profile_idc"), sps.profileIdc},
        {QStringLiteral("level_idc"), sps.levelIdc},
        {QStringLiteral("seq_parameter_set_id"), sps.seqParameterSetId},
        {QStringLiteral("width"), sps.width},
        {QStringLiteral("height"), sps.height},
        {QStringLiteral("vui_parameters_present_flag"), sps.vuiParametersPresentFlag},
        {QStringLiteral("aspect_ratio_idc"), sps.aspectRatioIdc},
        {QStringLiteral("timing_info_present_flag"), sps.timingInfoPresentFlag},
        {QStringLiteral("bitstream_restriction_flag"), sps.bitstreamRestrictionFlag}
    };
}

QJsonObject ppsToJson(const PpsInfo &pps)
{
    return {
        {QStringLiteral("valid"), pps.valid},
        {QStringLiteral("pic_parameter_set_id"), pps.picParameterSetId},
        {QStringLiteral("seq_parameter_set_id"), pps.seqParameterSetId},
        {QStringLiteral("entropy_coding_mode_flag"), pps.entropyCodingModeFlag},
        {QStringLiteral("weighted_pred_flag"), pps.weightedPredFlag},
        {QStringLiteral("weighted_bipred_idc"), pps.weightedBipredIdc},
        {QStringLiteral("transform_8x8_mode_flag"), pps.transform8x8ModeFlag},
        {QStringLiteral("pic_init_qp_minus26"), pps.picInitQpMinus26}
    };
}

QJsonObject streamInfoToJson(const StreamInfo &stream)
{
    return {
        {QStringLiteral("file_name"), stream.fileName},
        {QStringLiteral("absolute_file_path"), QDir::toNativeSeparators(stream.absoluteFilePath)},
        {QStringLiteral("size_bytes"), static_cast<double>(stream.sizeBytes)},
        {QStringLiteral("codec_name"), stream.codecName},
        {QStringLiteral("pixel_format"), stream.pixelFormatName},
        {QStringLiteral("width"), stream.width},
        {QStringLiteral("height"), stream.height},
        {QStringLiteral("frame_rate"), stream.frameRate},
        {QStringLiteral("duration_us"), static_cast<double>(stream.durationUs)},
        {QStringLiteral("bit_rate"), static_cast<double>(stream.bitRate)}
    };
}

QJsonObject naluToJson(const NaluInfo &nalu)
{
    QJsonObject result {
        {QStringLiteral("offset"), static_cast<double>(nalu.offset)},
        {QStringLiteral("size"), static_cast<double>(nalu.size)},
        {QStringLiteral("nal_ref_idc"), nalu.nalRefIdc},
        {QStringLiteral("nal_unit_type"), nalu.nalUnitType},
        {QStringLiteral("nal_unit_type_name"), nalu.nalUnitTypeName}
    };
    if (nalu.sps.valid) {
        result.insert(QStringLiteral("sps"), spsToJson(nalu.sps));
    }
    if (nalu.pps.valid) {
        result.insert(QStringLiteral("pps"), ppsToJson(nalu.pps));
    }
    return result;
}

QJsonObject sliceToJson(const SliceInfo &slice)
{
    QJsonArray macroblocks;
    for (const MacroblockInfo &mb : slice.macroblocks) {
        macroblocks.append(macroblockToJson(mb));
    }

    QJsonArray diagnostics;
    for (const ParserDiagnosticInfo &diagnostic : slice.diagnostics) {
        diagnostics.append(diagnosticToJson(diagnostic));
    }

    QJsonArray warnings;
    for (const QString &warning : slice.macroblockParseWarnings) {
        warnings.append(warning);
    }

    return {
        {QStringLiteral("slice_type"), slice.sliceType},
        {QStringLiteral("slice_type_name"), slice.sliceTypeName},
        {QStringLiteral("first_mb_in_slice"), slice.firstMbInSlice},
        {QStringLiteral("pic_parameter_set_id"), slice.picParameterSetId},
        {QStringLiteral("frame_num"), slice.frameNum},
        {QStringLiteral("pic_order_cnt_lsb"), slice.picOrderCntLsb},
        {QStringLiteral("slice_qp_delta"), slice.sliceQpDelta},
        {QStringLiteral("derived_qp"), slice.derivedQp},
        {QStringLiteral("pic_width_in_mbs"), slice.picWidthInMbs},
        {QStringLiteral("pic_height_in_mbs"), slice.picHeightInMbs},
        {QStringLiteral("macroblocks_parsed"), slice.macroblocksParsed},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("macroblock_parse_warnings"), warnings},
        {QStringLiteral("macroblocks"), macroblocks}
    };
}

QJsonObject frameSyntaxToJson(const FrameSyntaxInfo &syntaxInfo)
{
    QJsonArray nalus;
    for (const NaluInfo &nalu : syntaxInfo.nalus) {
        nalus.append(naluToJson(nalu));
    }

    QJsonArray slices;
    for (const SliceInfo &slice : syntaxInfo.slices) {
        slices.append(sliceToJson(slice));
    }

    return {
        {QStringLiteral("index"), syntaxInfo.index},
        {QStringLiteral("codec"), codecKindName(syntaxInfo.codecKind)},
        {QStringLiteral("codec_name"), syntaxInfo.codecName},
        {QStringLiteral("pts"), static_cast<double>(syntaxInfo.pts)},
        {QStringLiteral("dts"), static_cast<double>(syntaxInfo.dts)},
        {QStringLiteral("poc"), syntaxInfo.poc},
        {QStringLiteral("frame_num"), syntaxInfo.frameNum},
        {QStringLiteral("frame_type"), syntaxInfo.frameType},
        {QStringLiteral("nalus"), nalus},
        {QStringLiteral("slices"), slices}
    };
}

QJsonObject bitFieldToJson(const AnalysisBitField &field)
{
    return {
        {QStringLiteral("path"), field.path},
        {QStringLiteral("name"), field.name},
        {QStringLiteral("bit_offset"), static_cast<double>(field.bitOffset)},
        {QStringLiteral("bit_length"), static_cast<double>(field.bitLength)},
        {QStringLiteral("value"), field.value}
    };
}

QJsonObject frameAnalysisToJson(const FrameAnalysis &analysis)
{
    QJsonArray units;
    for (const AnalysisUnit &unit : analysis.units) {
        units.append(QJsonObject {
            {QStringLiteral("kind"), analysisUnitKindName(unit.kind)},
            {QStringLiteral("offset"), static_cast<double>(unit.offset)},
            {QStringLiteral("size"), static_cast<double>(unit.size)},
            {QStringLiteral("type"), unit.type},
            {QStringLiteral("type_name"), unit.typeName}
        });
    }

    QJsonArray parameterSets;
    for (const AnalysisParameterSet &parameterSet : analysis.parameterSets) {
        QJsonArray fields;
        for (const AnalysisBitField &field : parameterSet.bitFields) {
            fields.append(bitFieldToJson(field));
        }
        parameterSets.append(QJsonObject {
            {QStringLiteral("kind"), parameterSet.kind},
            {QStringLiteral("id"), parameterSet.id},
            {QStringLiteral("summary"), parameterSet.summary},
            {QStringLiteral("bit_fields"), fields}
        });
    }

    QJsonArray regions;
    for (const AnalysisRegion &region : analysis.regions) {
        regions.append(QJsonObject {
            {QStringLiteral("kind"), analysisRegionKindName(region.kind)},
            {QStringLiteral("address"), region.address},
            {QStringLiteral("x"), region.x},
            {QStringLiteral("y"), region.y},
            {QStringLiteral("width"), region.width},
            {QStringLiteral("height"), region.height},
            {QStringLiteral("qp"), region.qp},
            {QStringLiteral("type"), region.type},
            {QStringLiteral("prediction_mode"), region.predictionMode},
            {QStringLiteral("parsed"), region.parsed},
            {QStringLiteral("skipped"), region.skipped},
            {QStringLiteral("note"), region.note}
        });
    }

    QJsonArray motionVectors;
    for (const AnalysisMotionVector &mv : analysis.motionVectors) {
        motionVectors.append(QJsonObject {
            {QStringLiteral("region_address"), mv.regionAddress},
            {QStringLiteral("list"), mv.list},
            {QStringLiteral("reference_index"), mv.referenceIndex},
            {QStringLiteral("source_x"), mv.sourceX},
            {QStringLiteral("source_y"), mv.sourceY},
            {QStringLiteral("reference_x"), mv.referenceX},
            {QStringLiteral("reference_y"), mv.referenceY},
            {QStringLiteral("mv_x_quarter_pel"), mv.mvXQuarterPel},
            {QStringLiteral("mv_y_quarter_pel"), mv.mvYQuarterPel}
        });
    }

    QJsonArray diagnostics;
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        diagnostics.append(QJsonObject {
            {QStringLiteral("path"), diagnostic.path},
            {QStringLiteral("code"), diagnostic.code},
            {QStringLiteral("message"), diagnostic.message},
            {QStringLiteral("severity"), diagnostic.severity}
        });
    }

    QJsonArray bitFields;
    for (const AnalysisBitField &field : analysis.bitFields) {
        bitFields.append(bitFieldToJson(field));
    }

    return {
        {QStringLiteral("index"), analysis.frameIndex},
        {QStringLiteral("codec"), codecKindName(analysis.codecKind)},
        {QStringLiteral("codec_name"), analysis.codecName},
        {QStringLiteral("pts"), static_cast<double>(analysis.pts)},
        {QStringLiteral("dts"), static_cast<double>(analysis.dts)},
        {QStringLiteral("poc"), analysis.poc},
        {QStringLiteral("frame_num"), analysis.frameNum},
        {QStringLiteral("frame_type"), analysis.frameType},
        {QStringLiteral("has_frame"), analysis.hasFrame},
        {QStringLiteral("units"), units},
        {QStringLiteral("parameter_sets"), parameterSets},
        {QStringLiteral("regions"), regions},
        {QStringLiteral("motion_vectors"), motionVectors},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("bit_fields"), bitFields}
    };
}

QJsonObject selectedFrameExportToJson(const StreamInfo &stream, const FrameAnalysis &analysis)
{
    const FrameSyntaxInfo syntaxInfo = h264SyntaxFromFrameAnalysis(analysis);
    return {
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("generator"), QCoreApplication::applicationName()},
        {QStringLiteral("generator_version"), QCoreApplication::applicationVersion()},
        {QStringLiteral("stream"), streamInfoToJson(stream)},
        {QStringLiteral("frame_analysis"), frameAnalysisToJson(analysis)},
        {QStringLiteral("frame"), frameSyntaxToJson(syntaxInfo)}
    };
}

QJsonObject allFramesExportToJson(const StreamInfo &stream, const QVector<FrameAnalysis> &frames)
{
    QJsonArray frameArray;
    for (const FrameAnalysis &frame : frames) {
        if (frame.frameIndex >= 0) {
            QJsonObject frameObject = frameAnalysisToJson(frame);
            frameObject.insert(QStringLiteral("h264"), frameSyntaxToJson(h264SyntaxFromFrameAnalysis(frame)));
            frameArray.append(frameObject);
        }
    }

    return {
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("generator"), QCoreApplication::applicationName()},
        {QStringLiteral("generator_version"), QCoreApplication::applicationVersion()},
        {QStringLiteral("stream"), streamInfoToJson(stream)},
        {QStringLiteral("frames"), frameArray}
    };
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<StreamInfo>("StreamInfo");
    qRegisterMetaType<DecodedVideoFramePtr>("DecodedVideoFramePtr");
    qRegisterMetaType<FrameSyntaxInfo>("FrameSyntaxInfo");
    qRegisterMetaType<FrameAnalysis>("FrameAnalysis");
    qRegisterMetaType<FrameSeekCheckpoint>("FrameSeekCheckpoint");

    setWindowTitle(tr("H.264 Analyzer"));
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

    m_exportFrameSyntaxJsonAction = new QAction(tr("Export Frame Syntax JSON"), this);
    connect(m_exportFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportFrameSyntaxJson);

    m_exportAllFrameSyntaxJsonAction = new QAction(tr("Export All Decoded Frame Syntax JSON"), this);
    connect(m_exportAllFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportAllFrameSyntaxJson);

    m_exportFrameListCsvAction = new QAction(tr("Export Frame List CSV"), this);
    connect(m_exportFrameListCsvAction, &QAction::triggered, this, &MainWindow::exportFrameListCsv);

    m_exportScreenshotAction = new QAction(tr("Export Screenshot"), this);
    connect(m_exportScreenshotAction, &QAction::triggered, this, &MainWindow::exportScreenshot);

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
    m_videoCanvas->setShowGrid(m_showGridAction->isChecked());
    m_videoCanvas->setShowQpHeatmap(m_showQpHeatmapAction->isChecked());
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
    fileMenu->addAction(m_exportFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportAllFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportFrameListCsvAction);
    fileMenu->addAction(m_exportScreenshotAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_docksMenu = viewMenu->addMenu(tr("&Docks"));
    m_docksMenu->addAction(m_frameDock->toggleViewAction());
    m_docksMenu->addAction(m_propertyDock->toggleViewAction());
    m_docksMenu->addAction(m_logDockWidget->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_showGridAction);
    viewMenu->addAction(m_showQpHeatmapAction);
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
    m_videoCanvas->setAnalysisOverlay(FrameAnalysis {});
    m_frameListView->clearFrames();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_frameCache.clear();
    m_frameAnalysisByIndex.clear();
    m_seekCheckpoints.clear();
    m_currentFrameIndex = -1;
    m_latestFrameIndex = -1;
    m_playbackPaused = false;
    updateFrameIndexDisplay();
    updateExportActionState();

    startDecoder(stream.absoluteFilePath);
}

void MainWindow::startDecoder(const QString &filePath,
                              int startFrameIndex,
                              bool pauseAfterFirstFrame,
                              const FrameSeekCheckpoint &seekCheckpoint)
{
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
    connect(m_decodeWorker, &DecodeWorker::seekCheckpointReady,
            this, &MainWindow::handleSeekCheckpoint,
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

void MainWindow::exportFrameSyntaxJson()
{
    const CachedFrame *cached = currentCachedFrame();
    if (cached == nullptr) {
        const QString message = tr("No cached selected frame is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString defaultName = QStringLiteral("frame-%1-syntax.json").arg(cached->index, 5, 10, QLatin1Char('0'));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Frame Syntax JSON"),
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

    const QJsonDocument document(selectedFrameExportToJson(m_document.streamInfo(), cached->analysis));
    file.write(document.toJson(QJsonDocument::Indented));
    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported frame syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported frame syntax JSON"), 3000);
}

void MainWindow::exportAllFrameSyntaxJson()
{
    if (m_frameAnalysisByIndex.isEmpty()) {
        const QString message = tr("No decoded frame syntax is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export All Decoded Frame Syntax JSON"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("decoded-frame-syntax.json")),
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

    const QJsonDocument document(allFramesExportToJson(m_document.streamInfo(), m_frameAnalysisByIndex));
    file.write(document.toJson(QJsonDocument::Indented));
    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported all decoded frame syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported decoded frame syntax JSON"), 3000);
}

void MainWindow::exportFrameListCsv()
{
    if (m_frameAnalysisByIndex.isEmpty()) {
        const QString message = tr("No frame list is available to export.");
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Warning] %1").arg(message));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Frame List CSV"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("frame-list.csv")),
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
    out << "index,type,poc,frame_num\n";
    for (const FrameAnalysis &analysis : std::as_const(m_frameAnalysisByIndex)) {
        if (analysis.frameIndex < 0) {
            continue;
        }
        out << csvEscape(QString::number(analysis.frameIndex)) << ','
            << csvEscape(analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType) << ','
            << csvEscape(analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-")) << ','
            << csvEscape(analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-")) << '\n';
    }

    m_lastExportDirectory = QFileInfo(filePath).absolutePath();
    m_logDock->appendLine(tr("[Info] Exported frame list CSV: %1").arg(QDir::toNativeSeparators(filePath)));
    statusBar()->showMessage(tr("Exported frame list CSV"), 3000);
}

void MainWindow::exportScreenshot()
{
    if (m_videoCanvas == nullptr) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Screenshot"),
        QDir(defaultExportDirectory()).filePath(QStringLiteral("h264-analyzer-screenshot.png")),
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

    if (frameIndex >= 0) {
        if (frameIndex >= m_frameAnalysisByIndex.size()) {
            m_frameAnalysisByIndex.resize(frameIndex + 1);
        }
        m_frameAnalysisByIndex[frameIndex] = analysis;
    }

    m_latestFrameIndex = std::max(m_latestFrameIndex, frameIndex);
    m_frameListView->addFrameAnalysis(analysis);
    showFrameFromCache(frameIndex, true, m_playbackPaused);
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

void MainWindow::handleFrameListSelection(int frameIndex)
{
    if (!m_decodeWorker.isNull()) {
        pausePlayback();
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
        m_videoCanvas->setFrame(cached.frame);
        m_videoCanvas->setAnalysisOverlay(cached.analysis);
        if (updatePropertyTree) {
            m_propertyTreeView->showFrameAnalysis(cached.analysis);
        }
        if (selectInList) {
            m_frameListView->selectFrameIndex(frameIndex);
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
    updatePlaybackActionState();
    setPlaybackControlsEnabled(false);
    m_videoCanvas->setOverlayMessage(tr("Buffering to frame %1...").arg(frameIndex + 1));
    statusBar()->showMessage(tr("Buffering to frame %1").arg(frameIndex + 1), 3000);

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

void MainWindow::updateExportActionState()
{
    const bool hasCurrentFrame = currentCachedFrame() != nullptr;
    const bool hasDecodedSyntax = !m_frameAnalysisByIndex.isEmpty();

    if (m_exportFrameSyntaxJsonAction != nullptr) {
        m_exportFrameSyntaxJsonAction->setEnabled(hasCurrentFrame);
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
