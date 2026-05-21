#include "core/parser/video/h264/H264CabacDecoder.h"

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

void testConsumeAlignmentBits()
{
    BitReader aligned(QByteArray::fromHex("ff"));
    require(H264CabacDecoder::consumeAlignmentBits(aligned), "already aligned CABAC alignment");
    require(aligned.bitOffset() == 0, "already aligned bit offset unchanged");

    BitReader reader(QByteArray::fromHex("bf")); // 10111111
    reader.readBits(3);
    require(H264CabacDecoder::consumeAlignmentBits(reader), "CABAC alignment consumes one bits");
    require(reader.bitOffset() == 8, "CABAC alignment reaches byte boundary");

    BitReader malformed(QByteArray::fromHex("af")); // 10101111
    malformed.readBits(3);
    require(!H264CabacDecoder::consumeAlignmentBits(malformed), "CABAC alignment rejects zero bit");
}

void testInitializeDecoderState()
{
    BitReader reader(QByteArray::fromHex("aa80")); // first 9 bits: 101010101
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC decoder initializes from 9 bits");
    require(decoder.state().initialized, "CABAC decoder initialized flag");
    require(decoder.state().codIRange == 510, "CABAC initial range");
    require(decoder.state().codIOffset == 341, "CABAC initial offset");
    require(reader.bitOffset() == 9, "CABAC initialize consumes 9 bits");
}

void testContextModelInitialization()
{
    const H264CabacContextModel low = H264CabacDecoder::initializedContextModel(0, 10, 26);
    require(low.stateIndex == 53, "CABAC context low state index");
    require(low.valueMps == 0, "CABAC context low MPS");

    const H264CabacContextModel high = H264CabacDecoder::initializedContextModel(0, 100, 26);
    require(high.stateIndex == 36, "CABAC context high state index");
    require(high.valueMps == 1, "CABAC context high MPS");

    const H264CabacContextModel clipped = H264CabacDecoder::initializedContextModel(-100, -100, 26);
    require(clipped.stateIndex == 62, "CABAC context clipped state index");
    require(clipped.valueMps == 0, "CABAC context clipped MPS");
}

void testContextModelSetInitialization()
{
    const H264CabacContextModelSet pIdc0 =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 26, 23);
    const H264CabacContextModelSet pIdc1 =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 1, 26, 23);
    require(pIdc0.size() == 24, "CABAC P context set size");
    require(pIdc0.isInitialized(0), "CABAC ctxIdx 0 initialized");
    require(pIdc0.isInitialized(23), "CABAC ctxIdx 23 initialized");
    require(pIdc0.model(11).stateIndex != pIdc1.model(11).stateIndex
                || pIdc0.model(11).valueMps != pIdc1.model(11).valueMps,
            "CABAC cabac_init_idc selects different P context table");

    const H264CabacContextModelSet intra =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, 26, 23);
    require(intra.isInitialized(0), "CABAC intra ctxIdx 0 initialized");
    require(!intra.isInitialized(11), "CABAC intra ctxIdx 11 not initialized in covered subset");

    const H264CabacContextModelSet invalid =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 3, 26, 23);
    require(!invalid.isInitialized(0), "CABAC invalid cabac_init_idc leaves contexts uninitialized");

    const H264CabacContextModelSet lowQp =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, -10, 0);
    const H264CabacContextModelSet clippedLowQp =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 0, 0);
    require(lowQp.model(0).stateIndex == clippedLowQp.model(0).stateIndex,
            "CABAC context set clips low QP state index");
    require(lowQp.model(0).valueMps == clippedLowQp.model(0).valueMps,
            "CABAC context set clips low QP MPS");
}

void testDecodeDecisionMps()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC MPS decoder initializes");

    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;

    int bin = -1;
    require(decoder.decodeBin(reader, context, &bin), "CABAC decodes MPS bin");
    require(bin == 0, "CABAC MPS bin value");
    require(context.stateIndex == 1, "CABAC MPS state transition");
    require(context.valueMps == 0, "CABAC MPS keeps valueMps");
    require(decoder.state().codIRange == 270, "CABAC MPS range update");
    require(decoder.state().codIOffset == 0, "CABAC MPS offset update");
    require(reader.bitOffset() == 9, "CABAC MPS no renormalization bit needed");
}

void testDecodeDecisionLps()
{
    BitReader reader(QByteArray::fromHex("ff80"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC LPS decoder initializes");

    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;

    int bin = -1;
    require(decoder.decodeBin(reader, context, &bin), "CABAC decodes LPS bin");
    require(bin == 1, "CABAC LPS bin value");
    require(context.stateIndex == 0, "CABAC LPS state transition");
    require(context.valueMps == 1, "CABAC LPS toggles valueMps at state 0");
    require(decoder.state().codIRange == 480, "CABAC LPS renormalized range");
    require(decoder.state().codIOffset == 482, "CABAC LPS renormalized offset");
    require(reader.bitOffset() == 10, "CABAC LPS consumes renormalization bit");
}

void testDecodeDecisionFromContextSet()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC context-set decoder initializes");

    H264CabacContextModelSet contexts(1);
    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;
    contexts.setModel(0, context);

    int bin = -1;
    require(decoder.decodeBin(reader, contexts, 0, &bin), "CABAC decodes bin from context set");
    require(bin == 0, "CABAC context-set bin value");
    require(contexts.model(0).stateIndex == 1, "CABAC context-set stores updated state");
    require(!decoder.decodeBin(reader, contexts, 1, &bin), "CABAC rejects uninitialized context");
}

void testDecodeBypassBin()
{
    BitReader zeroReader(QByteArray::fromHex("1900")); // first 9 bits 000110010, next bit 0
    H264CabacDecoder zeroDecoder;
    require(zeroDecoder.initialize(zeroReader), "CABAC bypass zero decoder initializes");
    int bin = -1;
    require(zeroDecoder.decodeBypassBin(zeroReader, &bin), "CABAC decodes bypass zero");
    require(bin == 0, "CABAC bypass zero bin");
    require(zeroDecoder.state().codIOffset == 100, "CABAC bypass zero offset");
    require(zeroReader.bitOffset() == 10, "CABAC bypass zero consumes one bit");

    BitReader oneReader(QByteArray::fromHex("9600")); // first 9 bits 100101100, next bit 0
    H264CabacDecoder oneDecoder;
    require(oneDecoder.initialize(oneReader), "CABAC bypass one decoder initializes");
    require(oneDecoder.decodeBypassBin(oneReader, &bin), "CABAC decodes bypass one");
    require(bin == 1, "CABAC bypass one bin");
    require(oneDecoder.state().codIOffset == 90, "CABAC bypass one offset");
}

void testDecodeTerminate()
{
    BitReader zeroReader(QByteArray::fromHex("0000"));
    H264CabacDecoder zeroDecoder;
    require(zeroDecoder.initialize(zeroReader), "CABAC terminate zero decoder initializes");
    int bin = -1;
    require(zeroDecoder.decodeTerminate(zeroReader, &bin), "CABAC decodes terminate zero");
    require(bin == 0, "CABAC terminate zero bin");
    require(zeroDecoder.state().codIRange == 508, "CABAC terminate zero range");
    require(zeroReader.bitOffset() == 9, "CABAC terminate zero no renormalization bit");

    BitReader oneReader(QByteArray::fromHex("ff80"));
    H264CabacDecoder oneDecoder;
    require(oneDecoder.initialize(oneReader), "CABAC terminate one decoder initializes");
    require(oneDecoder.decodeTerminate(oneReader, &bin), "CABAC decodes terminate one");
    require(bin == 1, "CABAC terminate one bin");
    require(oneDecoder.state().codIRange == 508, "CABAC terminate one range");
    require(oneReader.bitOffset() == 9, "CABAC terminate one consumes no extra bit");
}
}

int main()
{
    testConsumeAlignmentBits();
    testInitializeDecoderState();
    testContextModelInitialization();
    testContextModelSetInitialization();
    testDecodeDecisionMps();
    testDecodeDecisionLps();
    testDecodeDecisionFromContextSet();
    testDecodeBypassBin();
    testDecodeTerminate();
    std::cout << "H264CabacDecoder tests passed\n";
    return 0;
}
