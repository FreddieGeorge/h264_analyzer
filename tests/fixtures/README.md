# H.264 Parser Fixtures

These tiny text fixtures store H.264 packets as hexadecimal bytes so they remain
reviewable in source control.

They are intentionally minimal parser fixtures, not visual-quality video clips:

- `annexb_sps_pps_idr_i.hex`: Annex B SPS/PPS/IDR I-slice with one 16x16 macroblock.
- `avcc_sps_pps.hex`: AVCC/length-prefixed SPS/PPS packet.
- `cavlc_i_qp_delta.hex`: CAVLC I-slice that exposes a non-zero `mb_qp_delta`.
- `cavlc_p_skip.hex`: CAVLC P-slice skip macroblock.
- `cavlc_p_motion_vector.hex`: CAVLC P-slice with a non-zero L0 motion vector delta.
- `cavlc_p_residual_then_motion_vector.hex`: CAVLC P-slice that verifies residual parsing can continue into a later motion-vector macroblock.
- `unsupported_cabac_p.hex`: CABAC P-slice used to assert graceful unsupported diagnostics.
- `truncated_p_slice_data.hex`: P-slice cut off during `slice_data`, used to assert structured truncation diagnostics.
