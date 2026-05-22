#include "core/decode/DecodeSeekPlanner.h"

#include "core/decode/FFmpegDecoder.h"
#include "core/decode/StreamLogFormatter.h"

DecodeStartPosition prepareDecodeStart(FFmpegDecoder &decoder,
                                       const QString &filePath,
                                       int targetFrameIndex,
                                       const FrameSeekCheckpoint &checkpoint)
{
    DecodeStartPosition position;
    if (checkpoint.frameIndex < 0) {
        return position;
    }

    if (decoder.seekToCheckpoint(checkpoint)) {
        position.frameIndex = checkpoint.frameIndex;
        position.logMessages.append(checkpointSeekLogMessage(checkpoint.frameIndex, targetFrameIndex));
        return position;
    }

    position.logMessages.append(checkpointSeekFailedLogMessage(decoder.lastError()));
    decoder.close();
    if (!decoder.openFile(filePath)) {
        position.ok = false;
        position.errorMessage = decoder.lastError();
        return position;
    }

    position.reopenedStream = true;
    position.reopenedStreamInfo = decoder.getStreamInfo();
    return position;
}
