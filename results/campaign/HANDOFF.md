# HANDOFF — thesis state for the next session (2026-06-14)

Read this first. Self-contained pickup point after a context compaction.

## State
- **Branch `main`**, latest commit `e59456e`. Thesis builds **clean at 69 pp** (`cd thesis/latex && latexmk -pdf thesis.tex`; 0 errors, 0 undefined refs).
- **GPU clocks ARE NOW LOCKED** (SM pinned 1800, mem 9751, 350 W). Verified: light kernels hold p50/max 1800. `applications.gr` reads N/A — that's normal for `-lgc` (don't worry). Re-lock if needed: `bash scripts/campaign/lock_clocks.sh` (needs the USER to run it via `! ` — sudo).
- Records live in `results/campaign/*.md`. Calibrated binaries in `~/winbins/` (exe_{cloud,tornado,explosion,bunny,stock,analog} + ir_*).
- Standing constraints (still in force): NO AI mentions / NO Co-Authored-By in commits; `git push` / branch deletion / merge-to-main are the USER's actions; do NOT `git gc`/`prune`/`reflog expire` (orphan-recovered deprecated-* branches). latexmk must be clean before any thesis commit.

## What's MISSING in the thesis (priority order)
1. **Ch7 R3 — scaling study** (the real remaining results gap). Time AND memory vs primitive count: the 4 assets as points (cloud 652, tornado, explosion, bunny 25600) + optionally a stress_N sweep. Placeholder at `chapters/07-results.tex` line ~176. Needs a handful of timed renders — clocks are locked, so straightforward. Per-asset memory already measured (`vram.md`); just needs the time-vs-N points + a short section + figure.
2. **Abstract check** — `thesis/latex/abstract.tex` exists + is included but NOT re-verified against the final story. Make sure it states: the march/sort/root-find-free architecture; correctness via differential + voxel-GT; the ~59× equal-quality headline; the memory win (578<838 MiB); the negative-results map. Read + update.
3. **Appendix A placement decision (USER)** — `chapters/appendix-a-a1.tex` (per-step Rao–Blackwellisation, full investigation) is still in. User wanted to decide keep / shorten / cut. The §6.9 autopsy bullet + §8.27 reference it.
4. **Ch7 R4 — beauty/showcase montage** (polish): 4 assets env-lit, "money shots". Needs renders, no analysis. Low-priority visual.
5. **G1 flat rung** (optional, supporting): ours vs Mitsuba-analog on a FLAT env — "speedup without the env-lighting advantage". Not essential.

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
