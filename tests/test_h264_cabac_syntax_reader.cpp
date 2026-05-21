#include "core/parser/video/h264/H264CabacDecoder.h"
#include "core/parser/video/h264/H264CabacSyntaxReader.h"
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
}

int main()
{
    testReadPSliceMbSkipFlag();
    testReadPSliceMbSkipFlagUsesLeftNeighbor();
    testReadISliceMbTypePrefix();
    testReadISliceMbTypeINxN();
    testReadBSliceMbSkipFlagReportsMissingContext();
    std::cout << "H264CabacSyntaxReader tests passed\n";
    return 0;
}
