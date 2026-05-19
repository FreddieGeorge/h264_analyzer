# AI Roadmap Toward a StreamEye-Class Analyzer

This document is a staged roadmap for future AI/coding agents. It focuses on
how to evolve this project from a H.264-focused analyzer toward a broader
professional bitstream analysis tool.

The current project already has:

- Qt 6 desktop UI with frame list, property tree, log dock, and OpenGL canvas.
- FFmpeg background decoding through `DecodeWorker`.
- A codec-neutral parser interface (`IBitstreamParser`, `CodecKind`).
- A direct H.264 parser (`H264Parser`) for Annex B and AVCC packets.
- H.264 SPS/PPS/Slice Header parsing.
- Partial CAVLC macroblock parsing, residual block counting, QP, and P-slice L0 MV overlay.
- Seek checkpoints for rebuffering from keyframe/IDR positions.
- JSON/CSV/screenshot exports, persisted UI settings, Windows CI, and release workflow.

## Guiding Principle

Do not chase StreamEye by adding isolated UI buttons. Build depth first:

1. Make H.264 analysis correct and trusted.
2. Extract codec-neutral analysis models.
3. Add bit/hex synchronization and statistics.
4. Add quality comparison.
5. Add HEVC and more codecs.
6. Add automation, reports, and product hardening.

## Stage 1: Deepen H.264 Correctness

Goal:

- Make H.264 overlays and macroblock data trustworthy instead of estimated.

Main tasks:

- Implement P_8x8 / P_8x8ref0 sub-macroblock prediction parsing.
- Implement B-slice L0/L1/bi/direct motion vectors.
- Add CABAC macroblock parsing after CAVLC paths are stable.
- Add MBAFF/interlaced handling or precise structured diagnostics.
- Add FMO diagnostics or support where practical.
- Expose residual coefficient details, not only residual block/coeff counts.
- Add more malformed/truncated stream diagnostics.

Acceptance criteria:

- QP heatmap uses parsed macroblock data for supported streams.
- MV overlay distinguishes parsed vectors from unavailable/unsupported data.
- Unsupported syntax produces structured diagnostics, not generic notes.
- Regression fixtures cover I/P/B, P_8x8, CABAC unsupported/supported, and truncation.

Suggested files:

- `src/core/H264Parser.*`
- `src/ui/VideoCanvas.*`
- `src/ui/PropertyTreeView.*`
- `tests/test_h264_parser.cpp`
- `tests/fixtures/`

## Stage 2: Introduce Codec-Neutral FrameAnalysis

Goal:

- Stop forcing all analysis through H.264-shaped `NaluInfo`, `SliceInfo`, and `MacroblockInfo`.

Main tasks:

- Add a codec-neutral model, for example:

```text
FrameAnalysis
  codec
  frameIndex
  pts/dts
  frameType
  units[]          // NALU, OBU, tile group, etc.
  parameterSets[]  // SPS/PPS/VPS/sequence header, etc.
  regions[]        // macroblock, CTU, superblock
  motionVectors[]
  diagnostics[]
  bitFields[]
```

- Make `IBitstreamParser::parsePacket()` return or populate `FrameAnalysis`.
- Keep H.264-specific rich structs internally or behind codec-specific details.
- Update UI/export paths to consume codec-neutral fields where possible.
- Keep existing H.264 UI behavior during migration.

Acceptance criteria:

- H.264 still displays the same information after the migration.
- `FFmpegDecoder` and `DecodeWorker` do not need codec-specific branches for UI handoff.
- Export JSON includes a stable codec-neutral schema section.
- H.264-specific fields are nested under codec-specific details, not mixed into every generic model.

Suggested files:

- `src/core/BitstreamParser.*`
- New `src/core/FrameAnalysis.*`
- `src/core/H264Parser.*`
- `src/core/FFmpegDecoder.*`
- `src/app/MainWindow.*`
- `src/ui/PropertyTreeView.*`

## Stage 3: Add Bitstream Hex And Bit-Offset Navigation

Goal:

- Give users StreamEye-style syntax-to-bitstream traceability.

Main tasks:

- Add a hex/bitstream dock view.
- Store exact byte/bit offsets for syntax fields.
- Clicking a property tree field should highlight the bit range.
- Clicking a NALU/block/field in the hex view should select related syntax.
- Support Annex B and length-prefixed packet offsets.
- Include bit offset metadata in JSON export.

Acceptance criteria:

- SPS/PPS/Slice Header fields can navigate to exact bit ranges.
- Macroblock/slice data ranges are at least coarsely highlighted.
- Hex view remains responsive on large streams through paging or lazy loading.

Suggested files:

- New `src/ui/BitstreamHexView.*`
- `src/core/H264Parser.*`
- `src/core/FrameAnalysis.*`
- `src/app/MainWindow.*`

## Stage 4: Add Analysis Charts And Statistics

Goal:

- Provide useful encoder-analysis summaries before adding reference-quality metrics.

Main tasks:

- Add frame type distribution.
- Add QP distribution and QP-over-time chart.
- Add bitrate per frame and instant bitrate chart.
- Add macroblock/CTU/superblock type distribution.
- Add motion vector magnitude/direction distribution.
- Add residual coefficient/block count distribution.
- Add GOP structure timeline.

Acceptance criteria:

- Charts are derived from internal analysis data, not UI text.
- CSV/JSON exports include the same statistics.
- Large streams stay responsive through bounded caches or background aggregation.

Suggested files:

- New `src/core/AnalysisStats.*`
- New `src/ui/StatsDock.*`
- `src/app/MainWindow.*`
- `src/core/DecodeWorker.*`

## Stage 5: Add Reference Comparison And Quality Metrics

Goal:

- Move toward StreamEye Studio-style quality analysis.

Main tasks:

- Add reference YUV loading.
- Synchronize decoded frame and reference frame by index/PTS.
- Add PSNR.
- Add SSIM.
- Add frame difference view.
- Add heatmap/subtraction/split comparison modes.
- Later add VMAF/libvmaf, MS-SSIM, VIF, and batch quality reports.

Acceptance criteria:

- User can load encoded stream plus reference YUV and inspect per-frame metrics.
- Metrics are exportable as CSV/JSON.
- Pixel format, bit depth, resolution, and frame count mismatches are reported clearly.

Suggested files:

- New `src/core/ReferenceFrameReader.*`
- New `src/core/QualityMetrics.*`
- `src/ui/VideoCanvas.*`
- `src/app/MainWindow.*`

## Stage 6: Add HEVC/H.265 Parser Module

Goal:

- Become a multi-codec analyzer instead of a H.264-only tool.

Prerequisite:

- Stage 2 should be mostly complete before starting this.

Main tasks:

- Add `HevcParser` implementing `IBitstreamParser`.
- Parse Annex B and hvcC/length-prefixed HEVC data.
- Parse VPS/SPS/PPS.
- Parse slice segment headers.
- Add CTU grid.
- Add QP map.
- Add CU/PU/TU basic structure.
- Add L0/L1 motion vectors.
- Add reference picture set and DPB basics.

Acceptance criteria:

- HEVC streams decode and show basic frame/syntax info.
- CTU-level grid/QP overlay works for supported streams.
- H.264 behavior remains unchanged.

Suggested files:

- New `src/core/HevcParser.*`
- `src/core/BitstreamParser.*`
- `src/core/FrameAnalysis.*`
- `src/ui/VideoCanvas.*`
- `tests/fixtures/hevc/`

## Stage 7: Add Container And Stream-Level Analysis

Goal:

- Connect elementary-stream analysis with container and transport context.

Main tasks:

- Show packet offset, DTS/PTS, duration, keyframe flags, and stream index.
- Add MP4/MKV/TS frame-to-packet mapping.
- Add elementary stream extraction helpers.
- Add TS/PES continuity and timestamp diagnostics where practical.
- Add DASH/HLS input exploration later.

Acceptance criteria:

- A decoded frame can be traced back to container packets.
- Container timeline and decoded frame timeline are visible.
- Timestamp discontinuities or missing keyframe indexes are diagnosed.

Suggested files:

- `src/core/FFmpegDecoder.*`
- New `src/core/ContainerAnalysis.*`
- `src/ui/FrameListView.*`
- `src/app/MainWindow.*`

## Stage 8: Add CLI, Batch Reports, And Automation

Goal:

- Make the analyzer useful in CI, encoder regression, and batch workflows.

Main tasks:

- Add command-line analysis mode.
- Export JSON/CSV/HTML reports without opening the GUI.
- Add compare mode for two encodes.
- Add quality metric batch mode.
- Add deterministic exit codes for pass/warn/fail.

Example future commands:

```bash
H264AnalyzerCLI analyze input.264 --json report.json
H264AnalyzerCLI compare encoded.mp4 reference.yuv --metrics psnr,ssim
```

Acceptance criteria:

- Parser tests can run against a fixture corpus from the CLI.
- CI can fail on parse crashes or configured conformance errors.
- GUI and CLI share parser/export code.

Suggested files:

- New `src/cli/`
- New shared export/analysis modules under `src/core/`
- `CMakeLists.txt`
- `.github/workflows/`

## Stage 9: Product Hardening

Goal:

- Improve robustness, performance, and release confidence.

Main tasks:

- Add large-file indexing cache.
- Add seek/rebuffer cancellation and progress.
- Add bounded syntax cache separate from decoded frame cache.
- Add background parse/aggregation queues.
- Add crash-safe malformed stream handling.
- Add a legally redistributable regression corpus.
- Add Linux CI when Qt/FFmpeg packages are stable enough.
- Add release signing/installer later if needed.

Acceptance criteria:

- Long streams remain usable.
- Repeated seek/click workflows do not queue stale work.
- Malformed streams produce diagnostics rather than crashes.
- CI covers parser, packaging, and representative fixture smoke tests.

Suggested files:

- `src/core/DecodeWorker.*`
- `src/core/FFmpegDecoder.*`
- `src/app/MainWindow.*`
- `.github/workflows/`
- `scripts/`

## Suggested Next Commit Order

Good focused commits:

```text
Add codec-neutral frame analysis model
Move H264 syntax into codec-specific analysis details
Add bitstream hex view and field navigation
Track H264 DPB and reference pictures
Parse H264 P8x8 sub-macroblock motion vectors
Parse H264 B-slice motion vectors
Add QP bitrate and block-type statistics
Add reference YUV reader and PSNR metric
Add HEVC VPS SPS PPS parser skeleton
Add CLI analysis report mode
```

## Notes For Future Agents

- Preserve current H.264 behavior while refactoring.
- Keep heavy parsing and seeking off the UI thread.
- Add fixtures before or alongside parser behavior changes.
- Do not rely on FFmpeg's H.264 parser for syntax analysis.
- Prefer codec-neutral UI/export paths, with codec-specific details nested below.
- If a stream is unsupported, log structured diagnostics and keep playback usable.
