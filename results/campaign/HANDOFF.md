# HANDOFF — thesis state for the next session (2026-06-14)

Read this first. Self-contained pickup point after a context compaction.

## State
- **Branch `main`**, latest commit `cc5abf1` (R3 + abstract + showcase + generalisation, this session). Thesis builds **clean at 69 pp** (`cd thesis/latex && latexmk -pdf thesis.tex`; 0 errors, 0 undefined refs). All Results gaps now closed; remaining items are a user decision + optional polish (see below).
- **GPU clocks ARE NOW LOCKED** (SM pinned 1800, mem 9751, 350 W). Verified: light kernels hold p50/max 1800. `applications.gr` reads N/A — that's normal for `-lgc` (don't worry). Re-lock if needed: `bash scripts/campaign/lock_clocks.sh` (needs the USER to run it via `! ` — sudo).
- Records live in `results/campaign/*.md`. Calibrated binaries in `~/winbins/` (exe_{cloud,tornado,explosion,bunny,stock,analog} + ir_*).
- Standing constraints (still in force): NO AI mentions / NO Co-Authored-By in commits; `git push` / branch deletion / merge-to-main are the USER's actions; do NOT `git gc`/`prune`/`reflog expire` (orphan-recovered deprecated-* branches). latexmk must be clean before any thesis commit.

## DONE since this handoff was written (2026-06-14, post-compaction session)
- **Ch7 R3 — scaling study DONE** (commit `3234391`). New §7.7 `sec:results-scaling` + `fig:scaling`
  (two panels). Synthetic square-grid sweep → t∝N^0.40; 4 real assets at operating point → t∝N^0.71
  (both strongly sub-linear); memory decoupled from N (flat 1200 MiB reservation; GAS ~0.16 KB/prim,
  2–3 orders below). Driver `scripts/campaign/run_g3_scaling.sh`, plot `scripts/plots/scaling.py`,
  data `scaling.{csv,md}`. Ran at locked 1800/9751.
- **Abstract DONE** (commit `75253cf`). Dropped the "fivefold deficit" framing; now leads with
  correctness + ~59× equal-quality headline + memory win (578<838) + voxel-GT cross-check; RIS 1.4×
  kept as second win.
- **fig:showcase MADE REAL** (commit `a93d28d`). The climactic §5.5/§5.7 combined-showcase placeholder
  is now a real equal-time money shot (ours-MIS clean vs Mitsuba-analog fireflies, seed 02, means
  0.321 vs 0.321). `scripts/plots/showcase.py`.
- **fig:generalisation ADDED** (commit `cc5abf1`, bonus). The previously figure-less §7.6 now has the
  tornado+explosion parity triptych (ours | Mitsuba-analog | |diff|×10) from the G10 renders.

## What's MISSING in the thesis (priority order)
1. **Appendix A placement decision (USER)** — `chapters/appendix-a-a1.tex` (per-step
   Rao–Blackwellisation, full investigation) is still in. User wanted to decide keep / shorten / cut.
   The §6.9 autopsy bullet + §8.27 reference it.
2. **Ch7 R4 — 4-asset env-lit beauty montage** (OPTIONAL polish): the cloud showcase is now real and
   tornado/explosion have the parity figure, so a 4-asset env-lit gallery would be additional/somewhat
   redundant + carries untuned-framing risk. Offer to user; don't do blind.
3. **G1 flat rung** (optional, supporting): ours vs Mitsuba-analog on a FLAT env. Not essential.
4. **(low) Confirm 4-view archive** — §5.5 claims the cloud "matches across four well-separated views".
   The machinery is real (cloud scene = 24 ortho views, `run_multiview`; campaign used `SG_CAM=0`).
   Claim is plausibly backed by pre-campaign validation; just confirm those renders are archived if a
   committee asks. NOT an overclaim — do not soften without checking with Kacper.

## What's DONE (don't redo)
- **Ch1–6, 8 written + TWICE review-passed** (Kacper's two review rounds folded in: icosphere reframe, tab overflows fixed via resizebox, reference-config methodology, Epanechnikov limitation, dropped "initial gap"/"DSYG programme" framing, etc.).
- **Ch7 written EXCEPT R3/R4**: speed headline R1 (production ~59×), fireflies/NEE-bias R5 (fig:g1-bias), denoiser R4-fig, memory R6/R7 (tab:vram, leads with 578<838 vs Mitsuba), startup G7, generalisation R2.
- **G1 done**: B discovery (Mitsuba-NEE +156% biased / ours correct); production ~59× equal-quality (clipped-variance ratio, clock-independent → robust). bunny 50.4 s ours-internal.
- **G1 comparison A done + SETTLED**: pure-analog fix committed (`raygen.cuh`, NEE-off is now true analog — furnace PASS, mean 0.327). Result: analog-vs-analog is **firefly-metric-UNSTABLE** (ours-analog k_raw 15075 / k_clip 0.2 vs Mitsuba 3899 / 110.6 — ours' noise is a few extreme sparse fireflies; max pixel 2179). NO stable core-sampler number exists; R1 says so honestly and makes the production 59× the headline. (`g1_analog_final.md`, `analog_compare.png`.) **Do NOT re-chase a single analog speedup number — it's genuinely metric-dependent.**
- **G2**: RR-depth (min d12), fast-erf (free), denoiser (~28× confirmed = dev ~30×). **Merge-ladder: INCONCLUSIVE** (`g2_ladder.md`) — old commits build but early ones render the cloud trivially + measurable ones diluted at pre-calibration caps; dev §-numbers in tab:wins STAND. Don't re-run it.
- **G3 RIS, G4 ncu, G5/G5b memory, G6 confirms (adaptive net-loss; wavefront cited), G7 JIT, G8 icosphere, G10 parity** — all complete.
- **Voxel-GT** (bonus, branch merged): absorption cross-check VALID (bulk-exact, edge-resolution-limited, in Ch5 sec:voxel-gt); scattering mean-only (firefly-limited). The independent grid integrator (Mitsuba heterogeneous/gridvolume) reproduces our cloud. Scripts: `tools/refs/voxel_*.py`.
- **Branch cleanup**: deleted 4 MERGED feature branches (cap-calibration, icosphere-gas, volumetric-ris, voxel-gt). KEPT all `deprecated-*` (autopsy evidence for the thesis) + unmerged feature archives (cap-free-streaming, wavefront-phase1, env-is-alias-table, path-guiding).

## USER actions still pending
- `git branch -D thesis` — the stale `thesis` branch is fully in main (verified) but force-delete was permission-blocked here; the user runs it.
- The Appendix-A keep/cut decision (#3 above).
- Whether to do R3 / R4 / flat-rung.

## Ideas raised (not yet acted)
- **Cheap path-guiding kill-test** (the "oracle-ceiling" test: bake in-scattered radiance to a coarse grid, use as a continuation guide, measure gain; upper-bounds any real guide → valid kill if low). A good 1-session / parallel-session candidate. Written into §8.3-ish? No — only discussed. `deprecated-path-guiding` / `feature/path-guiding` branches exist.
- **Chunked re-traversal** for recompile-free caps (fixed small buffer; on fill, re-issue trace from last t) — written into Ch8 future work; untried; promising.
- Representation-fidelity voxel GT vs the ORIGINAL Disney VDB (`wdas_cloud_eighth.npy`) — would need that source from Jorge (not on the machine).
