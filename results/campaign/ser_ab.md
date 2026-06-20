# SER (Shader Execution Reordering) A/B — RTX 4090 (Ada), 2026-06-20

**Setup.** Rented Vast.ai RTX 4090 (Ada, CC 8.9, driver 595, CUDA 13.2). Renderer ported + built there
(`-DCUDA_ARCH=89`, OptiX 9). SER added as one `optixReorder(hint,8)` per bounce after the scatter event
in `device/entry/raygen.cuh`, gated by `THESIS_ENABLE_SER`; coherence hint = hash of the quantised
scatter cell (escapes→0). **ncu perf counters are host-blocked on the container** (`RmProfilingAdminOnly=1`,
`ERR_NVGPUCTRPERM`) and clock-locking is not permitted, so the A/B is **wall-clock** (warmup +
11 interleaved off/on reps, 64 spp, min+median; image verified bit-identical by `oiiotool --diff`).
Image-identity ⇒ wall-clock speedup == equal-quality speedup. The 4090 is a **cross-arch probe**; all
thesis operating-point numbers remain 3090-pinned.

## Per-asset SER speedup (tuned caps, 64 spp, image-identical, 0 overflow)

| asset | off med (s) | on med (s) | speedup (med) | speedup (min) |
|---|---|---|---|---|
| cloud     | 3.274  | 2.303  | **1.42×** | 1.45× |
| tornado   | 3.060  | 2.729  | **1.12×** | 1.11× |
| explosion | 2.644  | 1.877  | **1.41×** | 1.41× |
| bunny     | 22.674 | 13.494 | **1.68×** | 1.68× |

SER helps on every asset (1.12–1.68×), **largest on the dense, deeply-overlapped bunny (1.68×)**,
smallest on the **sparse tornado (1.12×)**. The effect tracks the amount of divergent *shading* work
(dense overlap), not raw sparsity — tornado is divergent but has little shading to reorder.

## Cloud SER hint tuning (vs cloud SER-off)

| variant | speedup | note |
|---|---|---|
| 8-cell spatial (default) | **1.42×** | best |
| no-hint `optixReorder()`  | 1.25× | hint matters: +14% over no-hint |
| 4-cell  | 1.39× | |
| 16-cell | 1.35× | |
| bounce-0 only | 1.10× | per-bounce reordering is essential |

## Under product-RIS (showcase estimator)
cloud, `--ris`: **1.35×** — SER holds under the production direct-lighting estimator.

## Bunny Mitsuba VRAM (fills tab:vram), measured on the LOCAL 3090
Generated the `_sigmat` PLY (opacities_0→sigma_t_0 header rename) for the Gaussian fit; rendered
volprim_prb analog (512², meadow): **peak VRAM = 806 MiB** (same as tornado/explosion — Mitsuba's
footprint is framebuffer-dominated, asset-count-independent). So `tab:vram` now: ours below Mitsuba on
cloud (−31%) and explosion (−26%), **above** on tornado (+1.5%) and bunny (+12%).

## Thesis framing (for §6)
- SER is the §6 divergence lever the **Ampere 3090 cannot use** and the reference's **Dr.Jit backend
  cannot emit** (no reorder points). Our hand-written argmin megakernel can, because we control the trace
  points — an architectural advantage, measured: 1.12–1.68× on Ada, image-identical.
- The hand-placed spatial hint earns its keep (8-cell 1.42× vs no-hint 1.25×); per-bounce reordering is
  what matters (bounce-0-only 1.10×).
- Frame as a forward-looking cross-arch result; do NOT fold into 3090 headline numbers, and do NOT
  multiply across GPUs.
- For a "vs Mitsuba on the same GPU" headline, the clean (not-yet-run) experiment is ours+SER vs
  Mitsuba-analog **on the 4090**, equal-quality, on the cloud showcase.
