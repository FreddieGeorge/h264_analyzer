#include "core/H264FrameAnalysisAdapter.h"

FrameAnalysis frameAnalysisFromH264Syntax(const FrameSyntaxInfo &syntaxInfo)
{
    FrameAnalysis analysis;
    analysis.frameIndex = syntaxInfo.index;
    analysis.mediaKind = MediaKind::Video;
    analysis.accessUnitKind = AccessUnitKind::VideoFrame;
    analysis.codecKind = syntaxInfo.codecKind;
    analysis.codecName = syntaxInfo.codecName;
    analysis.pts = syntaxInfo.pts;
    analysis.dts = syntaxInfo.dts;
    analysis.poc = syntaxInfo.poc;
    analysis.frameNum = syntaxInfo.frameNum;
    analysis.frameType = syntaxInfo.frameType;
    analysis.hasFrame = !syntaxInfo.slices.isEmpty();
    analysis.codecSpecificDetails = QVariant::fromValue(syntaxInfo);

    for (const ParserDiagnosticInfo &diagnostic : syntaxInfo.diagnostics) {
        analysis.diagnostics.append({QStringLiteral("frame"),
                                     diagnostic.code,
                                     diagnostic.message,
                                     QStringLiteral("warning")});
    }

    auto appendBitFields = [&analysis](const QString &path, const QVector<SyntaxFieldInfo> &fields) {
        for (const SyntaxFieldInfo &field : fields) {
            AnalysisBitField bitField;
            bitField.path = path;
            bitField.name = field.name;
            bitField.bitOffset = field.bitOffset;
            bitField.bitLength = field.bitLength;
            bitField.value = field.value;
            analysis.bitFields.append(bitField);
        }
    };

    for (int naluIndex = 0; naluIndex < syntaxInfo.nalus.size(); ++naluIndex) {
        const NaluInfo &nalu = syntaxInfo.nalus[naluIndex];

        AnalysisUnit unit;
        unit.kind = AnalysisUnitKind::Nalu;
        unit.offset = nalu.offset;
        unit.size = nalu.size;
        unit.type = nalu.nalUnitType;
        unit.typeName = nalu.nalUnitTypeName;
        analysis.units.append(unit);

        const QString naluPath = QStringLiteral("units/%1").arg(naluIndex);
        for (const ParserDiagnosticInfo &diagnostic : nalu.diagnostics) {
            analysis.diagnostics.append({naluPath,
                                         diagnostic.code,
                                         diagnostic.message,
                                         QStringLiteral("warning")});
        }
        if (nalu.sps.valid) {
            AnalysisParameterSet parameterSet;
            parameterSet.kind = QStringLiteral("SPS");
            parameterSet.id = nalu.sps.seqParameterSetId;
            parameterSet.summary = QStringLiteral("%1x%2 profile %3 level %4")
                                       .arg(nalu.sps.width)
                                       .arg(nalu.sps.height)
                                       .arg(nalu.sps.profileIdc)
                                       .arg(nalu.sps.levelIdc);
            for (const SyntaxFieldInfo &field : nalu.sps.fields) {
                parameterSet.bitFields.append({naluPath + QStringLiteral("/sps"),
                                               field.name,
                                               field.bitOffset,
                                               field.bitLength,
                                               field.value});
            }
            analysis.parameterSets.append(parameterSet);
            appendBitFields(naluPath + QStringLiteral("/sps"), nalu.sps.fields);
        }
        if (nalu.pps.valid) {
            AnalysisParameterSet parameterSet;
            parameterSet.kind = QStringLiteral("PPS");
            parameterSet.id = nalu.pps.picParameterSetId;
            parameterSet.summary = QStringLiteral("SPS %1, initial QP %2")
                                       .arg(nalu.pps.seqParameterSetId)
                                       .arg(26 + nalu.pps.picInitQpMinus26);
            for (const SyntaxFieldInfo &field : nalu.pps.fields) {
                parameterSet.bitFields.append({naluPath + QStringLiteral("/pps"),
                                               field.name,
                                               field.bitOffset,
                                               field.bitLength,
                                               field.value});
            }
            analysis.parameterSets.append(parameterSet);
            appendBitFields(naluPath + QStringLiteral("/pps"), nalu.pps.fields);
        }
    }

    for (int sliceIndex = 0; sliceIndex < syntaxInfo.slices.size(); ++sliceIndex) {
        const SliceInfo &slice = syntaxInfo.slices[sliceIndex];
        const QString slicePath = QStringLiteral("slices/%1").arg(sliceIndex);
        appendBitFields(slicePath, slice.fields);

        for (const ParserDiagnosticInfo &diagnostic : slice.diagnostics) {
            analysis.diagnostics.append({slicePath,
                                         diagnostic.code,
                                         diagnostic.message,
                                         QStringLiteral("warning")});
        }

        const int picWidthInMbs = slice.picWidthInMbs > 0 ? slice.picWidthInMbs : 1;
        for (const MacroblockInfo &mb : slice.macroblocks) {
            const int mbX = mb.address % picWidthInMbs;
            const int mbY = mb.address / picWidthInMbs;

            AnalysisRegion region;
            region.kind = AnalysisRegionKind::Macroblock;
            region.address = mb.address;
            region.x = mbX * 16;
            region.y = mbY * 16;
            region.width = 16;
            region.height = 16;
            region.qp = mb.qp;
            region.type = mb.mbType;
            region.predictionMode = mb.predictionMode;
            region.parsed = mb.parsed;
            region.skipped = mb.skipped;
            region.note = mb.note;
            analysis.regions.append(region);

            for (const MotionVectorInfo &mv : mb.motionVectors) {
                AnalysisMotionVector analysisMv;
                analysisMv.regionAddress = mb.address;
                analysisMv.list = mv.list;
                analysisMv.referenceIndex = mv.referenceIndex;
                analysisMv.sourceX = region.x + region.width / 2;
                analysisMv.sourceY = region.y + region.height / 2;
                analysisMv.referenceX = mv.referenceX;
                analysisMv.referenceY = mv.referenceY;
                analysisMv.mvXQuarterPel = mv.mvXQuarterPel;
                analysisMv.mvYQuarterPel = mv.mvYQuarterPel;
                analysis.motionVectors.append(analysisMv);
            }
        }
    }

    return analysis;
}

FrameSyntaxInfo h264SyntaxFromFrameAnalysis(const FrameAnalysis &analysis)
{
    if (analysis.codecSpecificDetails.canConvert<FrameSyntaxInfo>()) {
        return analysis.codecSpecificDetails.value<FrameSyntaxInfo>();
    }

    FrameSyntaxInfo syntax;
    syntax.index = analysis.frameIndex;
    syntax.codecKind = analysis.codecKind;
    syntax.codecName = analysis.codecName;
    syntax.pts = analysis.pts;
    syntax.dts = analysis.dts;
    syntax.poc = analysis.poc;
    syntax.frameNum = analysis.frameNum;
    syntax.frameType = analysis.frameType;
    return syntax;
}
