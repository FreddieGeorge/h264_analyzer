#pragma once

#include "core/model/FrameSeekCheckpoint.h"

#include <optional>

std::optional<FrameSeekCheckpoint> seekCheckpointForDecodedFrame(const FrameSeekCheckpoint &checkpoint,
                                                                 int frameIndex);
