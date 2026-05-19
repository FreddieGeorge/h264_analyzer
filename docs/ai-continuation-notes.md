# AI Continuation Notes

This document is the current handoff note for future AI/coding agents working on this repository.

Repository:

```text
git@github.com:FreddieGeorge/ZStreamEye.git
```

Typical local path:

```text
D:\Desktop\ZStreamEye
```

## Current State

The project is now a usable Qt/FFmpeg-based H.264 bitstream analysis tool.

Implemented capabilities:

- Qt 6 Widgets main window with dockable frame list, property tree, log panel, and `QOpenGLWidget` video canvas.
- Background FFmpeg decoding through `DecodeWorker` on `QThread`.
- Playback controls: play/pause, previous frame, next frame, stop, current frame indicator.
- Frame synchronization between decoded image, frame list selection, property tree, and overlays.
- Recent decoded frame cache plus checkpoint-based rebuffering when a selected frame was evicted.
- Indexed seek checkpoints built from decoded keyframe/IDR packets, including
  packet timestamps/byte positions and parser parameter-set state for resumed
  syntax parsing.
- Codec-neutral bitstream parser interface:
  - `CodecKind`
  - `IBitstreamParser`
  - `FrameAnalysis`
  - parser state snapshots for checkpoint resume
  - `FFmpegDecoder` no longer owns `H264Parser` directly
- H.264 analysis is adapted into codec-neutral `FrameAnalysis` through
  `H264FrameAnalysisAdapter`, while retaining rich H.264 structs as
  codec-specific details.
- Custom H.264 parser for Annex B and AVCC/length-prefixed packets.
- SPS/PPS/Slice Header parsing with VUI/timing/aspect/bitstream restriction and field bit metadata where practical.
- Basic CAVLC `slice_data` parsing for common baseline/main-profile paths:
  - macroblock address
  - `mb_type`
  - prediction mode
  - coded block pattern
  - QP / `mb_qp_delta`
  - residual CAVLC block parsing/counting for common 4:2:0 streams
  - P-slice L0 motion vector differences for supported partition types
- Analysis overlays:
  - macroblock grid
  - macroblock QP heatmap
  - P-slice L0 motion vectors
  - overlay opacity control
- Export features:
  - selected frame JSON with schema/version, stream metadata, codec-neutral
    `frame_analysis`, and H.264 codec-specific details
  - batch JSON export for all decoded frame analysis
  - frame list CSV from internal `FrameAnalysis` data
  - screenshot including visible overlays
- UI settings persisted with `QSettings`:
  - window geometry
  - dock positions
  - overlay toggles
  - opacity
  - last open/export directories
- Windows portable deployment script:
  - `scripts/deploy-windows-msys2.ps1`
  - output under `dist/ZStreamEye-windows-ucrt64`
  - zip at `dist/ZStreamEye-windows-ucrt64.zip`
- CTest parser tests for Exp-Golomb, Annex B, AVCC, and SPS dimensions.
- Tiny checked-in parser fixtures under `tests/fixtures/` for Annex B, AVCC,
  CAVLC I/P macroblocks, P-slice motion vectors, and unsupported CABAC
  diagnostics.
- Truncated P-slice fixture coverage verifies structured `slice_data_truncated`
  diagnostics and keeps estimated macroblock data instead of dropping the
  interrupted macroblock.
- Truncated slice-header fixture coverage verifies structured
  `slice_header_truncated` diagnostics without inventing macroblock data.
- Malformed AVCC length-prefix fixture coverage verifies
  `avcc_nalu_length_exceeds_packet` frame-level diagnostics, exposed through
  both H.264 syntax export and codec-neutral `FrameAnalysis`.
- Truncated SPS/PPS fixtures verify `sps_truncated` and `pps_truncated`
  diagnostics, and invalid parameter sets are not cached.
- GitHub Actions workflow for Windows MSYS2 build/test/package artifact.
- Tag-triggered release workflow for versioned Windows portable zips.

## Build And Verification

Use MSYS2 UCRT64 on Windows:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64 -DBUILD_TESTING=ON"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ctest --test-dir build-msys2-ucrt --output-on-failure"
```

Run from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

Create portable package:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy-windows-msys2.ps1
```

Do not distribute `build-msys2-ucrt/ZStreamEye.exe` alone. Use the portable folder or zip in `dist/`.

## Important Rules

- Do not commit `build*/`, `build-msys2-ucrt/`, `dist/`, or generated package artifacts.
- Do not use FFmpeg's H.264 parser as the main syntax parser. `H264Parser` is intentionally a direct bitstream parser.
- Keep decoding, seeking, and heavy parsing off the UI thread.
- Parser failures must be fault tolerant: log or mark unsupported syntax, do not crash.
- Reuse `VideoCanvas` coordinate helpers:
  - `videoDisplayRect()`
  - `mapVideoPointToWidget()`
  - `macroblockWidgetRect()`
- After meaningful code changes, run build and tests. If deployment script changes, run the deployment script too.

## Highest Priority Improvements

### 1. Real Random Access And Seeking

Current behavior:

- The app has a recent decoded frame cache.
- If the user selects an old frame that was evicted, `MainWindow::seekToFrame()`
  picks the nearest previous keyframe/IDR checkpoint and buffers from there.
- If FFmpeg cannot seek to the selected checkpoint, the worker falls back to
  decoding from frame 0.

Implemented:

- Records packet/frame checkpoints while decoding.
- Keeps keyframe/IDR packet timestamps and byte offsets where FFmpeg provides them.
- Restarts rebuffer decoding from the nearest previous checkpoint instead of
  always restarting from frame 0.
- Carries SPS/PPS parser state in checkpoints so syntax parsing can resume after
  a seek.

Recommended remaining improvement:

- Add cancellation if the user clicks another frame while a checkpoint rebuffer
  is already in progress.
- Add visible progress reporting during long checkpoint rebuffer seeks.
- Add more real stream smoke tests for raw Annex B `.264` files and containers.
- Keep UI behavior unchanged: selecting an old frame should show buffering, then land on that frame and pause.
- Be careful: H.264 raw `.264` streams may not have container timestamps or seek indexes, so Annex B byte offsets and IDR detection matter.

Suggested files:

- `src/core/FFmpegDecoder.*`
- `src/core/DecodeWorker.*`
- `src/app/MainWindow.*`
- `src/core/H264Parser.*`

### 2. Parser Correctness And Coverage

Current limitations:

- CAVLC residual parsing now consumes residual blocks and counts coefficients so
  macroblock parsing can continue, but individual residual coefficient values
  are not yet represented in the public syntax model.
- CABAC macroblock parsing is not implemented.
- B-slice motion vectors are not implemented.
- P_8x8 and sub-macroblock prediction parsing are not implemented.
- MBAFF/interlaced and FMO are not implemented.
- Some fields are safely skipped or summarized instead of fully represented.

Recommended next parser milestones:

1. Expose residual coefficient details where useful instead of only block/coeff counts.
2. Implement P_8x8 / P_8x8ref0 sub-macroblock parsing.
3. Add B-slice direct/list0/list1/bi prediction modes and L1 MV visualization.
4. Add CABAC support only after the CAVLC path is well tested.
5. Add richer diagnostics for unsupported syntax instead of generic notes.

Acceptance focus:

- Unsupported streams should remain usable.
- QP heatmap should become genuinely macroblock-level across entire supported frames.
- Motion vector overlay should avoid drawing guessed data as if it were fully parsed.

### 3. Test Fixtures And Regression Coverage

Current tests now combine small synthetic bit-writing tests with checked-in
hex fixtures under `tests/fixtures/`.

Completed fixture coverage:

- Annex B with SPS/PPS/IDR
- AVCC packet with SPS/PPS
- CAVLC I-slice with non-zero `mb_qp_delta`
- CAVLC P-slice skip macroblock
- CAVLC P-slice with non-zero L0 motion vector
- Unsupported CABAC stream that reports a structured diagnostic and does not crash

Recommended remaining improvement:

- Add multi-macroblock and multi-frame fixtures.
- Add decoded-real-world samples where redistribution is legally safe.
- Expand assertions as parser coverage grows:
  - residual CAVLC coefficient totals
  - sub-macroblock P_8x8 motion vectors
  - B-slice L0/L1/bi prediction modes
  - richer unsupported diagnostics for MBAFF/FMO and truncated streams

Suggested files:

- `tests/test_h264_parser.cpp`
- `tests/fixtures/`
- `CMakeLists.txt`

### 4. Playback Performance

Current performance safeguards:

- Property tree is not rebuilt on every playing frame.
- Frame list stores frame indexes rather than full syntax objects.
- Macroblock display in the property tree is capped.
- Old frame cache misses rebuffer on demand.

Remaining optimizations:

- Avoid copying very large `FrameSyntaxInfo` objects more than needed.
- Consider `QSharedPointer<const FrameSyntaxInfo>` for cache and UI handoff.
- Consider a model/view table for frame list instead of `QTreeWidget`.
- Add a bounded syntax cache separate from decoded image cache.
- Add cancellation if the user clicks another frame while a rebuffer seek is in progress.
- Add progress reporting during long rebuffer seeks.

Suggested files:

- `src/app/MainWindow.*`
- `src/ui/FrameListView.*`
- `src/ui/PropertyTreeView.*`
- `src/core/DecodeWorker.*`

### 5. Export Quality

Current exports cover selected frame JSON, all decoded frame syntax JSON, frame
list CSV, and screenshots. JSON exports include a schema version, generator
version, and stream metadata.

Implemented:

- Add JSON schema version and stream metadata.
- Add frame list CSV export from an explicit internal frame model rather than reading UI text.
- Add batch export for all decoded frame syntax.
- Add export failure details to status bar and log.
- Preserve last export directory separately from last open directory.

Recommended remaining improvement:

- Add screenshot size options and overlay-only export.
- Consider moving export serialization into `src/core/ExportWriter.*` once more
  export formats are added.

Suggested files:

- `src/app/MainWindow.*`
- potential new `src/core/ExportWriter.*`

### 6. UI/UX Polish

Recommended improvement:

- Add a real timeline/seek bar separate from the frame list.
- Add visible buffering state during rebuffer seek.
- Add overlay legend for QP heatmap and MV colors.
- Add option to cap MV drawing density for high-resolution clips.

Implemented:

- Disabled/enabled states for export actions when no frame/syntax is available.
- Persist UI settings with `QSettings`:
  - window geometry
  - dock positions
  - overlay toggles
  - opacity
  - last open/export directories

Suggested files:

- `src/app/MainWindow.*`
- `src/ui/VideoCanvas.*`

### 7. CI And Release Hardening

Current CI:

- Windows MSYS2 configure/build/test/package workflow exists.
- Portable zip is uploaded as a retained, SHA-named artifact.
- Release workflow creates versioned Windows portable zips for `v*` tags.

Implemented:

- Add workflow status badge to README.
- Add release workflow triggered by tags.
- Add artifact retention and versioned zip names.

Recommended remaining improvement:

- Run tests on Linux too if Qt/FFmpeg packages are stable enough.
- Cache MSYS2 packages if workflow time becomes painful.
- Add a small smoke test that runs the parser test binary from the packaged environment.

Suggested files:

- `.github/workflows/windows-msys2.yml`
- `scripts/deploy-windows-msys2.ps1`
- `README.md`

## Lower Priority Ideas

- Add a codec-neutral `FrameAnalysis` model before exposing HEVC/H.265 in the UI.
- Add support for HEVC/H.265 as a separate parser module after H.264 is more mature.
- Add side-by-side frame comparison.
- Add search/filter in property tree.
- Add bitstream hex view synchronized with syntax fields.
- Add per-field bit offset navigation.
- Add chart view for QP distribution and frame-type distribution.
- Add JSON import/replay mode for previously exported analysis.
- Add command-line analysis mode for CI or batch use.

## Suggested Next Commit Plan

Use focused commits. Good examples:

```text
Add indexed frame seek checkpoints
Parse CAVLC residual coefficients
Continue macroblock parsing after residual data
Parse P8x8 sub-macroblock motion vectors
Add parser fixture bitstreams
Persist overlay and window settings
Improve export writers and metadata
Add release workflow for tagged builds
Introduce codec-neutral parser interface
Add codec-neutral frame analysis model
```

## Quick Orientation For Future Agents

Most important files:

```text
src/app/MainWindow.*
src/core/AnalysisExportWriter.*
src/core/DecodeWorker.*
src/core/FFmpegDecoder.*
src/core/BitstreamParser.*
src/core/FrameAnalysis.*
src/core/H264FrameAnalysisAdapter.*
src/core/H264Parser.*
src/ui/VideoCanvas.*
src/ui/FrameListView.*
src/ui/PropertyTreeView.*
tests/test_h264_parser.cpp
scripts/deploy-windows-msys2.ps1
.github/workflows/windows-msys2.yml
```

Before changing parser behavior, read:

- `H264Parser::parseSliceHeader`
- `H264Parser::parseSliceData`
- `MacroblockInfo`
- `SliceInfo`
- `VideoCanvas::drawQpHeatmap`
- `VideoCanvas::drawMotionVectors`

Before changing playback/seek behavior, read:

- `MainWindow::handleFrameReady`
- `MainWindow::showFrameFromCache`
- `MainWindow::seekToFrame`
- `DecodeWorker::decodeFileFromFrame`

The project is now beyond the original Stage 1-7 checklist. Future work should focus less on adding menu items and more on correctness, parser depth, seek performance, and regression coverage.
