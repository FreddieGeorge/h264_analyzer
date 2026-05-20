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
  - `MediaKind`
  - `AccessUnitKind`
  - `IBitstreamParser`
  - `FrameAnalysis`
  - parser state snapshots for checkpoint resume
  - `FFmpegDecoder` no longer owns `H264Parser` directly
- Parser implementations are organized by media kind: audio parsers under
  `src/core/parser/audio`, video parsers under `src/core/parser/video`, and
  codec-neutral low-level helpers under `src/core/util`.
- H.264 analysis is adapted into codec-neutral `FrameAnalysis` through
  `H264FrameAnalysisAdapter`, while retaining rich H.264 structs as
  codec-specific details.
- `StreamInfo` and `FrameAnalysis` now carry media/access-unit fields
  (`mediaKind`, `streamIndex`, `accessUnitKind`) plus placeholder audio stream
  metadata. Current decoding is still video-only, but the export/property-tree
  model no longer assumes that every future access unit must be a video frame.
- `FFmpegDecoder` discovers all container streams and stores them in
  `StreamInfo::streams`, including basic video/audio metadata. It still selects
  the best video stream for decoding by default.
- HEVC/H.265 has a parser skeleton wired into `FFmpegDecoder`: it identifies
  Annex B or length-prefixed NAL units, reports VPS/SPS/PPS parameter-set units,
  marks VCL access units as frames, and emits graceful unsupported diagnostics
  for deeper slice syntax.
- AAC ADTS and MP3 have audio parser skeletons for access-unit level analysis.
  AAC parses the 7-byte ADTS header; MP3 parses the 4-byte MPEG audio frame
  header. Both emit codec-neutral bit fields, mark valid packets as
  `MediaKind::Audio` / `AccessUnitKind::AudioFrame`, and report structured
  malformed-header diagnostics.
- `FFmpegDecoder` keeps video decoding as the selected playback path, but it
  now parses discovered AAC/MP3 audio packets opportunistically while reading
  container packets for video. `DecodeWorker` emits those audio
  `FrameAnalysis` records separately, and `MainWindow` lists them in
  `FrameListView` without routing them through `VideoCanvas` or old-frame
  rebuffering.
- The access-unit list now has a toolbar stream selector plus All / Video /
  Audio / Diagnostics-only filtering. This is a list/analysis filter over
  discovered streams; it does not yet switch the FFmpeg playback stream or add
  audio-only analysis mode.
- `FrameAnalysis` carries packet metadata and raw packet bytes through
  `PacketRawData`: container packet index, per-stream packet index, stream
  index, media kind, codec kind, PTS/DTS, duration, byte position, packet size,
  keyframe flag, and raw bytes. JSON/CSV exports include packet metadata and
  raw-byte size, not full raw bytes.
- `BitstreamHexView` is a read-only hex dock for the selected access unit's
  packet bytes. It renders bounded 4096-byte pages, highlights the byte range
  for a selected `AnalysisBitField`, shows the selected bit/byte range plus an
  ASCII bit mask preview, and lets a hex-byte click select a covering syntax
  field in `PropertyTreeView`. When multiple fields cover the same byte, it
  opens a small selection menu. It does not yet provide graphical sub-byte
  decoration or full offset normalization for macroblock fields/container
  wrappers.
- `AnalysisBitField` now carries `offsetBasis`. AAC/MP3 header fields are
  packet-relative. H.264 SPS/PPS/slice fields are currently RBSP-relative
  because they come from the post-NAL-header, emulation-prevention-stripped
  reader, but the parser now also records normalized packet bit ranges for
  SPS/PPS/slice header fields and selected macroblock syntax fields using an
  EBSP/RBSP mapping table. The hex dock uses those packet bit ranges for
  accurate raw packet highlighting, including fields split by
  emulation-prevention bytes.
- H.264 `PropertyTreeView` summary rows now carry bit-field roles directly for
  common SPS/PPS/slice-header/macroblock fields. Users should be able to click
  rows such as `slice_type`, `frame_num`, `pic_order_cnt_lsb`, `mb_type`,
  `coded_block_pattern`, and `mb_qp_delta` directly to drive hex highlighting;
  avoid reintroducing a separate visible `Bit positions` subtree for normal
  H.264 navigation.
- Custom H.264 parser for Annex B and AVCC/length-prefixed packets.
- H.264 parser implementation is split by syntax responsibility:
  - `src/core/parser/video/h264/H264Parser.cpp`: packet/NALU dispatch, decoder
    configuration parsing, parser state snapshots, SPS/PPS cache management,
    and top-level H.264 naming/mapping helpers.
  - `src/core/parser/video/h264/H264ParameterSetParser.cpp`: SPS/PPS/VUI parsing.
  - `src/core/parser/video/h264/H264SliceHeaderParser.cpp`: slice header parsing,
    reference-list modification summaries, prediction-weight-table summaries,
    and decoded-reference-picture-marking summaries.
  - `src/core/parser/video/h264/H264MacroblockParser.cpp`: `slice_data`, macroblock,
    focused CAVLC residual counting, QP, and motion-vector parsing.
- Codec-neutral parser utilities now live under `src/core/util`:
  - `BitReader.*`: bit reading plus Exp-Golomb helpers and fixed-offset
    `readBitsAt()` for audio headers.
  - `ByteStream.*`: Annex B start-code helpers and big-endian length reading.
  - `Rbsp.*`: EBSP-to-RBSP conversion and RBSP-to-packet bit-range mapping for
    hex highlighting.
- SPS/PPS/Slice Header parsing with VUI/timing/aspect/bitstream restriction and field bit metadata where practical.
- Basic CAVLC `slice_data` parsing for common baseline/main-profile paths:
  - macroblock address
  - `mb_type`
  - prediction mode
  - coded block pattern
  - QP / `mb_qp_delta`
  - residual CAVLC block parsing/counting for common 4:2:0 streams
  - P-slice L0 motion vector differences for supported partition types,
    including focused CAVLC P_8x8/P_8x8ref0 sub-macroblock fixtures
  - focused CAVLC B-slice L0/L1/Bi motion vector parsing for non-direct
    16x16/16x8/8x16 macroblocks
- Analysis overlays:
  - macroblock grid
  - macroblock QP heatmap
  - P-slice L0 motion vectors
  - overlay opacity control
- Overlay availability is now explained outside the OpenGL canvas:
  - `PropertyTreeView` has an `Overlay Availability` section with macroblock
    region counts, fully parsed macroblock counts, QP range/constant notes,
    motion-vector availability, and diagnostics counts.
  - QP heatmap notes explain flat-color frames when QP is constant, and range
    frames when QP varies.
  - Motion-vector notes distinguish I-frames, B-slice unsupported parsing,
    CABAC unsupported parsing, unsupported P sub-macroblock types, and
    MBAFF/FMO unsupported parsing where diagnostics are available.
  - Status bar hints mirror the current QP/MV overlay state using short text.
- `PropertyTreeView` supports word-wrapped values and a right-click copy menu
  for cell, row, or row-with-children text.
- Frame list selection now avoids forcing selected old/rebuffered frames to
  the middle of `FrameListView` during paused/manual navigation where possible.
  A decoder generation guard ignores stale queued frame/seek callbacks from an
  older worker after a new rebuffer decode starts.
- Export features:
  - selected access-unit JSON with schema/version, stream metadata, generic
    `frame_analysis`, and H.264 codec-specific details only for H.264 frames
  - batch JSON export for all decoded video/audio access-unit analysis
  - JSON schema v3 includes `media_kind`, `stream_index`, and
    `access_unit_kind` fields so future audio or multi-stream analysis can be
    represented without overloading video-only fields.
  - stream metadata export includes the discovered container stream list and
    selected stream marker.
  - access-unit list CSV from internal `FrameAnalysis` data
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
- CTest coverage includes direct util tests for `BitReader`, `ByteStream`, and
  `Rbsp`, plus parser tests for Exp-Golomb, Annex B, AVCC, and SPS dimensions.
- Tiny checked-in parser fixtures under `tests/fixtures/` plus synthetic
  bit-writer fixtures for Annex B, AVCC, CAVLC I/P macroblocks, P-slice motion
  vectors, P_8x8/P_8x8ref0 sub-macroblock motion vectors, focused B_Bi
  motion vectors, B_Direct unsupported diagnostics, and unsupported CABAC
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
- For every release tag, add `docs/releases/<tag>.md` before pushing the tag.
  The release workflow uses that file as the GitHub Release body and fails if
  it is missing. Do not rely on auto-generated release notes.

## Highest Priority Improvements

Recommended next direction for the next AI/coding session:

1. First verify the `v0.1.8` GitHub release artifacts and upgrade path. Install
   or upgrade from `v0.1.7`, confirm that the install root keeps only the
   launcher and support files, and confirm that Qt/FFmpeg/MSYS2 DLLs live under
   `runtime/`.
2. Broaden old-frame seek/rebuffer validation. Explicit cancellation, status-bar
   buffering progress, generation-guarded callbacks, and
   `ZStreamEyeRebufferStateTests` are in place; remaining work is UI/integration
   smoke coverage with real raw Annex B `.264` files and indexed `.mp4`/`.mkv`
   content.
3. Continue H.264 correctness work before adding a full new codec. Prefer
   residual coefficient details, broader P/B sub-macroblock fixtures,
   B_Direct/B_8x8 support, and richer diagnostics over CABAC.
4. Expand the audio stream workflow on top of the AAC/MP3 skeletons: explicit
   stream selection, better packet/access-unit filtering, and deeper codec
   details. Keep this path separate from `VideoCanvas` and `DecodedVideoFrame`.
5. Start Stage 3 only after the seek/rebuffer path is comfortable: add a
   bitstream hex dock and connect existing syntax field bit offsets to
   selection/highlighting.
6. Use a thin HEVC/unknown-codec parser skeleton later to prove that the
   codec-neutral `FrameAnalysis` UI/export path degrades gracefully, but do not
   start a large HEVC parser before the H.264 parser is more trusted.

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
- FrameListView selection synchronization now has a no-scroll mode used during
  paused/manual selection and rebuffer completion.
- MainWindow tracks decoder generations so stale queued callbacks from an older
  worker are ignored after a newer decode/rebuffer starts.
- Starting a new old-frame rebuffer cancels the previous worker early, records
  the canceled target, and shows status-bar progress from checkpoint/start
  frame to target frame. `RebufferState` holds the pending target/generation
  logic and is covered by `ZStreamEyeRebufferStateTests`.

Recommended remaining improvement:

- Add broader UI/integration coverage for repeated old-frame clicks while a
  checkpoint rebuffer is already running. The core state behavior is covered:
  repeated old-frame requests cancel the previous target, stale
  generation/target progress and completion are ignored, and progress is
  clamped to the checkpoint-to-target range.
- Add more real stream smoke tests for raw Annex B `.264` files and containers.
- Keep UI behavior unchanged: selecting an old frame should show buffering, then land on that frame and pause.
- Be careful: H.264 raw `.264` streams may not have container timestamps or seek indexes, so Annex B byte offsets and IDR detection matter.

Manual smoke path for the current rebuffer behavior:

1. Open a long raw Annex B `.264` stream and wait until early frames are evicted
   from the recent cache.
2. While paused, click several far-apart old frames in quick succession. The
   status bar should report buffering progress for only the newest target, the
   selected frame should land paused, and old queued callbacks must not replace
   the current image/property tree.
3. Repeat the same quick-click path with indexed `.mp4` or `.mkv` H.264 content.
   Check that the frame list does not jump to the middle after manual selection
   or rebuffer completion.
4. Repeat with a non-H.264 video. Playback should still work, and the property
   tree should continue to report that bitstream analysis is unsupported.

Suggested files:

- `src/core/decode/FFmpegDecoder.*`
- `src/core/decode/DecodeWorker.*`
- `src/app/MainWindow.*`
- `src/core/parser/video/h264/H264*.cpp`

### 2. Parser Correctness And Coverage

Current limitations:

- CAVLC residual parsing now consumes residual blocks and counts coefficients so
  macroblock parsing can continue, but individual residual coefficient values
  are not yet represented in the public syntax model.
- CABAC macroblock parsing is not implemented.
- Focused CAVLC B-slice L0/L1/Bi motion vector parsing is implemented for
  non-direct 16x16/16x8/8x16 macroblocks. B_Direct and B_8x8 remain
  unsupported with structured diagnostics.
- Focused CAVLC P_8x8 / P_8x8ref0 sub-macroblock L0 motion vector parsing is
  implemented; broader sub-macroblock prediction coverage still needs work.
- MBAFF/interlaced and FMO are not implemented.
- Some fields are safely skipped or summarized instead of fully represented.

Recommended next parser milestones:

1. Expose residual coefficient details where useful instead of only block/coeff counts.
2. Expand P_8x8 / P_8x8ref0 sub-macroblock parsing coverage beyond the current
   focused CAVLC L0 fixtures.
3. Expand B-slice support to B_Direct and B_8x8/sub-macroblock prediction
   modes, and refine L1 MV visualization.
4. Add CABAC support only after the CAVLC path is well tested.
5. Add richer diagnostics for unsupported syntax instead of generic notes.

Acceptance focus:

- Unsupported streams should remain usable.
- QP heatmap should become genuinely macroblock-level across entire supported frames.
- Motion vector overlay should avoid drawing guessed data as if it were fully parsed.

### 2A. Multi-Codec And Audio Foundation

Current foundation:

- `MediaTypes.*` defines `MediaKind` and `AccessUnitKind`.
- `StreamInfo` carries `mediaKind`, `streamIndex`, and placeholder audio
  metadata (`sampleRate`, `channels`, `sampleFormatName`,
  `channelLayoutName`).
- `StreamInfo::streams` records all discovered container streams with basic
  video/audio metadata and a `selected` marker for the stream currently decoded.
- `FrameAnalysis` carries `mediaKind`, `streamIndex`, and `accessUnitKind`.
- Export schema v3 writes stream/access-unit media fields, and
  `PropertyTreeView` shows them in the summary.
- `AacAdtsParser` validates the audio access-unit extension path by emitting
  AAC ADTS header bit fields, an `adts_frame` analysis unit, and structured
  diagnostics without requiring decoded audio samples.
- `Mp3FrameParser` follows the same pattern for MPEG audio frame headers,
  emitting an `mp3_frame` analysis unit plus bitrate/sample-rate/channel-mode
  header fields.
- `FrameListView` now displays video and audio access units together. Selecting
  an audio access unit updates `PropertyTreeView` and JSON export state, but it
  does not trigger video seek/rebuffer.
- `FrameListView` can filter displayed access units by stream, media kind, or
  diagnostics. Hidden audio access units remain separate from `VideoCanvas` and
  old-frame rebuffer behavior.
- `PropertyTreeView` shows packet metadata for parsed access units. This is the
  data foundation for a future hex/bit view linked to `AnalysisBitField`
  offsets.
- A first `BitstreamHexView` dock now consumes that foundation directly from
  `FrameAnalysis::packet.bytes`.

Current limitation:

- `FFmpegDecoder` discovers all streams but still selects only
  `AVMEDIA_TYPE_VIDEO`, owns a single `m_videoStreamIndex`, and emits
  `DecodedVideoFramePtr`. Audio parser work should not be bolted onto
  `VideoCanvas` or `DecodedVideoFrame`.
- Audio parsing is opportunistic during video packet reading. There is still no
  explicit stream selector, no audio-only playback/analysis mode, and no deep
  AAC raw_data_block or MP3 side-info parsing.

Recommended next step:

- Add a thin `MediaDecoder`/stream-selection layer or split the current
  `FFmpegDecoder` video path from packet/access-unit parsing. The current UI
  stream selector filters the access-unit list only; use the existing AAC/MP3
  skeletons as the first audio proofs for actual stream switching or
  audio-only analysis before adding deep audio syntax.
- For deeper HEVC work, extend the existing skeleton from NALU/VPS/SPS/PPS/VCL
  detection into SPS dimensions, slice headers, and CTU regions. For AV1, start
  with OBUs and superblock regions; for VP9, start with frame headers and
  superblock regions.

Suggested files:

- `src/core/model/MediaTypes.*`
- `src/core/model/StreamDocument.*`
- `src/core/model/FrameAnalysis.*`
- `src/core/export/AnalysisExportWriter.*`
- future `src/core/parser/audio/AacParser.*`
- `src/core/parser/video/hevc/HevcParser.*`

### 3. Test Fixtures And Regression Coverage

Current tests now combine small synthetic bit-writing tests with checked-in
hex fixtures under `tests/fixtures/`.

Completed fixture coverage:

- Annex B with SPS/PPS/IDR
- AVCC packet with SPS/PPS
- CAVLC I-slice with non-zero `mb_qp_delta`
- CAVLC P-slice skip macroblock
- CAVLC P-slice with non-zero L0 motion vector
- CAVLC P_8x8 / P_8x8ref0 sub-macroblock L0 motion vectors through synthetic
  bit-writer fixtures
- Focused CAVLC B_Bi L0/L1 motion vectors through synthetic bit-writer fixtures
- B_Direct unsupported diagnostics through synthetic bit-writer fixtures
- Unsupported CABAC stream that reports a structured diagnostic and does not crash
- Truncated slice headers that report `slice_header_truncated`
- Truncated P-slice data that reports `slice_data_truncated`
- Malformed AVCC length-prefixed packets that report
  `avcc_nalu_length_exceeds_packet`
- Truncated SPS/PPS NALUs that report `sps_truncated` / `pps_truncated`
  without caching invalid parameter sets

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
- Add UI/integration stress coverage for repeated old-frame rebuffer seeks.

Suggested files:

- `src/app/MainWindow.*`
- `src/ui/FrameListView.*`
- `src/ui/PropertyTreeView.*`
- `src/core/decode/DecodeWorker.*`

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
- Add visible buffering progress during rebuffer seek.
- Add non-canvas overlay legends or property/status hints for any future
  visualization modes. Avoid drawing text in `VideoCanvas` until the Windows
  OpenGL/QPainter text rendering issue is understood.
- Add option to cap MV drawing density for high-resolution clips.

Implemented:

- Disabled/enabled states for export actions when no frame/syntax is available.
- Persist UI settings with `QSettings`:
  - window geometry
  - dock positions
  - overlay toggles
  - opacity
  - last open/export directories
- `PropertyTreeView` `Overlay Availability` summary, wrapped long values, and
  right-click copy menu.
- Status bar QP/MV availability hints.
- Avoidance of OpenGL canvas text for overlay analysis hints to prevent garbled
  text on some Windows deployments.

Suggested files:

- `src/app/MainWindow.*`
- `src/ui/VideoCanvas.*`

### 7. CI And Release Hardening

Current CI:

- Windows MSYS2 configure/build/test/package workflow exists.
- Portable zip is uploaded as a retained, SHA-named artifact.
- Release workflow creates versioned Windows portable zips and Inno Setup
  installers for `v*` tags.
- Windows packages use a root `ZStreamEye.exe` launcher and group the real Qt
  app plus runtime DLLs/plugins under `runtime/`.

Implemented:

- Add workflow status badge to README.
- Add release workflow triggered by tags.
- Add artifact retention and versioned zip names.
- Add release notes files under `docs/releases/<tag>.md`; the release workflow
  uses them as GitHub Release bodies.

Recommended remaining improvement:

- After each tagged Windows release, manually smoke-test the installer and
  portable zip before announcing the release.
- Add a packaged-app smoke test that checks the root launcher starts the app and
  that no DLLs are left in the package/install root.
- Run tests on Linux too if Qt/FFmpeg packages are stable enough.
- Cache MSYS2 packages if workflow time becomes painful.
- Consider code signing later to remove the Windows "Unknown publisher" warning.

Suggested files:

- `.github/workflows/windows-msys2.yml`
- `scripts/deploy-windows-msys2.ps1`
- `README.md`

## Lower Priority Ideas

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
Add packaged Windows layout smoke validation
Add repeated old-frame rebuffer UI smoke coverage
Add AAC parser skeleton for audio access units
Expose H264 residual coefficient details
Expand HEVC skeleton with CTU regions
Expand H264 P8x8 sub-macroblock fixtures
Parse H264 B_Direct motion vectors
Parse H264 B_8x8 sub-macroblock motion vectors
Add bitstream hex dock skeleton
Link property fields to bit offsets
Add QP and frame-type statistics dock
Add HEVC parser skeleton with graceful unsupported UI
```

## Quick Orientation For Future Agents

Most important files:

```text
src/app/MainWindow.*
src/core/export/AnalysisExportWriter.*
src/core/decode/DecodeWorker.*
src/core/decode/FFmpegDecoder.*
src/core/parser/BitstreamParser.*
src/core/model/FrameAnalysis.*
src/core/parser/video/h264/H264FrameAnalysisAdapter.*
src/core/parser/video/h264/H264*.cpp
src/core/parser/video/hevc/HevcParser.*
src/core/util/*
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
