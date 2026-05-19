#pragma once

#include "core/CodecKind.h"
#include "core/MediaTypes.h"

#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>

enum class AnalysisUnitKind
{
    Unknown,
    Nalu,
    Obu,
    TileGroup,
    AdtsFrame,
    Mp3Frame
};

enum class AnalysisRegionKind
{
    Unknown,
    Macroblock,
    Ctu,
    Superblock
};

struct AnalysisBitField
{
    QString path;
    QString name;
    qsizetype bitOffset = 0;
    qsizetype bitLength = 0;
    QString value;
};

struct AnalysisDiagnostic
{
    QString path;
    QString code;
    QString message;
    QString severity = QStringLiteral("warning");
};

struct AnalysisUnit
{
    AnalysisUnitKind kind = AnalysisUnitKind::Unknown;
    qsizetype offset = 0;
    qsizetype size = 0;
    int type = -1;
    QString typeName;
};

struct AnalysisParameterSet
{
    QString kind;
    int id = -1;
    QString summary;
    QVector<AnalysisBitField> bitFields;
};

struct AnalysisRegion
{
    AnalysisRegionKind kind = AnalysisRegionKind::Unknown;
    int address = -1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int qp = -1;
    QString type;
    QString predictionMode;
    bool parsed = false;
    bool skipped = false;
    QString note;
};

struct AnalysisMotionVector
{
    int regionAddress = -1;
    int list = 0;
    int referenceIndex = 0;
    int sourceX = 0;
    int sourceY = 0;
    int referenceX = -1;
    int referenceY = -1;
    int mvXQuarterPel = 0;
    int mvYQuarterPel = 0;
};

struct FrameAnalysis
{
    int frameIndex = -1;
    int streamIndex = -1;
    MediaKind mediaKind = MediaKind::Video;
    AccessUnitKind accessUnitKind = AccessUnitKind::VideoFrame;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName = QStringLiteral("Unknown");
    qint64 pts = 0;
    qint64 dts = 0;
    int poc = -1;
    int frameNum = -1;
    QString frameType;
    bool hasFrame = false;
    QVector<AnalysisUnit> units;
    QVector<AnalysisParameterSet> parameterSets;
    QVector<AnalysisRegion> regions;
    QVector<AnalysisMotionVector> motionVectors;
    QVector<AnalysisDiagnostic> diagnostics;
    QVector<AnalysisBitField> bitFields;
    QVariant codecSpecificDetails;
};

QString analysisUnitKindName(AnalysisUnitKind kind);
QString analysisRegionKindName(AnalysisRegionKind kind);

Q_DECLARE_METATYPE(AnalysisBitField)
Q_DECLARE_METATYPE(AnalysisDiagnostic)
Q_DECLARE_METATYPE(AnalysisUnit)
Q_DECLARE_METATYPE(AnalysisParameterSet)
Q_DECLARE_METATYPE(AnalysisRegion)
Q_DECLARE_METATYPE(AnalysisMotionVector)
Q_DECLARE_METATYPE(FrameAnalysis)
