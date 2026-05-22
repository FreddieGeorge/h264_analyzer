#include "core/decode/SeekCheckpointEmitter.h"

namespace
{
bool hasSeekAnchor(const FrameSeekCheckpoint &checkpoint)
{
    return checkpoint.packetPosition >= 0
        || checkpoint.packetPts != AV_NOPTS_VALUE
        || checkpoint.packetDts != AV_NOPTS_VALUE;
}

bool isEligibleSyncPoint(const FrameSeekCheckpoint &checkpoint, int frameIndex)
{
    return checkpoint.keyframe || checkpoint.idr || frameIndex == 0;
}
}

std::optional<FrameSeekCheckpoint> seekCheckpointForDecodedFrame(const FrameSeekCheckpoint &checkpoint,
                                                                 int frameIndex)
{
    if (!isEligibleSyncPoint(checkpoint, frameIndex) || !hasSeekAnchor(checkpoint)) {
        return std::nullopt;
    }

    FrameSeekCheckpoint result = checkpoint;
    result.frameIndex = frameIndex;
    return result;
}
