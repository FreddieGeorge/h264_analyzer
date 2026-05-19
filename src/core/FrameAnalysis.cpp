#include "core/FrameAnalysis.h"

QString analysisUnitKindName(AnalysisUnitKind kind)
{
    switch (kind) {
    case AnalysisUnitKind::Nalu: return QStringLiteral("NALU");
    case AnalysisUnitKind::Obu: return QStringLiteral("OBU");
    case AnalysisUnitKind::TileGroup: return QStringLiteral("tile_group");
    case AnalysisUnitKind::AdtsFrame: return QStringLiteral("adts_frame");
    case AnalysisUnitKind::Mp3Frame: return QStringLiteral("mp3_frame");
    case AnalysisUnitKind::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

QString analysisRegionKindName(AnalysisRegionKind kind)
{
    switch (kind) {
    case AnalysisRegionKind::Macroblock: return QStringLiteral("macroblock");
    case AnalysisRegionKind::Ctu: return QStringLiteral("ctu");
    case AnalysisRegionKind::Superblock: return QStringLiteral("superblock");
    case AnalysisRegionKind::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}
