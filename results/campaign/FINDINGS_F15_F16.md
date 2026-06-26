# F15 (half-precision compression) & F16 (fixed-buffer + bitmask traversal) — 2026-06-26

## F15 — half-precision primitive fields (Jorge #5): grounded NULL, accuracy-safe but no lever
- **Accuracy (fp16 round-trip):** optical_thickness max rel err 0.0, albedo 1.1e-4, density_norm_factor
  3.3e-4. fp16 on the scalar/colour fields is comfortably accuracy-safe.
- **VRAM:** the Primitive struct is 80 B; per-asset primitive footprint is 52 KB (cloud, 652p) … 2.0 MB
  (bunny, 25600p) — **<=0.25% of the ~800 MB total VRAM** (dominated by the OptiX GAS, framebuffers, env
  map). Halving the struct saves <=1 MB → negligible.
- **Perf:** the kernel is latency-/divergence-bound, and the primitive data is already L1≈82% / L2≈99%
  resident (sec:bottleneck) — the thesis already concludes "reducing its footprint cannot help much."
  fp16 reduces footprint → predicted no perf gain.
- **Verdict:** compression is a grounded NULL — accuracy-safe but there is **no footprint lever** to pull
  for these assets, so it is not worth destabilizing the perf-tuned, standard-layout struct (hot 64 B in
  two L2 sectors). Recorded as a *reasoned decision* (Ch6 taxonomy), reinforcing the sec:bottleneck
  diagnosis. (Would only matter for assets where primitives dominate VRAM — not the case here.)

## F16 — fixed-50 buffer + global bitmask, ceil(H/50) re-traversals (Jorge #3): design + feasibility
**Design.** Replace the per-asset compile-time `HIT_BUFFER_CAPACITY` with a fixed small hit buffer (~50)
plus a per-ray bitmask over primitives; optixTrace returns hits unordered, so collect up to 50, mark them
in the bitmask, and if the ray crossed more than 50 (H>50) re-traverse skipping already-seen prims (any-hit
checks the bitmask), repeating ceil(H/50) times. The argmin then reduces over the batches.

**Feasibility / predicted outcome (analysis):**
- **Memory does NOT clearly improve for the thesis assets.** The per-ray bitmask is N bits = N/8 bytes
  (Jorge's "N/64 words"). For bunny N=25600 → **3.2 KB/ray**, which equals the current 528-entry buffer
  (528×6 B ≈ 3.2 KB). So the fixed-50 buffer's saving is cancelled by the bitmask for high-N assets; only
  low-N assets gain, where the current caps are already small.
- **Time likely WORSE.** ceil(H/50) re-traversals add BVH descents on the deep-overlap rays (the cloud's
  worst chord and the bunny shell), on the hot path. This is the same trade that made the **cap-free
  streaming** variant (bit-exact but per-ray re-collection) **12–22% slower** on small assets and led to
  it being abandoned (project_capfree_streaming). The bitmask variant is a sibling of that and is predicted
  to land in the same place.
- **Correctness risk.** It is a rewrite of validated hot-path traversal (collect_hits + any-hit + the
  argmin batching); the argmin must see every pre-scatter hit across batches. Must be verified bit-identical
  (the megafunction-refactor protocol applies: render cloud_asset_scattering, cmp EXR vs baseline on
  MIS/RIS/analog).

**Recommendation.** Do NOT rush this into the validated traversal under the current session. It is the one
item whose honest evaluation needs a dedicated, isolated implementation + bit-identical/parity pass. Given
the analysis predicts no net win (memory neutral for high-N, extra traversals cost time, mirrors the
abandoned cap-free streaming), it is a *candidate negative result*; worth implementing only if we want the
measured confirmation as a thesis negative-ledger entry (like the cap-free streaming archive). Flagged as a
scoped follow-up.

## F16 — FRESH TEST (2026-06-26): the approach-class measured, current hardware
The cap-free-streaming worktree (feature/cap-free-streaming) IS the F16 approach realized: fixed hit
buffer + in-anyhit argmin + a second re-traversal for the rebuild, NO per-asset HIT_BUFFER_CAPACITY (one
universal binary). Re-measured vs the per-asset-tuned cloud binary (exe_cloud), cloud meadow 64spp seed42:
- Correctness: cap-free mean 0.3216 == tuned 0.3215 (matches to 1e-4 -> same image; the archived gate
  capfree_b_gate.md certified it bit-exact vs its contemporary baseline; vs today's main it differs only
  by unrelated code drift, RIS/fast-erf/refactor).
- Timing (interleaved ABAB x4): tuned 6.88/7.11/7.21/7.34 s vs cap-free 7.93/8.10/8.18/8.23 s ->
  **+15.4 / +14.0 / +13.5 / +12.1 % (avg +13.7%)** -- cap-free is consistently slower, reproducing the
  documented +12.2% cloud (and +21.8% tornado / +15.9% explosion) regression. Mechanism: the extra
  re-traversal descent is compute-bound (+5.9% instructions, -8% L2 bytes; ncu).
- Jorge's BITMASK variant shares this re-traversal cost and adds an N/8-byte/ray bitmask + per-hit
  set/test ops on top, so it is BOUNDED BELOW by these numbers -- it cannot beat per-asset caps on time.

### F16 VERDICT (tested): negative on speed, positive on generality.
The fixed-buffer + re-traversal approach (cap-free streaming, and a fortiori the bitmask variant)
eliminates per-asset cap calibration (one universal binary, zero overflows on all assets) but costs
~12-22% render time from the extra BVH descent. It is a genuine NEGATIVE RESULT for speed; its value is
generality. Recommendation: keep per-asset caps as the shipped default; document cap-free/bitmask in the
negative-results ledger (the cap-free branch is already archived for exactly this).
