#include "core/RebufferState.h"

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

void repeatedRequestsCancelPreviousTarget()
{
    RebufferState state;

    RebufferState::StartResult first = state.start(120, 4);
    require(!first.canceledPrevious, "first rebuffer request should not cancel anything");
    require(state.hasPending(), "first request should leave a pending rebuffer");
    require(state.accepts(4, 120), "first request should accept matching generation and target");

    RebufferState::StartResult second = state.start(240, 5);
    require(second.canceledPrevious, "second far old-frame request should cancel previous target");
    require(second.canceledTargetFrameIndex == 120, "second request should report canceled target");
    require(!state.accepts(4, 120), "old generation and target should become stale");
    require(state.accepts(5, 240), "new generation and target should be current");
}

void staleProgressAndCompletionAreIgnored()
{
    RebufferState state;
    state.start(300, 7);

    require(!state.accepts(6, 300), "stale generation progress should be ignored");
    require(!state.accepts(7, 250), "stale target progress should be ignored");
    require(!state.complete(6, 300), "stale generation completion should be ignored");
    require(state.hasPending(), "stale completion should not clear pending request");
    require(!state.complete(7, 250), "wrong target completion should be ignored");
    require(state.hasPending(), "wrong target completion should not clear pending request");

    require(state.complete(7, 300), "current target completion should be accepted");
    require(!state.hasPending(), "current completion should clear pending request");
}

void progressIsClampedToTargetRange()
{
    RebufferState::Progress first = RebufferState::progress(100, 100, 140);
    require(first.bufferedFrames == 1, "first decoded frame should count as one buffered frame");
    require(first.totalFrames == 40, "total buffered frames should span start to target");
    require(first.percent == 2, "first progress percent should be integer clamped");

    RebufferState::Progress final = RebufferState::progress(100, 139, 140);
    require(final.bufferedFrames == 40, "last hidden frame should complete buffering span");
    require(final.percent == 100, "last hidden frame should report 100 percent");

    RebufferState::Progress overshoot = RebufferState::progress(100, 160, 140);
    require(overshoot.bufferedFrames == 40, "progress should clamp overshoot to total");
    require(overshoot.percent == 100, "overshoot progress should clamp to 100 percent");
}
}

int main()
{
    repeatedRequestsCancelPreviousTarget();
    staleProgressAndCompletionAreIgnored();
    progressIsClampedToTargetRange();

    std::cout << "Rebuffer state tests passed\n";
    return 0;
}
