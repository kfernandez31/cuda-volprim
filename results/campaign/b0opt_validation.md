# Bounce-0 camera-set precompute — implementation + validation record (2026-07-29)

Branch: feature/bounce0-camera-set (4 files + 2 new kernel files; see git diff).
Design: tier-3 from the §7.6 analysis. A perspective camera gives every camera ray the
same origin, so the origin-inside scan of build_origin_inside_set (the a·N per-sample
term of the scaling study) is a per-launch constant. A single-thread kernel
(device/kernels/camera_active_set.cu) computes it once per render with the SAME device
predicate in the SAME ascending order (RNG draw order downstream is load-bearing);
the megakernel copies the list at bounce 0. Fallbacks keep the scan: orthographic
cameras (per-pixel origins), --measure-caps (records true origin overlap), and
BitVector builds (MAX_PRIMITIVES <= 256; insert-refusal semantics differ).

## Validation (cloud, 652 prims, asset_validation recipe, 150 W informal)
- Absorption 16 spp seed 0:  stock vs opt EXR **byte-identical** (cmp).
- Scattering 64 spp seed 0 (albedo 0.9, white env, sigma 10): **byte-identical** (cmp).
- --measure-caps on branch: scan path retained, reports hits/ray 53, overlap 15 (sane).
- Informal single-run walls @150 W: scattering 4.871 -> 3.856 s (-21 %); absorption
  0.122 -> 0.055 s (-55 %).

## Note on magnitude vs prediction
§7.6's fitted model predicted ~5 % for the cloud; informal measurement shows ~21 %.
Candidate explanations: (a) the joint fit under-attributes the scan on production scenes
(cube/stack collinearity); (b) removing the scan loop lowers register pressure in the
megakernel and lifts occupancy — a superlinear effect outside the linear model.
The controlled experiment (below) decides; thesis text updates ONLY from that.

## Pending: controlled A/B (needs a 350 W locked-clock window, ~30-45 min)
Per-asset interleaved stock-vs-opt at the tab:asset-cost recipe (scattering, albedo 0.9,
white env, sigma 10, 512x512, 64 spp), per-asset CALIBRATED caps (tab:overlap:
cloud 64/96, tornado 112/384, explosion 32/160, bunny 80/528), warm-up + 5 retained
repeats per arm per asset. Binaries: 8 stashes built by scripts/campaign/build_b0opt_pairs.sh.
Stock EXR refs banked: scratchpad b0opt/ (volatile) — re-derive via seed 0 if needed.
