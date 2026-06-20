# Plot/figure/table audit — open items (2026-06-17)

Kacper's float-by-float review of the thesis figures/tables/algorithms. Criterion per float:
*is it worth having?* and *is it well executed?* This doc lists what's **still open** after the
session (for a fresh, post-compaction context to pick up), plus what's already done so nothing is redone.

Build state: thesis compiles clean (0 errors, 0 undefined), 69 pp. All edits below are uncommitted.

---

## OPEN — needs a decision and/or GPU work

### 1. `tab:vram` (§7.4 memory) — Mitsuba column measured only for the cloud
- **Issue:** the Mitsuba column has 838 for the cloud and `---` for tornado/explosion/bunny. Kacper wants
  the **other assets measured too** (not the column removed — a removal was done then reverted).
- **Action:** measure Mitsuba peak VRAM for tornado, explosion, (and bunny if feasible) under matched
  settings, fill the column, drop the "dashes mark polls not yet taken" caption clause.
- **How:** render each asset in Mitsuba `volprim` (`tools/refs/.venv-volprim`, `render_asset_via_prb.py`
  with the native renamed PLY — tornado/explosion are already wired in `run_g10_parity.sh`/`asset_parity.md`)
  and poll **peak GPU VRAM** during the render (GPU-wide poll, like `scaling.py::poll_peak` or
  `run_g5b_vram.sh`). VRAM is **clock-independent** → runnable even at the 150 W cap.
- **Caveats:** Mitsuba's poll is GPU-wide vs ours per-process (asymmetry already noted in the caption —
  it only inflates Mitsuba, so the "below reference" margin stays conservative). **Bunny** was never run
  against Mitsuba (ambiguous native fits) — may need a one-off Mitsuba bunny setup or stay dashed with a
  note.

### 2. `fig:rr-depth` (§6.x, Russian-roulette efficiency) — no depth-14 point + contaminated timing
- **Issue:** sweep is {5,6,8,10,12,16}; **no 14** (visible gap 12→16). Separately, that sweep's *timing*
  was partly contaminated by a desktop-session burst (`rr_depth.md`: rescued by per-block-normalized
  medians; `k` is image-derived and immune). Record already flags an optional "timing-only rerun on a
  quiet GPU (~10 min)".
- **Kacper leans: redo it.** Re-render the sweep on a **quiet, locked-clock GPU** (`lock_clocks.sh`),
  *including depth 14* (and optionally a uniform grid, e.g. {5,6,8,10,12,14,16}), recompute `k·t`,
  replot `fig:rr-depth`, and update the basin numbers in §6 if they shift (currently min @12, basin
  ≤1.2%, 5→12 +4.7%). `k` can reuse saved EXRs (`results/campaign/rr_seeds/`, gitignored); only the
  **timing** needs fresh renders, so needs the GPU pinned.
- **Note:** depth 14 lands between 12 (min) and 16 on the gentle up-slope — won't change the conclusion,
  but closes the gap and the rerun cleans the timing contamination.

### 3. `fig:scattering-ladder` (Fig 5.3) — confusing; likely cut or rework
- **Issue:** the "flat line + shrinking s.e. band" mean-convergence plot is hard to read (Kacper didn't
  get it after two explanations — that's the figure failing). The whole result it carries is **two
  numbers**: ours-MIS and Mitsuba-analog both converge to mean 0.321 (agree 0.4 %, both unbiased), and
  ours' per-seed scatter is ~68× smaller. Those are already stated in §5 prose.
- **Options:** **(a) cut it**, keep the two numbers in prose (lean — consistent with the other cuts; a
  figure for two numbers is overkill); **(b) rework** into a standard convergence plot — each method's
  *running* mean as seeds accumulate (a wiggly Mitsuba-analog line settling toward the same value ours
  snaps to). Builder: `scripts/plots/scattering_convergence.py` (would need rewrite for (b)). **Decision
  needed.** Recommendation: (a) cut.

### 4. `fig:roofline` (Fig 6.1) — only 2 points (cloud-meadow, bunny-meadow)
- **Issue:** sparse-looking; `roofline.csv` has cloud (AI 6.75, 1012 GFLOP/s) + bunny (24.4, 404).
- **Reality:** the two points are the asset-size **extremes** and both sit far under both roofs, which is
  the whole (non-saturation → latency-bound) argument; FLOPs are **estimates** (OptiX rejects the HW FLOP
  counters), so the figure is explicitly non-saturation-only. More asset points (tornado/explosion)
  would **cluster near cloud** and reinforce, not extend.
- **Options:** keep 2 (lean); or add a **cloud-absorption** point (no phase eval → genuinely different
  arithmetic intensity = a *regime* contrast, more informative than more assets). Needs `ncu`
  (clock-independent, runnable at 150 W). **Decision needed.**

### 5. `fig:scaling` memory panel ("Memory is decoupled from N") — only 2 points
- **Issue:** GAS line has 2 points (cloud, bunny); flat-reservation line has 2 synthetic endpoints
  (N=16, N=8192).
- **Reality:** GAS line is a linear fit (extremes define it; it's negligible vs the 1200 MiB floor) —
  fine as 2. The **flat reservation line** would be visually nailed by adding the **intermediate
  synthetic VRAM points** (N=256/512/1024/2048/4096, all at 1200 MiB) — directly *shows* "flat in N"
  rather than asking the reader to trust the line.
- **Action (cheap, recommended if it looks thin):** VRAM-poll the intermediate synthetic stress grids
  (the `stress_*_gaussians` scenes already exist) and add them to `scaling.csv` peak column, replot via
  `scripts/plots/scaling.py`. **Clock-independent** (VRAM) → runnable at 150 W.

### 6. Recurring theme Kacper flagged: "why only 2 points?"
Several plots use just the cloud+bunny extremes (roofline, scaling). The honest defense is that
non-saturation / decoupling arguments only need to bracket the range — but if they read as thin, the
cheap, *meaningful* additions are: a regime contrast (roofline: cloud-absorption) and filling flat lines
(scaling-memory: intermediate synthetic points). Avoid adding clustered same-regime asset points (no new
information, just ink).

---

## OPEN — floats not yet reviewed (continue the audit)
- **`fig:pipeline` (Fig 4.1)** — the one Ch4 float skipped; needs the worth-it/well-executed pass.
- **Ch5:** `fig:voxel-gt`, `fig:absorption-ladder`, `fig:showcase`.
- **Ch6:** `fig:ris-ksweep`, `fig:icosphere-sliver`; tables `tab:wins`, `tab:complexity`, `tab:icosphere`.
- **Ch7:** `fig:g1-bias`, `fig:denoise`, `fig:generalisation`; `fig:scaling` *time* panel (the square-grid
  fit was partly discussed — rectangular grids now shown as open markers).

---

## DONE this session (do not redo)
- **Algorithm 1 (`alg:argmin`)** — reworked: caption "for one ray (one path bounce)"; DRY `Try(k,t_start)`
  sub-procedure; `inv_cdf_segment`→`inv_cdf_span` (code + thesis); `t_exit(k)` added to Require. The
  code's two loops legitimately differ (kept).
- **`fig:optical-depth` (Fig 4.2)** — redrawn as two-panel real→whitened (shows the anisotropic ellipse +
  the whitening), rotation fixed so the ray cuts across the anisotropy.
- **`fig:argmin`** — **cut** (decorative number-line; carried by Alg 1 + §4.4).
- **`fig:per-ray-state`** — **cut** (unreferenced; "two stacked arrays"; carried by tab:vram + prose).
- **`tab:ref-config` (Table 5.1)** — **cut**; the load-bearing number (path-traced 0.31 vs tomographic
  0.53, ~1.7× from convention) folded into the §5 prose.
- **`tab:overlap`** — row labels unified to "Disney cloud, {primary/coarse/dense/fine} fit" (WDAS = Disney;
  was inconsistent); caption + prose updated.
- **`tab:vram`** — Mitsuba column restored (was briefly removed in error); cloud 838, others dashed
  pending measurement (see Open #1).

(The one remaining `WDAS` in `05-validation.tex` is intentional — it states "Disney/WDAS cloud" once to
define the equivalence.)
