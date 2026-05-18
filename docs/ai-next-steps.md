# AI Next Steps for H264 Analyzer

This document is intended for the next AI/coding agent that continues development of this project.

## Project Context

Repository:

```text
git@github.com:FreddieGeorge/h264_analyzer.git
```

Typical local path:

```text
D:\Desktop\h264_analyzer
```

Technology stack:

- C++17
- Qt 6 Widgets
- `QOpenGLWidget`
- CMake
- FFmpeg
- Windows development environment: MSYS2 UCRT64
- Windows portable deployment script is already available

Important existing files:

```text
CMakeLists.txt
src/app/MainWindow.*
src/core/FFmpegDecoder.*
src/core/DecodeWorker.*
src/core/H264Parser.*
src/ui/VideoCanvas.*
src/ui/FrameListView.*
src/ui/PropertyTreeView.*
src/ui/LogDock.*
scripts/deploy-windows-msys2.ps1
docs/windows-deployment.md
installEnv.md
```

## Current Capabilities

The project currently has:

1. Qt main window framework
   - `MainWindow`
   - left dock: `FrameListView`
   - center: `VideoCanvas`
   - right dock: `PropertyTreeView`
   - bottom dock: `LogDock`

2. FFmpeg decoding
   - `FFmpegDecoder`
   - `DecodeWorker`
   - decoding runs on a background `QThread`

3. Basic H.264 syntax parsing
   - custom `H264Parser`
   - NALU splitting
   - Annex B and AVCC/length-prefixed support
   - Exp-Golomb decoding
   - SPS/PPS/Slice Header basic fields

4. UI binding
   - frame list shows frame type, POC, and `frame_num`
   - property tree shows NALU/SPS/PPS/Slice information

5. `VideoCanvas` overlay foundation
   - video texture rendering
   - macroblock grid
   - QP heatmap
   - motion vector drawing interface

6. Windows deployment
   - `scripts/deploy-windows-msys2.ps1`
   - creates portable package under `dist/H264Analyzer-windows-ucrt64`
   - creates `dist/H264Analyzer-windows-ucrt64.zip`

## Build And Verification Commands

Use MSYS2 UCRT64 toolchain:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake --build build-msys2-ucrt"
```

Run from development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && ./build-msys2-ucrt/H264Analyzer.exe"
```

Create Windows portable package:

```powershell
.\scripts\deploy-windows-msys2.ps1
```

Do not distribute `build-msys2-ucrt/H264Analyzer.exe` alone. Distribute:

```text
dist/H264Analyzer-windows-ucrt64.zip
```

## Important Rules For Future Work

1. Do not commit `build-msys2-ucrt/`, `build*/`, or `dist/`.
2. Do not use FFmpeg's H.264 parser as the main syntax parser. The project goal is to implement direct H.264 syntax parsing in `H264Parser`.
3. Do not block the UI thread with decoding, seeking, or heavy syntax parsing.
4. `H264Parser` must be fault tolerant. Bad or unsupported bitstreams should produce warnings/logs, not crashes.
5. Reuse existing `VideoCanvas` coordinate mapping helpers:
   - `videoDisplayRect()`
   - `mapVideoPointToWidget()`
   - `macroblockWidgetRect()`
6. `MotionVectorInfo` is already defined, but real MV parsing is not implemented yet.
7. After each meaningful stage, run a build and create a Git commit.
8. If deployment changes, run `scripts/deploy-windows-msys2.ps1`.

## Stage 1: Playback Control And Frame Synchronization

Goal:

Turn the app from continuous playback into a controllable analysis tool.

Tasks:

1. Add playback toolbar controls:
   - Play/Pause
   - Previous Frame
   - Next Frame
   - Stop
   - current frame index display

2. Refactor `DecodeWorker`:
   - support pause
   - support single-frame stepping
   - support stop

3. Add frame cache:
   - cache recent decoded frames
   - cache matching `FrameSyntaxInfo`
   - clicking a frame in `FrameListView` should show that frame in `VideoCanvas`

4. Synchronize:
   - video image
   - left frame list selection
   - right property tree
   - overlay data

Acceptance criteria:

- Opening a stream starts decoding.
- User can pause playback.
- User can step to next frame.
- User can click a frame in `FrameListView` and the canvas/property tree update to that frame.
- UI remains responsive.

## Stage 2: Better H.264 Syntax Parsing

Goal:

Make the right property panel more useful and closer to a professional bitstream analyzer.

Tasks:

1. Extend SPS parsing:
   - constraint flags
   - VUI presence
   - timing info
   - aspect ratio
   - bitstream restriction

2. Extend PPS parsing:
   - `weighted_pred_flag`
   - `weighted_bipred_idc`
   - `transform_8x8_mode_flag`
   - deblocking-related flags

3. Extend Slice Header parsing:
   - `field_pic_flag`
   - `bottom_field_flag`
   - `num_ref_idx_active_override_flag`
   - `ref_pic_list_modification`
   - `pred_weight_table`
   - `dec_ref_pic_marking`

4. Add bit/byte position metadata where practical:
   - NALU offset
   - field bit offset
   - field length

Acceptance criteria:

- Right property tree shows richer SPS/PPS/Slice fields.
- Parser does not crash on unsupported streams.
- Unsupported fields are skipped safely or marked as unsupported.

## Stage 3: Real Macroblock-Level Information

Goal:

Make QP heatmap and macroblock information real instead of slice-level estimates.

Current state:

- `MacroblockInfo` exists.
- `QP` heatmap currently uses slice-level estimated QP.
- `mb_type` is currently placeholder/estimated.

Tasks:

1. Implement basic `slice_data` parsing.
2. Start with common CAVLC baseline/main profile streams.
3. Parse per macroblock:
   - `mb_address`
   - `mb_type`
   - QP
   - `coded_block_pattern`
   - intra/inter/skip/direct mode where possible

4. Update `MacroblockInfo`:
   - `mbType`
   - `qp`
   - coded block information
   - prediction mode

5. Update `VideoCanvas` QP heatmap to use real macroblock QP.

Acceptance criteria:

- Property tree can expand macroblock information.
- QP heatmap varies per macroblock on streams where QP changes.
- Unsupported entropy modes are clearly logged.

## Stage 4: Motion Vector Parsing And Visualization

Goal:

Draw real motion vector arrows.

Current state:

- `MotionVectorInfo` exists.
- `VideoCanvas::drawMotionVectors()` exists.
- Real MV values are not yet parsed from the bitstream.

Tasks:

1. Parse P/B Slice motion vector related syntax.
2. Fill `MacroblockInfo::motionVectors`:
   - list L0/L1
   - reference index
   - `mv_x` in quarter-pel units
   - `mv_y` in quarter-pel units
   - reference position if available

3. Update drawing:
   - L0: cyan
   - L1: magenta
   - skip zero-length vectors

4. Add display switch for motion vectors.

Coordinate mapping logic:

```text
mbX = mb.address % picWidthInMbs
mbY = mb.address / picWidthInMbs

start = current macroblock center:
  x = mbX * 16 + 8
  y = mbY * 16 + 8

end = referenceBase + MV:
  x = referenceX + 8 + mvXQuarterPel / 4.0
  y = referenceY + 8 + mvYQuarterPel / 4.0
```

If explicit reference position is not available, use co-located macroblock center:

```text
referenceBase = currentCenter
```

Then map video pixel coordinates to widget coordinates through `mapVideoPointToWidget()`.

Acceptance criteria:

- P/B frames show real MV arrows.
- I frames show no MV arrows.
- Motion vector overlay can be toggled.

## Stage 5: Overlay Control Panel

Goal:

Give users control over analysis overlays.

Tasks:

1. Add toolbar or menu controls:
   - Show Macroblock Grid
   - Show QP Heatmap
   - Show Motion Vectors
   - Overlay opacity slider

2. Add `VideoCanvas` APIs:

```cpp
void setShowGrid(bool enabled);
void setShowQpHeatmap(bool enabled);
void setShowMotionVectors(bool enabled);
void setOverlayOpacity(float opacity);
```

3. Defaults:
   - grid: on
   - QP heatmap: off
   - MV: off

Acceptance criteria:

- Each overlay can be independently toggled.
- Opacity changes are visible immediately.
- Settings do not require reloading the file.

## Stage 6: Export Features

Goal:

Allow users to save analysis results.

Tasks:

1. Add `File -> Export Frame Syntax JSON`.
2. Add `File -> Export Frame List CSV`.
3. Add `File -> Export Screenshot`.
   - Screenshot should include current video frame and overlays.
4. Add log output for export path and success/failure.

Acceptance criteria:

- Selected frame syntax exports to JSON.
- Frame list exports to CSV.
- Screenshot matches the visible canvas.

## Stage 7: Engineering Quality And CI

Goal:

Make the project easier to maintain and release.

Tasks:

1. Add GitHub Actions for Windows MSYS2 build.
2. Generate portable zip artifact in CI.
3. Add tests:
   - Exp-Golomb decoding
   - Annex B NALU splitting
   - AVCC/length-prefixed NALU splitting
   - SPS width/height parsing

4. Update README:
   - build instructions
   - run instructions
   - deployment instructions
   - feature overview

5. Run:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake --build build-msys2-ucrt"
```

Acceptance criteria:

- CI builds successfully.
- Windows portable zip is available as an artifact.
- Tests pass locally and in CI.

## Suggested Commit Plan

Use separate commits per stage, for example:

```text
Add playback controls and frame synchronization
Expand H264 SPS PPS slice parsing
Parse macroblock QP and mb_type
Visualize real motion vectors
Add overlay control toolbar
Add frame export and screenshot features
Add Windows CI build and deployment artifact
```

## Final Reminder

Prioritize correctness and stability over completeness. H.264 bitstream parsing is complex; unsupported syntax should be logged and skipped gracefully. The UI should remain responsive at all times.

