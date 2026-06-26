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
