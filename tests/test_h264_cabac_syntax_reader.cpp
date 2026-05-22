#include "core/parser/video/h264/cabac/H264CabacDecoder.h"
#include "core/parser/video/h264/cabac/H264CabacMacroblockParser.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxReader.h"
#include "core/parser/video/h264/H264SliceDataContext.h"

#include <QByteArray>

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void initializeBasicSlice(SliceInfo &slice, int sliceType, int firstMbInSlice)
{
    slice.sliceType = sliceType;
    slice.firstMbInSlice = firstMbInSlice;
    slice.picWidthInMbs = 2;
    slice.picHeightInMbs = 2;
    slice.derivedQp = 26;
    slice.cabacInitIdc = 0;
}

void initializeBasicSps(SpsInfo &sps)
{
    sps.frameMbsOnlyFlag = true;
    sps.chromaFormatIdc = 1;
}

H264CabacDecoder initializedDecoder(BitReader &reader)
{
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC syntax reader decoder initialization");
    return decoder;
}

void setCbpZeroContexts(H264CabacContextModelSet &contexts)
{
    for (int ctxIdx = 73; ctxIdx <= 77; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
}

void testReadPSliceMbSkipFlag()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_skip_flag result");
    require(result.value == 0 || result.value == 1, "CABAC P mb_skip_flag bin value is binary");
    require(result.ctxIdx == 11, "CABAC P mb_skip_flag first macroblock ctxIdx");
    require(contexts.model(11).stateIndex !=
                H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 26, 23).model(11).stateIndex,
            "CABAC P mb_skip_flag updates context state");
}

void testReadPSliceMbSkipFlagUsesLeftNeighbor()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 1);
    initializeBasicSps(sps);
    MacroblockInfo left;
    left.address = 0;
    left.skipped = false;
    slice.macroblocks.append(left);

    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_skip_flag with left neighbor");
    require(result.ctxIdx == 12, "CABAC P mb_skip_flag left neighbor ctxIdx increment");
}

void testReadPSliceMbTypeP16x16()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_16x16 result");
    require(result.complete, "CABAC P mb_type P_L0_16x16 is complete");
    require(result.mbType == 0, "CABAC P mb_type P_L0_16x16 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_16x16 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_16x16 ctxIdx");
}

void testReadPSliceMbTypeP16x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(18);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 1});
    contexts.setModel(17, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_L0_16x8 result");
    require(result.complete, "CABAC P mb_type P_L0_L0_16x8 is complete");
    require(result.mbType == 1, "CABAC P mb_type P_L0_L0_16x8 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_L0_16x8 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_L0_16x8 ctxIdx");
}

void testReadPSliceMbTypeP8x16()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(18);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 1});
    contexts.setModel(17, {0, 0});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_L0_8x16 result");
    require(result.complete, "CABAC P mb_type P_L0_L0_8x16 is complete");
    require(result.mbType == 2, "CABAC P mb_type P_L0_L0_8x16 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_L0_8x16 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_L0_8x16 ctxIdx");
}

void testReadPSliceMbTypeP8x8RequiresSubMbType()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(17);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_8x8 branch result");
    require(result.complete, "CABAC P mb_type P_8x8 branch completes mb_type");
    require(result.needsSubMacroblockTypes, "CABAC P mb_type P_8x8 requires sub_mb_type");
    require(result.mbType == 3, "CABAC P mb_type P_8x8 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_8x8 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_8x8 ctxIdx");
}

void testReadPSubMbTypeP8x8()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_8x8 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_8x8 is complete");
    require(result.subMbType == 0, "CABAC P sub_mb_type P_L0_8x8 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_8x8 ctxIdx");
}

void testReadPSubMbTypeReportsMissingContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 1});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(!result.ok, "CABAC P sub_mb_type missing context fails cleanly");
    require(!result.complete, "CABAC P sub_mb_type missing context remains incomplete");
    require(result.ctxIdx == 22, "CABAC P sub_mb_type missing context ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC P sub_mb_type missing context diagnostic");
}

void testReadPSubMbTypeP8x4()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 1});
    contexts.setModel(22, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_8x4 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_8x4 is complete");
    require(result.subMbType == 1, "CABAC P sub_mb_type P_L0_8x4 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_8x4 ctxIdx");
}

void testReadPSubMbTypeP4x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 1});
    contexts.setModel(22, {0, 1});
    contexts.setModel(23, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_4x8 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_4x8 is complete");
    require(result.subMbType == 2, "CABAC P sub_mb_type P_L0_4x8 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_4x8 ctxIdx");
}

void testReadPSubMbTypeP4x4()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 1});
    contexts.setModel(22, {0, 1});
    contexts.setModel(23, {0, 1});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_4x4 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_4x4 is complete");
    require(result.subMbType == 3, "CABAC P sub_mb_type P_L0_4x4 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_4x4 ctxIdx");
}

void testReadFourPSubMbTypes()
{
    BitReader reader(QByteArray::fromHex("0000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 0});
    contexts.setModel(22, {0, 0});
    contexts.setModel(23, {0, 0});

    const H264CabacSubMbTypesResult result =
        h264ReadCabacPSubMbTypes(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P reads four sub_mb_type values");
    require(result.complete, "CABAC P four sub_mb_type values complete");
    require(result.subMbTypes.size() == 4, "CABAC P four sub_mb_type count");
    require(result.subMbTypes[0] == 0, "CABAC P sub_mb_type[0]");
    require(result.subMbTypes[1] == 0, "CABAC P sub_mb_type[1]");
    require(result.subMbTypes[2] == 0, "CABAC P sub_mb_type[2]");
    require(result.subMbTypes[3] == 0, "CABAC P sub_mb_type[3]");
}

void testReadFourPSubMbTypesReportsIndexedFailure()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 1});

    const H264CabacSubMbTypesResult result =
        h264ReadCabacPSubMbTypes(reader, decoder, contexts, context, 4);
    require(!result.ok, "CABAC P four sub_mb_type reports indexed failure");
    require(!result.complete, "CABAC P four sub_mb_type indexed failure remains incomplete");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC P four sub_mb_type indexed failure diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("sub_mb_type[0]")),
            "CABAC P four sub_mb_type indexed failure message");
}

void testReadPSubMbRefIdxNotPresent()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 59);

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx not present result");
    require(!result.present, "CABAC P sub_mb ref_idx not present flag");
    require(result.refIdx.size() == 4, "CABAC P sub_mb ref_idx not present count");
    require(result.refIdx[0] == 0 && result.refIdx[3] == 0,
            "CABAC P sub_mb ref_idx not present defaults to zero");
}

void testReadPSubMbRefIdxZero()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 59);

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx zero result");
    require(result.present, "CABAC P sub_mb ref_idx present flag");
    require(result.refIdx.size() == 4, "CABAC P sub_mb ref_idx zero count");
    require(result.refIdx[0] == 0 && result.refIdx[1] == 0 && result.refIdx[2] == 0 && result.refIdx[3] == 0,
            "CABAC P sub_mb ref_idx zero values");
}

void testReadPSubMbRefIdxIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(54, {0, 1});

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx incomplete result");
    require(!result.refIdx.size(), "CABAC P sub_mb ref_idx incomplete stops before append");
    require(result.diagnosticCode == QStringLiteral("cabac_ref_idx_incomplete"),
            "CABAC P sub_mb ref_idx incomplete diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("ref_idx_l0[0]")),
            "CABAC P sub_mb ref_idx incomplete indexed message");
}

void testReadMvdComponentZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0);
    require(result.ok, "CABAC mvd_l0 component zero result");
    require(result.complete, "CABAC mvd_l0 component zero complete");
    require(result.value == 0, "CABAC mvd_l0 component zero value");
    require(result.ctxIdx == 40, "CABAC mvd_l0 x context");
}

void testReadMvdComponentAbs2Positive()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(41, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0, 1);
    require(result.ok, "CABAC mvd_l0 component abs2 positive result");
    require(result.complete, "CABAC mvd_l0 component abs2 positive complete");
    require(result.value == 2, "CABAC mvd_l0 component abs2 positive value");
    require(result.ctxIdx == 41, "CABAC mvd_l0 abs2 positive derived context");
}

void testReadMvdComponentAbs3Positive()
{
    BitReader reader(QByteArray::fromHex("270000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(41, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0, 1);
    require(result.ok, "CABAC mvd_l0 component abs3 positive result");
    require(result.complete, "CABAC mvd_l0 component abs3 positive complete");
    require(result.value == 3, "CABAC mvd_l0 component abs3 positive value");
    require(result.ctxIdx == 41, "CABAC mvd_l0 abs3 positive derived context");
}

void testReadMvdComponentGreaterThanThreeIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(47, {0, 1});
    contexts.setModel(50, {0, 1});
    contexts.setModel(51, {0, 1});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 1);
    require(result.ok, "CABAC mvd_l0 component greater-than-three prefix result");
    require(!result.complete, "CABAC mvd_l0 component greater-than-three incomplete");
    require(result.ctxIdx == 47, "CABAC mvd_l0 y context");
    require(result.diagnosticCode == QStringLiteral("cabac_mvd_incomplete"),
            "CABAC mvd_l0 component greater-than-three diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("greater than three")),
            "CABAC mvd_l0 component greater-than-three message");
}

void testReadPSubMbMvdZero8x8()
{
    BitReader reader(QByteArray::fromHex("00000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {0, 0, 0, 0});
    require(result.ok, "CABAC P sub_mb mvd zero 8x8 result");
    require(result.complete, "CABAC P sub_mb mvd zero 8x8 complete");
    require(result.mvd.size() == 4, "CABAC P sub_mb mvd zero 8x8 pair count");
    require(result.mvd[0].x == 0 && result.mvd[0].y == 0, "CABAC P sub_mb mvd zero first pair");
}

void testReadPSubMbMvdAbs2_8x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {0});
    require(result.ok, "CABAC P sub_mb mvd abs2 8x8 result");
    require(result.complete, "CABAC P sub_mb mvd abs2 8x8 complete");
    require(result.mvd.size() == 1, "CABAC P sub_mb mvd abs2 8x8 pair count");
    require(result.mvd[0].x == 2 && result.mvd[0].y == 0,
            "CABAC P sub_mb mvd abs2 8x8 pair value");
}

void testReadPSubMbMvdZero4x4()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {3, 3, 3, 3});
    require(result.ok, "CABAC P sub_mb mvd zero 4x4 result");
    require(result.complete, "CABAC P sub_mb mvd zero 4x4 complete");
    require(result.mvd.size() == 16, "CABAC P sub_mb mvd zero 4x4 pair count");
    require(result.mvd[15].x == 0 && result.mvd[15].y == 0, "CABAC P sub_mb mvd zero last pair");
}

void testReadCabacMacroblockSyntaxP8x8RefAbsent()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(60, {0, 1});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 ref absent result");
    require(result.complete, "CABAC macroblock syntax P_8x8 ref absent complete");
    require(result.parsedSubMacroblockSyntax, "CABAC macroblock syntax P_8x8 sub syntax flag");
    require(result.parsedCodedBlockPatternZero, "CABAC macroblock syntax P_8x8 CBP zero flag");
    require(result.mbType == 3, "CABAC macroblock syntax P_8x8 mb_type");
    require(result.subMbTypes.size() == 4, "CABAC macroblock syntax P_8x8 sub_mb_type count");
    require(!result.refIdxL0Present, "CABAC macroblock syntax P_8x8 ref_idx_l0 absent");
    require(result.refIdxL0.size() == 4, "CABAC macroblock syntax P_8x8 default ref_idx count");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 mvd pair count");
    require(result.codedBlockPattern == 0, "CABAC macroblock syntax P_8x8 CBP value");
    require(contexts.model(60).stateIndex == 0 && contexts.model(60).valueMps == 1,
            "CABAC macroblock syntax P_8x8 CBP-zero does not consume absent mb_qp_delta");
}

void testReadCabacMacroblockSyntaxP8x8RefZero()
{
    BitReader reader(QByteArray::fromHex("00000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(54, {0, 0});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 ref zero result");
    require(result.complete, "CABAC macroblock syntax P_8x8 ref zero complete");
    require(result.parsedCodedBlockPatternZero, "CABAC macroblock syntax P_8x8 ref zero CBP zero flag");
    require(result.refIdxL0Present, "CABAC macroblock syntax P_8x8 ref_idx_l0 present");
    require(result.refIdxL0.size() == 4, "CABAC macroblock syntax P_8x8 ref_idx_l0 count");
    require(result.refIdxL0[0] == 0 && result.refIdxL0[3] == 0,
            "CABAC macroblock syntax P_8x8 ref_idx_l0 zero values");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 ref zero mvd pair count");
}

void testReadCabacMacroblockSyntaxP8x8ChromaAcResidualIncomplete()
{
    BitReader reader(QByteArray::fromHex("00000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 0});
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 1});
    contexts.setModel(97, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 chroma AC residual prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 chroma AC residual incomplete");
    require(result.parsedSubMacroblockSyntax, "CABAC macroblock syntax P_8x8 chroma AC residual keeps motion flag");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma AC residual keeps CBP");
    require(result.codedBlockPatternChroma == 2,
            "CABAC macroblock syntax P_8x8 chroma AC residual CBP value");
    require(result.residualChromaDcComponents.size() == 2,
            "CABAC macroblock syntax P_8x8 chroma AC residual preserves DC components");
    require(result.residualChromaDcCodedBlockFlags[0] == 0
                && result.residualChromaDcCodedBlockFlags[1] == 0,
            "CABAC macroblock syntax P_8x8 chroma AC residual preserves DC CBF values");
    require(result.residualIncompleteCategory == QStringLiteral("chroma_ac"),
            "CABAC macroblock syntax P_8x8 chroma AC residual incomplete category");
    require(result.residualIncompleteStage == QStringLiteral("coded_block_flag"),
            "CABAC macroblock syntax P_8x8 chroma AC residual incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 chroma AC residual diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("chroma AC")),
            "CABAC macroblock syntax P_8x8 chroma AC residual message");
}

void testReadCabacMacroblockSyntaxP8x8SingleLumaResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 single-luma residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 single-luma residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 single-luma CBP parsed");
    require(!result.parsedCodedBlockPatternZero,
            "CABAC macroblock syntax P_8x8 single-luma CBP non-zero flag");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 single-luma residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 8,
            "CABAC macroblock syntax P_8x8 single-luma CBP bit");
    require(result.mbQpDelta == 0, "CABAC macroblock syntax P_8x8 single-luma mb_qp_delta");
    require(result.residualLuma4x4BlockIndices.size() == 4,
            "CABAC macroblock syntax P_8x8 single-luma residual block count");
    require(result.residualLuma4x4BlockIndices[0] == 12
                && result.residualLuma4x4BlockIndices[3] == 15,
            "CABAC macroblock syntax P_8x8 single-luma residual block indices");
    require(result.residualCodedBlockFlags[0] == 0 && result.residualCodedBlockFlags[3] == 0,
            "CABAC macroblock syntax P_8x8 single-luma residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8MultiLumaResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 1});
    contexts.setModel(85, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 multi-luma residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 multi-luma residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 multi-luma CBP parsed");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 multi-luma residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 15,
            "CABAC macroblock syntax P_8x8 multi-luma CBP bits");
    require(result.residualLuma4x4BlockIndices.size() == 16,
            "CABAC macroblock syntax P_8x8 multi-luma residual block count");
    require(result.residualLuma4x4BlockIndices[0] == 0
                && result.residualLuma4x4BlockIndices[15] == 15,
            "CABAC macroblock syntax P_8x8 multi-luma residual block indices");
    require(result.residualCodedBlockFlags[0] == 0 && result.residualCodedBlockFlags[15] == 0,
            "CABAC macroblock syntax P_8x8 multi-luma residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 0});
    contexts.setModel(97, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 chroma DC residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 chroma DC residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma DC CBP parsed");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 chroma DC residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 0,
            "CABAC macroblock syntax P_8x8 chroma DC luma CBP zero");
    require(result.codedBlockPatternChroma == 1,
            "CABAC macroblock syntax P_8x8 chroma DC CBP value");
    require(result.codedBlockPattern == 16,
            "CABAC macroblock syntax P_8x8 chroma DC combined CBP value");
    require(result.residualChromaDcComponents.size() == 2,
            "CABAC macroblock syntax P_8x8 chroma DC residual component count");
    require(result.residualChromaDcComponents[0] == 0
                && result.residualChromaDcComponents[1] == 1,
            "CABAC macroblock syntax P_8x8 chroma DC residual components");
    require(result.residualChromaDcCodedBlockFlags[0] == 0
                && result.residualChromaDcCodedBlockFlags[1] == 0,
            "CABAC macroblock syntax P_8x8 chroma DC residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 0});
    contexts.setModel(97, {0, 1});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF incomplete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF keeps CBP");
    require(result.residualChromaDcComponents.size() == 1,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF partial component count");
    require(result.residualChromaDcCodedBlockFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF partial flag count");
    require(result.residualChromaDcComponents[0] == 0,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF partial component");
    require(result.residualChromaDcCodedBlockFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF partial flag");
    require(result.residualIncompleteComponent == 0,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF incomplete component");
    require(result.residualIncompleteCategory == QStringLiteral("chroma_dc"),
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF incomplete category");
    require(result.residualIncompleteStage == QStringLiteral("significant_coeff_flag"),
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("chroma_dc coded_block_flag[0]")),
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF stage message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCbfNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 residual non-zero CBF prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 residual non-zero CBF keeps CBP");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF diagnostic");
    require(result.residualLuma4x4BlockIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial block count");
    require(result.residualCodedBlockFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial flag count");
    require(result.residualLuma4x4BlockIndices[0] == 12,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial block index");
    require(result.residualCodedBlockFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete block index");
    require(result.residualIncompleteStage == QStringLiteral("significant_coeff_flag"),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete stage");
    require(result.diagnosticMessage.contains(QStringLiteral("coded_block_flag[12]")),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF stage message");
}

void testReadCabacMacroblockSyntaxP8x8SmallNonZeroMvd()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 0});
    contexts.setModel(47, {0, 0});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 small non-zero mvd result");
    require(result.complete, "CABAC macroblock syntax P_8x8 small non-zero mvd complete");
    require(result.parsedCodedBlockPatternZero,
            "CABAC macroblock syntax P_8x8 small non-zero mvd CBP zero flag");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 small non-zero mvd pair count");
    require(result.mvdL0[0].x == 1 && result.mvdL0[0].y == 0,
            "CABAC macroblock syntax P_8x8 small non-zero first mvd pair");
}

void testReadCabacMacroblockSyntaxP8x8NonZeroMvdIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 0});
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {0, 1});
    contexts.setModel(47, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 non-zero mvd prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 non-zero mvd incomplete");
    require(result.diagnosticCode == QStringLiteral("cabac_mvd_incomplete"),
            "CABAC macroblock syntax P_8x8 non-zero mvd diagnostic");
}

void testReadCodedBlockPatternZeroMonochrome()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    setCbpZeroContexts(contexts);

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern zero monochrome result");
    require(result.complete, "CABAC coded_block_pattern zero monochrome complete");
    require(result.codedBlockPattern == 0, "CABAC coded_block_pattern zero monochrome value");
    require(result.codedBlockPatternLuma == 0, "CABAC coded_block_pattern zero monochrome luma");
    require(result.codedBlockPatternChroma == 0, "CABAC coded_block_pattern zero monochrome chroma");
    require(result.firstCtxIdx == 73, "CABAC coded_block_pattern zero first ctxIdx");
    require(contexts.model(73).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 73");
    require(contexts.model(76).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 76");
}

void testReadCodedBlockPatternZeroChroma()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    setCbpZeroContexts(contexts);

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern zero chroma result");
    require(result.complete, "CABAC coded_block_pattern zero chroma complete");
    require(result.codedBlockPattern == 0, "CABAC coded_block_pattern zero chroma value");
    require(result.codedBlockPatternLuma == 0, "CABAC coded_block_pattern zero chroma luma");
    require(result.codedBlockPatternChroma == 0, "CABAC coded_block_pattern zero chroma component");
    require(contexts.model(77).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 77");
}

void testReadCodedBlockPatternNonZeroLumaIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    contexts.setModel(73, {0, 1});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 0});

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern non-zero luma prefix result");
    require(!result.complete, "CABAC coded_block_pattern non-zero luma incomplete");
    require(result.codedBlockPatternLuma != 0, "CABAC coded_block_pattern non-zero luma value");
    require(result.diagnosticCode == QStringLiteral("cabac_cbp_incomplete"),
            "CABAC coded_block_pattern non-zero luma diagnostic");
}

void testReadCodedBlockPatternNonZeroChromaIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern non-zero chroma prefix result");
    require(!result.complete, "CABAC coded_block_pattern non-zero chroma incomplete");
    require(result.codedBlockPatternChroma != 0, "CABAC coded_block_pattern non-zero chroma value");
    require(result.diagnosticCode == QStringLiteral("cabac_cbp_incomplete"),
            "CABAC coded_block_pattern non-zero chroma diagnostic");
}

void testReadMbQpDeltaZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(61);
    contexts.setModel(60, {0, 0});

    const H264CabacMbQpDeltaResult result =
        h264ReadCabacMbQpDeltaZero(reader, decoder, contexts);
    require(result.ok, "CABAC mb_qp_delta zero result");
    require(result.complete, "CABAC mb_qp_delta zero complete");
    require(result.mbQpDelta == 0, "CABAC mb_qp_delta zero value");
    require(result.firstCtxIdx == 60, "CABAC mb_qp_delta zero ctxIdx");
    require(contexts.model(60).stateIndex == 1, "CABAC mb_qp_delta zero updates ctxIdx 60");
}

void testReadMbQpDeltaNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(61);
    contexts.setModel(60, {0, 1});

    const H264CabacMbQpDeltaResult result =
        h264ReadCabacMbQpDeltaZero(reader, decoder, contexts);
    require(result.ok, "CABAC mb_qp_delta non-zero prefix result");
    require(!result.complete, "CABAC mb_qp_delta non-zero incomplete");
    require(result.firstCtxIdx == 60, "CABAC mb_qp_delta non-zero ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_mb_qp_delta_incomplete"),
            "CABAC mb_qp_delta non-zero diagnostic");
}

void testReadResidualCodedBlockFlagZeroLuma4x4()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualBlockResult result =
        h264ReadCabacResidualCodedBlockFlagZero(
            reader,
            decoder,
            contexts,
            H264CabacResidualBlockCategory::Luma4x4);
    require(result.ok, "CABAC residual coded_block_flag zero result");
    require(result.complete, "CABAC residual coded_block_flag zero complete");
    require(result.codedBlockFlag == 0, "CABAC residual coded_block_flag zero value");
    require(result.ctxIdx == 85, "CABAC residual coded_block_flag zero ctxIdx");
    require(contexts.model(85).stateIndex == 1,
            "CABAC residual coded_block_flag zero updates ctxIdx 85");
}

void testReadResidualCodedBlockFlagNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 1});

    const H264CabacResidualBlockResult result =
        h264ReadCabacResidualCodedBlockFlagZero(
            reader,
            decoder,
            contexts,
            H264CabacResidualBlockCategory::Luma4x4);
    require(result.ok, "CABAC residual coded_block_flag non-zero prefix result");
    require(!result.complete, "CABAC residual coded_block_flag non-zero incomplete");
    require(result.codedBlockFlag == 1, "CABAC residual coded_block_flag non-zero value");
    require(result.ctxIdx == 85, "CABAC residual coded_block_flag non-zero ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual coded_block_flag non-zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("luma4x4 coded_block_flag")),
            "CABAC residual coded_block_flag non-zero category message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC residual coded_block_flag non-zero next-stage message");
}

void testReadResidualLuma4x4CodedBlockFlagNonZeroPartial()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 1});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 non-zero CBF partial result");
    require(!result.complete, "CABAC residual luma4x4 non-zero CBF partial incomplete");
    require(result.blockIndices.size() == 1, "CABAC residual luma4x4 non-zero CBF partial block count");
    require(result.codedBlockFlags.size() == 1, "CABAC residual luma4x4 non-zero CBF partial flag count");
    require(result.blockIndices[0] == 12, "CABAC residual luma4x4 non-zero CBF partial block index");
    require(result.codedBlockFlags[0] == 1, "CABAC residual luma4x4 non-zero CBF partial flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 non-zero CBF partial incomplete block index");
    require(result.incompleteStage == QStringLiteral("significant_coeff_flag"),
            "CABAC residual luma4x4 non-zero CBF partial incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 non-zero CBF partial diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coded_block_flag[12]")),
            "CABAC residual luma4x4 non-zero CBF partial indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC residual luma4x4 non-zero CBF partial stage message");
}

void testReadResidualLuma4x4CodedBlockFlagsZeroSingleLuma8x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 CBF-zero group result");
    require(result.complete, "CABAC residual luma4x4 CBF-zero group complete");
    require(result.blockIndices.size() == 4, "CABAC residual luma4x4 CBF-zero block count");
    require(result.blockIndices[0] == 12 && result.blockIndices[3] == 15,
            "CABAC residual luma4x4 CBF-zero block indices");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[3] == 0,
            "CABAC residual luma4x4 CBF-zero values");
    require(result.firstCtxIdx == 85, "CABAC residual luma4x4 CBF-zero first ctxIdx");
}

void testReadResidualLuma4x4CodedBlockFlagsZeroAllLuma8x8()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 15);
    require(result.ok, "CABAC residual luma4x4 all-CBF-zero group result");
    require(result.complete, "CABAC residual luma4x4 all-CBF-zero group complete");
    require(result.blockIndices.size() == 16, "CABAC residual luma4x4 all-CBF-zero block count");
    require(result.blockIndices[0] == 0 && result.blockIndices[15] == 15,
            "CABAC residual luma4x4 all-CBF-zero block indices");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[15] == 0,
            "CABAC residual luma4x4 all-CBF-zero values");
}

void testReadResidualChromaDcCodedBlockFlagsZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(97, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 1);
    require(result.ok, "CABAC residual chroma DC CBF-zero result");
    require(result.complete, "CABAC residual chroma DC CBF-zero complete");
    require(result.firstCtxIdx == 97, "CABAC residual chroma DC CBF-zero first ctxIdx");
    require(result.components.size() == 2, "CABAC residual chroma DC CBF-zero component count");
    require(result.components[0] == 0 && result.components[1] == 1,
            "CABAC residual chroma DC CBF-zero components");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[1] == 0,
            "CABAC residual chroma DC CBF-zero values");
}

void testReadResidualChromaDcCodedBlockFlagsZeroWithChromaAcPresent()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(97, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 2);
    require(result.ok, "CABAC residual chroma DC CBF-zero with AC result");
    require(result.complete, "CABAC residual chroma DC CBF-zero with AC complete");
    require(result.components.size() == 2,
            "CABAC residual chroma DC CBF-zero with AC component count");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[1] == 0,
            "CABAC residual chroma DC CBF-zero with AC values");
}

void testReadResidualChromaDcCodedBlockFlagNonZeroPartial()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(97, {0, 1});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 1);
    require(result.ok, "CABAC residual chroma DC non-zero CBF partial result");
    require(!result.complete, "CABAC residual chroma DC non-zero CBF partial incomplete");
    require(result.components.size() == 1, "CABAC residual chroma DC non-zero CBF partial component count");
    require(result.codedBlockFlags.size() == 1, "CABAC residual chroma DC non-zero CBF partial flag count");
    require(result.components[0] == 0, "CABAC residual chroma DC non-zero CBF partial component");
    require(result.codedBlockFlags[0] == 1, "CABAC residual chroma DC non-zero CBF partial flag");
    require(result.incompleteComponent == 0,
            "CABAC residual chroma DC non-zero CBF partial incomplete component");
    require(result.incompleteStage == QStringLiteral("significant_coeff_flag"),
            "CABAC residual chroma DC non-zero CBF partial incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual chroma DC non-zero CBF partial diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("chroma_dc coded_block_flag[0]")),
            "CABAC residual chroma DC non-zero CBF partial indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC residual chroma DC non-zero CBF partial stage message");
}

void testAppendCabacP8x8MacroblockSyntaxSkeleton()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton append result");
    require(context.currentAddress == 1, "CABAC P_8x8 syntax skeleton advances address");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.address == 0, "CABAC P_8x8 syntax skeleton address");
    require(mb.mbType == QStringLiteral("P_8x8"), "CABAC P_8x8 syntax skeleton type");
    require(mb.predictionMode == QStringLiteral("Pred_L0 sub-macroblock"),
            "CABAC P_8x8 syntax skeleton prediction mode");
    require(mb.parsed, "CABAC P_8x8 syntax skeleton is parsed for CBP zero");
    require(mb.residualParsed, "CABAC P_8x8 syntax skeleton marks no residual parsed");
    require(mb.motionVectors.size() == 4, "CABAC P_8x8 syntax skeleton motion-vector count");
    require(mb.motionVectors.first().list == 0, "CABAC P_8x8 syntax skeleton L0 motion vector");
    require(mb.motionVectors.first().referenceIndex == 0, "CABAC P_8x8 syntax skeleton ref_idx_l0");
    require(mb.motionVectors.first().mvXQuarterPel == 0 && mb.motionVectors.first().mvYQuarterPel == 0,
            "CABAC P_8x8 syntax skeleton zero predicted motion vector");
    require(context.mvStatesL0[0].valid, "CABAC P_8x8 syntax skeleton updates L0 MV state");
    require(mb.note.contains(QStringLiteral("no residual blocks")),
            "CABAC P_8x8 syntax skeleton note describes no residual");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 8;
    syntax.codedBlockPatternLuma = 8;
    syntax.codedBlockPatternChroma = 0;
    syntax.residualLuma4x4BlockIndices = {12, 13, 14, 15};
    syntax.residualCodedBlockFlags = {0, 0, 0, 0};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 residual CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 residual CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.parsed, "CABAC P_8x8 residual CBF-zero skeleton parsed flag");
    require(mb.residualParsed, "CABAC P_8x8 residual CBF-zero skeleton residual parsed flag");
    require(mb.codedBlockPatternLuma == 8,
            "CABAC P_8x8 residual CBF-zero skeleton luma CBP");
    require(mb.residualBlocks.size() == 4,
            "CABAC P_8x8 residual CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().kind == QStringLiteral("luma4x4"),
            "CABAC P_8x8 residual CBF-zero skeleton residual kind");
    require(mb.residualBlocks.first().blockIndex == 12
                && mb.residualBlocks.last().blockIndex == 15,
            "CABAC P_8x8 residual CBF-zero skeleton residual indices");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 residual CBF-zero skeleton coefficient count");
    require(mb.note.contains(QStringLiteral("coded_block_flag")),
            "CABAC P_8x8 residual CBF-zero skeleton note");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithMultiResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 15;
    syntax.codedBlockPatternLuma = 15;
    syntax.codedBlockPatternChroma = 0;
    for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
        syntax.residualLuma4x4BlockIndices.append(blockIndex);
        syntax.residualCodedBlockFlags.append(0);
    }

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 multi residual CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 multi residual CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.codedBlockPatternLuma == 15,
            "CABAC P_8x8 multi residual CBF-zero skeleton luma CBP");
    require(mb.residualBlocks.size() == 16,
            "CABAC P_8x8 multi residual CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().blockIndex == 0
                && mb.residualBlocks.last().blockIndex == 15,
            "CABAC P_8x8 multi residual CBF-zero skeleton residual indices");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 multi residual CBF-zero skeleton coefficient count");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 16;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 1;
    syntax.residualChromaDcComponents = {0, 1};
    syntax.residualChromaDcCodedBlockFlags = {0, 0};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 chroma DC CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.codedBlockPatternLuma == 0,
            "CABAC P_8x8 chroma DC CBF-zero skeleton luma CBP");
    require(mb.codedBlockPatternChroma == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton chroma CBP");
    require(mb.residualBlocks.size() == 2,
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().kind == QStringLiteral("chroma_dc"),
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual kind");
    require(mb.residualBlocks.first().component == 0
                && mb.residualBlocks.last().component == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual components");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 chroma DC CBF-zero skeleton coefficient count");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonUsesNeighborPrediction()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 1);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    context.mvStatesL0[0] = {true, 0, 3, -1};

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton neighbor append result");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton neighbor macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.address == 1, "CABAC P_8x8 syntax skeleton neighbor address");
    require(mb.motionVectors.size() == 4, "CABAC P_8x8 syntax skeleton neighbor motion-vector count");
    require(mb.motionVectors.first().mvXQuarterPel == 3 && mb.motionVectors.first().mvYQuarterPel == -1,
            "CABAC P_8x8 syntax skeleton uses neighboring MV prediction");
    require(context.mvStatesL0[1].valid, "CABAC P_8x8 syntax skeleton neighbor updates L0 MV state");
    require(context.mvStatesL0[1].mvX == 3 && context.mvStatesL0[1].mvY == -1,
            "CABAC P_8x8 syntax skeleton neighbor state value");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonAppliesMvd()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{1, 0}, {0, -1}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton non-zero MVD append result");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton non-zero MVD macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.motionVectors.size() == 4,
            "CABAC P_8x8 syntax skeleton non-zero MVD motion-vector count");
    require(mb.motionVectors[0].mvXQuarterPel == 1 && mb.motionVectors[0].mvYQuarterPel == 0,
            "CABAC P_8x8 syntax skeleton applies first tiny MVD");
    require(mb.motionVectors[1].mvXQuarterPel == 0 && mb.motionVectors[1].mvYQuarterPel == -1,
            "CABAC P_8x8 syntax skeleton applies second tiny MVD");
}

void testReadISliceMbTypePrefix()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbTypePrefix(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type prefix result");
    require(result.value == 0 || result.value == 1, "CABAC I mb_type prefix bin value is binary");
    require(result.ctxIdx == 3, "CABAC I mb_type prefix ctxIdx");
}

void testReadISliceMbTypeINxN()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, context.currentQp, 23);

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type result");
    require(result.complete, "CABAC I mb_type I_NxN is complete");
    require(result.mbType == 0, "CABAC I mb_type I_NxN value");
    require(result.prefixBin == 0, "CABAC I mb_type I_NxN prefix bin");
    require(result.ctxIdx == 3, "CABAC I mb_type I_NxN ctxIdx");
}

void testReadISliceMbTypeI16x16()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(11);
    for (int ctxIdx = 0; ctxIdx <= 10; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(3, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type I_16x16 result");
    require(result.complete, "CABAC I mb_type I_16x16 is complete");
    require(result.mbType == 1, "CABAC I mb_type I_16x16 value");
    require(result.prefixBin == 1, "CABAC I mb_type I_16x16 prefix bin");
    require(result.ctxIdx == 3, "CABAC I mb_type I_16x16 ctxIdx");
}

void testReadBSliceMbSkipFlagReportsMissingContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 1, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(!result.ok, "CABAC B mb_skip_flag missing context fails cleanly");
    require(result.ctxIdx == 24, "CABAC B mb_skip_flag ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC B mb_skip_flag missing context diagnostic");
}

void testReadBSliceMbSkipFlagWithCoveredContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 1, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 39);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC B mb_skip_flag covered context result");
    require(result.value == 0 || result.value == 1, "CABAC B mb_skip_flag bin value is binary");
    require(result.ctxIdx == 24, "CABAC B mb_skip_flag covered ctxIdx");
}
}

int main()
{
    testReadPSliceMbSkipFlag();
    testReadPSliceMbSkipFlagUsesLeftNeighbor();
    testReadPSliceMbTypeP16x16();
    testReadPSliceMbTypeP16x8();
    testReadPSliceMbTypeP8x16();
    testReadPSliceMbTypeP8x8RequiresSubMbType();
    testReadPSubMbTypeP8x8();
    testReadPSubMbTypeReportsMissingContext();
    testReadPSubMbTypeP8x4();
    testReadPSubMbTypeP4x8();
    testReadPSubMbTypeP4x4();
    testReadFourPSubMbTypes();
    testReadFourPSubMbTypesReportsIndexedFailure();
    testReadPSubMbRefIdxNotPresent();
    testReadPSubMbRefIdxZero();
    testReadPSubMbRefIdxIncomplete();
    testReadMvdComponentZero();
    testReadMvdComponentAbs2Positive();
    testReadMvdComponentAbs3Positive();
    testReadMvdComponentGreaterThanThreeIncomplete();
    testReadPSubMbMvdZero8x8();
    testReadPSubMbMvdAbs2_8x8();
    testReadPSubMbMvdZero4x4();
    testReadCabacMacroblockSyntaxP8x8RefAbsent();
    testReadCabacMacroblockSyntaxP8x8RefZero();
    testReadCabacMacroblockSyntaxP8x8ChromaAcResidualIncomplete();
    testReadCabacMacroblockSyntaxP8x8SingleLumaResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8MultiLumaResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfNonZeroIncomplete();
    testReadCabacMacroblockSyntaxP8x8ResidualCbfNonZeroIncomplete();
    testReadCabacMacroblockSyntaxP8x8SmallNonZeroMvd();
    testReadCabacMacroblockSyntaxP8x8NonZeroMvdIncomplete();
    testReadCodedBlockPatternZeroMonochrome();
    testReadCodedBlockPatternZeroChroma();
    testReadCodedBlockPatternNonZeroLumaIncomplete();
    testReadCodedBlockPatternNonZeroChromaIncomplete();
    testReadMbQpDeltaZero();
    testReadMbQpDeltaNonZeroIncomplete();
    testReadResidualCodedBlockFlagZeroLuma4x4();
    testReadResidualCodedBlockFlagNonZeroIncomplete();
    testReadResidualLuma4x4CodedBlockFlagNonZeroPartial();
    testReadResidualLuma4x4CodedBlockFlagsZeroSingleLuma8x8();
    testReadResidualLuma4x4CodedBlockFlagsZeroAllLuma8x8();
    testReadResidualChromaDcCodedBlockFlagsZero();
    testReadResidualChromaDcCodedBlockFlagsZeroWithChromaAcPresent();
    testReadResidualChromaDcCodedBlockFlagNonZeroPartial();
    testAppendCabacP8x8MacroblockSyntaxSkeleton();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithResidualCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithMultiResidualCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonUsesNeighborPrediction();
    testAppendCabacP8x8MacroblockSyntaxSkeletonAppliesMvd();
    testReadISliceMbTypePrefix();
    testReadISliceMbTypeINxN();
    testReadISliceMbTypeI16x16();
    testReadBSliceMbSkipFlagReportsMissingContext();
    testReadBSliceMbSkipFlagWithCoveredContext();
    std::cout << "H264CabacSyntaxReader tests passed\n";
    return 0;
}
