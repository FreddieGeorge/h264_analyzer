#pragma once

#include "core/parser/BitstreamParser.h"

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

struct SyntaxFieldInfo
{
    QString name;
    qsizetype bitOffset = 0;
    qsizetype bitLength = 0;
    QString value;
    QVector<AnalysisBitRange> packetBitRanges;
};

struct ParserDiagnosticInfo
{
    QString code;
    QString message;
};

struct SpsInfo
{
    bool valid = false;
    int profileIdc = 0;
    bool constraintSet0Flag = false;
    bool constraintSet1Flag = false;
    bool constraintSet2Flag = false;
    bool constraintSet3Flag = false;
    bool constraintSet4Flag = false;
    bool constraintSet5Flag = false;
    int reservedZero2Bits = 0;
    int levelIdc = 0;
    int seqParameterSetId = -1;
    int chromaFormatIdc = 1;
    int log2MaxFrameNumMinus4 = 0;
    int picOrderCntType = 0;
    int log2MaxPicOrderCntLsbMinus4 = 0;
    bool deltaPicOrderAlwaysZeroFlag = false;
    int picWidthInMbsMinus1 = 0;
    int picHeightInMapUnitsMinus1 = 0;
    bool frameMbsOnlyFlag = true;
    int frameCropLeftOffset = 0;
    int frameCropRightOffset = 0;
    int frameCropTopOffset = 0;
    int frameCropBottomOffset = 0;
    int width = 0;
    int height = 0;
    bool vuiParametersPresentFlag = false;
    bool aspectRatioInfoPresentFlag = false;
    int aspectRatioIdc = -1;
    int sarWidth = 0;
    int sarHeight = 0;
    bool timingInfoPresentFlag = false;
    quint32 numUnitsInTick = 0;
    quint32 timeScale = 0;
    bool fixedFrameRateFlag = false;
    bool bitstreamRestrictionFlag = false;
    bool motionVectorsOverPicBoundariesFlag = false;
    int maxBytesPerPicDenom = -1;
    int maxBitsPerMbDenom = -1;
    int log2MaxMvLengthHorizontal = -1;
    int log2MaxMvLengthVertical = -1;
    int maxNumReorderFrames = -1;
    int maxDecFrameBuffering = -1;
    QVector<SyntaxFieldInfo> fields;
};

struct PpsInfo
{
    bool valid = false;
    int picParameterSetId = -1;
    int seqParameterSetId = -1;
    bool entropyCodingModeFlag = false;
    bool bottomFieldPicOrderInFramePresentFlag = false;
    int numSliceGroupsMinus1 = 0;
    int numRefIdxL0DefaultActiveMinus1 = 0;
    int numRefIdxL1DefaultActiveMinus1 = 0;
    bool weightedPredFlag = false;
    int weightedBipredIdc = 0;
    int picInitQpMinus26 = 0;
    bool deblockingFilterControlPresentFlag = true;
    bool constrainedIntraPredFlag = false;
    bool redundantPicCntPresentFlag = false;
    bool transform8x8ModeFlag = false;
    bool picScalingMatrixPresentFlag = false;
    int secondChromaQpIndexOffset = 0;
    QVector<SyntaxFieldInfo> fields;
};

struct MotionVectorInfo
{
    int list = 0;
    int referenceIndex = 0;
    int mvXQuarterPel = 0;
    int mvYQuarterPel = 0;
    int referenceX = -1;
    int referenceY = -1;
};

struct MacroblockInfo
{
    int address = 0;
    QString mbType;
    QString predictionMode;
    int codedBlockPattern = -1;
    int codedBlockPatternLuma = -1;
    int codedBlockPatternChroma = -1;
    int qp = 26;
    int mbQpDelta = 0;
    int residualBlockCount = 0;
    int residualCoefficientCount = 0;
    bool residualParsed = false;
    bool skipped = false;
    bool parsed = false;
    QString note;
    QVector<MotionVectorInfo> motionVectors;
    QVector<SyntaxFieldInfo> fields;
};

struct SliceInfo
{
    bool valid = false;
    int nalUnitType = 0;
    int nalRefIdc = 0;
    int firstMbInSlice = 0;
    int sliceType = 0;
    QString sliceTypeName;
    int picParameterSetId = -1;
    int frameNum = 0;
    int idrPicId = -1;
    int picOrderCntLsb = -1;
    bool fieldPicFlag = false;
    bool bottomFieldFlag = false;
    bool directSpatialMvPredFlag = false;
    bool numRefIdxActiveOverrideFlag = false;
    int numRefIdxL0ActiveMinus1 = 0;
    int numRefIdxL1ActiveMinus1 = 0;
    bool refPicListModificationFlagL0 = false;
    bool refPicListModificationFlagL1 = false;
    QString refPicListModificationSummary;
    bool predWeightTablePresent = false;
    QString predWeightTableSummary;
    bool decRefPicMarkingPresent = false;
    bool noOutputOfPriorPicsFlag = false;
    bool longTermReferenceFlag = false;
    bool adaptiveRefPicMarkingModeFlag = false;
    QString decRefPicMarkingSummary;
    int sliceQpDelta = 0;
    int derivedQp = 26;
    int picWidthInMbs = 0;
    int picHeightInMbs = 0;
    bool macroblocksParsed = false;
    QVector<ParserDiagnosticInfo> diagnostics;
    QStringList macroblockParseWarnings;
    QVector<SyntaxFieldInfo> fields;
    QVector<MacroblockInfo> macroblocks;
};

struct NaluInfo
{
    qsizetype offset = 0;
    qsizetype size = 0;
    int forbiddenZeroBit = 0;
    int nalRefIdc = 0;
    int nalUnitType = 0;
    QString nalUnitTypeName;
    SpsInfo sps;
    PpsInfo pps;
    SliceInfo slice;
    QVector<ParserDiagnosticInfo> diagnostics;
};

struct FrameSyntaxInfo
{
    int index = -1;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName = QStringLiteral("Unknown");
    qint64 pts = 0;
    qint64 dts = 0;
    int poc = -1;
    int frameNum = -1;
    QString frameType;
    QVector<ParserDiagnosticInfo> diagnostics;
    QVector<NaluInfo> nalus;
    QVector<SliceInfo> slices;
};

struct H264ParserState : public BitstreamParserState
{
    QHash<int, SpsInfo> spsById;
    QHash<int, PpsInfo> ppsById;
    int nalLengthSize = 4;
};

class H264Parser : public IBitstreamParser
{
public:
    H264Parser();

    CodecKind codecKind() const override;
    void reset() override;
    void parseDecoderConfigurationRecord(const QByteArray &extraData) override;
    void setParameterSets(const QHash<int, SpsInfo> &spsById, const QHash<int, PpsInfo> &ppsById);
    FrameAnalysis parsePacket(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex) override;
    FrameSyntaxInfo parsePacketSyntax(const QByteArray &packetData, qint64 pts, qint64 dts, int packetIndex);
    BitstreamParserStatePtr snapshotState() const override;
    void restoreState(const BitstreamParserStatePtr &state) override;

    const QHash<int, SpsInfo> &spsMap() const;
    const QHash<int, PpsInfo> &ppsMap() const;
    int nalLengthSize() const;

    static QString naluTypeName(int nalUnitType);
    static QString sliceTypeName(int sliceType);

#ifdef H264_ANALYZER_ENABLE_TESTS
    static quint32 decodeUnsignedExpGolombForTest(const QByteArray &data, bool *ok = nullptr);
    static qint32 decodeSignedExpGolombForTest(const QByteArray &data, bool *ok = nullptr);
#endif

private:
    class BitReader;

    QVector<NaluInfo> splitNalus(const QByteArray &packetData, QVector<ParserDiagnosticInfo> *diagnostics);
    QVector<NaluInfo> splitAnnexBNalus(const QByteArray &packetData);
    QVector<NaluInfo> splitLengthPrefixedNalus(const QByteArray &packetData, QVector<ParserDiagnosticInfo> *diagnostics);
    NaluInfo parseNaluPayload(const NaluInfo &base, const QByteArray &nalu);
    SpsInfo parseSps(const QByteArray &rbsp) const;
    PpsInfo parsePps(const QByteArray &rbsp) const;
    SliceInfo parseSliceHeader(const QByteArray &rbsp, int nalUnitType, int nalRefIdc) const;
    void parseSliceData(BitReader &reader, SliceInfo &slice, const PpsInfo &pps, const SpsInfo &sps) const;

    static QByteArray rbspFromEbsp(const uint8_t *data, qsizetype size);
    static bool hasAnnexBStartCode(const QByteArray &data);
    static qsizetype startCodeSizeAt(const QByteArray &data, qsizetype offset);
    static int readBigEndianLength(const uint8_t *data, int size);
    static void skipScalingList(BitReader &reader, int sizeOfScalingList);
    static int codedBlockPatternFromCodeNum(quint32 codeNum, bool intra, int chromaArrayType);
    static QString intraMbTypeName(int mbType);
    static QString pMbTypeName(int mbType);
    static QString bMbTypeName(int mbType);

    QHash<int, SpsInfo> m_spsById;
    QHash<int, PpsInfo> m_ppsById;
    int m_nalLengthSize = 4;
};

Q_DECLARE_METATYPE(SpsInfo)
Q_DECLARE_METATYPE(PpsInfo)
Q_DECLARE_METATYPE(SyntaxFieldInfo)
Q_DECLARE_METATYPE(ParserDiagnosticInfo)
Q_DECLARE_METATYPE(MotionVectorInfo)
Q_DECLARE_METATYPE(MacroblockInfo)
Q_DECLARE_METATYPE(SliceInfo)
Q_DECLARE_METATYPE(NaluInfo)
Q_DECLARE_METATYPE(FrameSyntaxInfo)
