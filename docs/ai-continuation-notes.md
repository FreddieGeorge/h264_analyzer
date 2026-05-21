# AI Continuation Notes

This is the short handoff note for future AI/coding agents working on
ZStreamEye.

Repository:

```text
git@github.com:FreddieGeorge/ZStreamEye.git
```

Typical local path:

```text
D:\Desktop\ZStreamEye
```

For the longer staged roadmap, see `docs/ai-streameye-roadmap.md`. Keep this
file focused on current state, rules, and the next practical work.

## Current State

ZStreamEye is a Qt 6 / FFmpeg desktop bitstream analysis tool. The app has a
frame/access-unit list, property tree, log dock, stats dock, bitstream hex dock,
OpenGL video canvas, playback controls, overlays, JSON/CSV/screenshot export,
and persisted UI settings.

Core architecture:

- `FFmpegDecoder` and `DecodeWorker` handle background decoding and packet
  parsing. Video remains the selected playback path.
- `FrameAnalysis` is the codec-neutral handoff model used by UI, stats, and
  export.
- `StreamInfo` records discovered container streams and basic video/audio
  metadata.
- `AnalysisStats` aggregates access-unit, frame-type, macroblock, QP, motion
  vector, and diagnostic summaries.
- `BitstreamHexView` highlights selected packet byte ranges from
  `AnalysisBitField` metadata.
- Audio parser skeletons exist for AAC ADTS and MP3 frame headers. They emit
  access-unit metadata and diagnostics, but there is no audio playback or deep
  audio syntax parsing yet.
- HEVC has a shallow parser skeleton for NALU/VPS/SPS/PPS/VCL classification
  and graceful unsupported diagnostics. It does not parse full HEVC slice
  headers yet.

H.264 is the deepest parser and is intentionally a direct bitstream parser, not
a wrapper around FFmpeg's H.264 parser.

## H.264 Parser Map

Important H.264 files:

- `H264Parser.cpp`: packet/NALU dispatch, decoder configuration parsing, parser
  state snapshots, SPS/PPS cache management, and top-level helpers.
- `H264ParameterSetParser.cpp`: SPS/PPS/VUI parsing.
- `H264SliceHeaderParser.cpp`: slice header parsing, reference-list summaries,
  prediction-weight summaries, and decoded-reference-picture marking summaries.
- `H264MacroblockParser.cpp`: `slice_data` flow, skip-run handling,
  CAVLC/CABAC dispatch, and top-level unsupported/truncation handling.
- `H264SliceDataContext.h`: internal shared macroblock parsing context,
  diagnostics helpers, and small bit-field readers.
- `H264CavlcMacroblockParser.*`: CAVLC macroblock syntax orchestration,
  including macroblock type, intra/P/B syntax, and coded-block pattern flow.
- `H264CavlcMacroblockResidualParser.*`: macroblock-level CAVLC residual
  prediction/dispatch and coefficient-state updates.
- `H264CavlcResidualParser.*`: CAVLC residual block parsing and coefficient
  summaries.
- `H264MotionVectorParser.*`: motion-vector prediction, MV state updates, and
  supported P/B partition mapping.
- `H264MacroblockTypes.*`: macroblock type naming and coded-block-pattern
  mapping.
- `H264CabacContextModel.*`: CABAC context-model initialization tables/helpers.
- `H264CabacDecoder.*`: CABAC arithmetic-decoder foundation.
- `H264CabacMacroblockParser.*`: CABAC macroblock entry point. It currently
  reports structured `cabac_unsupported`.

Current H.264 coverage:

- SPS/PPS/slice headers with bit-field metadata where practical.
- Common CAVLC I/P macroblock parsing, QP, coded-block pattern, residual block
  counts, and focused non-zero coefficient summaries.
- P-slice L0 motion vectors for supported partition paths, including focused
  P_8x8/P_8x8ref0 fixtures.
- Focused non-direct B-slice L0/L1/Bi motion vectors for 16x16/16x8/8x16.
- Structured diagnostics for unsupported CABAC, B_Direct, B_8x8, MBAFF/FMO,
  malformed/truncated SPS/PPS/slice data, and malformed AVCC lengths.

Current H.264 limitations:

- CABAC macroblock parsing is not implemented. Groundwork includes
  `cabac_init_idc` on `SliceInfo`, `H264SliceDataContext`, a context-based
  CABAC unsupported entry point, `H264CabacContextModel.*`, and
  `H264CabacDecoder.*` bin-decoding primitives.
- CAVLC residual summaries are focused analysis data, not full inverse-scan,
  dequantized, or transformed residual visualization.
- B_Direct, B_8x8 sub-macroblock prediction, MBAFF/interlaced, and FMO remain
  unsupported or diagnostic-only paths.

## Build And Verification

For normal Codex/parser work, use the existing utility build:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-codex-util"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ctest --test-dir build-codex-util --output-on-failure"
```

For a fresh MSYS2 UCRT64 configure/build/test:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64 -DBUILD_TESTING=ON"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ctest --test-dir build-msys2-ucrt --output-on-failure"
```

Run the app from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

Create a portable package:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy-windows-msys2.ps1
```

The deployment script writes its own release build under
`build-deploy-msys2-ucrt` and package output under `dist/`. Do not distribute a
`ZStreamEye.exe` directly from any build directory.

## Important Rules

- Do not commit `build*/`, `build-msys2-ucrt/`, `dist/`, or generated package
  artifacts.
- Do not use FFmpeg's H.264 parser as the main syntax parser.
- Keep decoding, seeking, and heavy parsing off the UI thread.
- Parser failures must be fault tolerant: add structured diagnostics or mark
  unsupported syntax; do not crash.
- Keep parser and general code modular and decoupled.
- CABAC must stay layered: context models, arithmetic decoding, syntax
  dispatch, residual parsing, motion-vector updates, and shared slice state
  should remain separate helpers/files with focused tests.
- Reuse `VideoCanvas` coordinate helpers: `videoDisplayRect()`,
  `mapVideoPointToWidget()`, and `macroblockWidgetRect()`.
- After meaningful code changes, run build and tests. If deployment scripts
  change, run the deployment script too.
- For every release tag, add `docs/releases/<tag>.md` before pushing the tag.

## Parser Change Checklist

- Define module boundaries before implementing complex syntax.
- Keep parser functions narrow. If a function grows past roughly 120-150 lines,
  check whether it mixes syntax reading, derived state, diagnostics, shared
  mutation, and model population.
- For new syntax fields, carry data through the relevant public layers by
  default: parser model, JSON export, `PropertyTreeView`, and tests.
- Unsupported syntax should have a stable diagnostic code, clear message,
  parsed/estimated state, and fixture coverage when practical.
- Test small layers first. For CABAC, test context initialization, arithmetic
  decoder state, and narrow syntax helpers before macroblock integration.
- Keep internal parser plumbing private unless UI/export genuinely needs it.
- Do not couple parser internals to UI code.

## General Code Structure Checklist

- Keep layer ownership clear: UI presents state, decode owns FFmpeg/threading,
  parsers own syntax analysis, models carry stable data, export/statistics
  consume models.
- Prefer explicit adapters or model fields at cross-layer boundaries.
- Keep shared state explicit with small context/state structs instead of long
  parameter lists or hidden globals.
- Add abstractions only when they reduce real coupling or repeated logic.
- Test at the lowest useful layer, then add broader regression coverage.

## Next Parser Work

Recommended next H.264 direction:

1. Add `H264CabacSyntaxReader.*` for narrow syntax-element readers. Start with
   `mb_skip_flag` and `mb_type`, returning small result structs instead of
   mutating `MacroblockInfo` directly.
2. Wire only one narrow CABAC path into `H264CabacMacroblockParser`, preserving
   structured unsupported diagnostics for the first unimplemented syntax
   element.
3. Keep CABAC modules separate from CAVLC helpers. Reuse shared slice state via
   `H264SliceDataContext`, but keep CABAC-specific state and tables out of
   `H264MacroblockParser.cpp`.
4. Broaden CAVLC residual and P/B motion-vector fixtures where they expose
   assumptions needed by CABAC integration.
5. Preserve structured unsupported diagnostics for paths that are not ready.

Useful H.264 test areas:

- `tests/test_h264_parser.cpp`
- `tests/test_h264_cabac_decoder.cpp`
- `tests/fixtures/`
- `tests/test_analysis_stats.cpp`

## Other Useful Context

Playback/seek:

- Recent decoded frames are cached.
- Old-frame cache misses rebuffer from the nearest keyframe/IDR checkpoint.
- Checkpoints preserve enough parser state to resume syntax parsing after seek.
- Rebuffer generation guards prevent stale callbacks from replacing newer UI
  state.

Export/UI:

- `PropertyTreeView` and JSON export should consume stable model data rather
  than scraping UI text or parser internals.
- Avoid drawing analysis text in `VideoCanvas`; use property/status summaries
  because some Windows/OpenGL deployments render QPainter text poorly.
- `PropertyTreeView` caps displayed macroblocks for responsiveness; overlay
  data still uses the parsed macroblock list.

Release/deployment:

- Windows packaging uses a root launcher and places runtime files under
  `runtime/`.
- Release workflow expects `docs/releases/<tag>.md` for tagged releases.
- The installer is not code signed yet.

## Quick Orientation

Most important files:

```text
src/app/MainWindow.*
src/core/decode/DecodeWorker.*
src/core/decode/FFmpegDecoder.*
src/core/model/FrameAnalysis.*
src/core/analysis/AnalysisStats.*
src/core/export/AnalysisExportWriter.*
src/core/parser/BitstreamParser.*
src/core/parser/video/h264/H264*.*
src/core/parser/video/hevc/HevcParser.*
src/core/parser/audio/*
src/core/util/*
src/ui/BitstreamHexView.*
src/ui/FrameListView.*
src/ui/PropertyTreeView.*
src/ui/StatsDock.*
src/ui/VideoCanvas.*
tests/*
scripts/deploy-windows-msys2.ps1
.github/workflows/windows-msys2.yml
```

Before parser changes, read the relevant parser module plus `SliceInfo`,
`MacroblockInfo`, `FrameAnalysis`, `AnalysisExportWriter`, and
`PropertyTreeView`.

Before playback/seek changes, read `MainWindow::handleFrameReady`,
`MainWindow::showFrameFromCache`, `MainWindow::seekToFrame`, and
`DecodeWorker::decodeFileFromFrame`.
