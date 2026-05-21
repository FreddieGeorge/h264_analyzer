#include "core/parser/video/h264/cabac/H264CabacDecoder.h"
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

void testReadMvdComponentIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(47, {0, 1});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 1);
    require(result.ok, "CABAC mvd_l0 component non-zero prefix result");
    require(!result.complete, "CABAC mvd_l0 component non-zero prefix incomplete");
    require(result.ctxIdx == 47, "CABAC mvd_l0 y context");
    require(result.diagnosticCode == QStringLiteral("cabac_mvd_incomplete"),
            "CABAC mvd_l0 component incomplete diagnostic");
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
    testReadMvdComponentIncomplete();
    testReadPSubMbMvdZero8x8();
    testReadPSubMbMvdZero4x4();
    testReadISliceMbTypePrefix();
    testReadISliceMbTypeINxN();
    testReadISliceMbTypeI16x16();
    testReadBSliceMbSkipFlagReportsMissingContext();
    testReadBSliceMbSkipFlagWithCoveredContext();
    std::cout << "H264CabacSyntaxReader tests passed\n";
    return 0;
}
