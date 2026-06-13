# Section-6 Experiment Campaign — Running Checklist

**Updated:** 2026-06-13 16:35 CEST · **Legend:** `[x]` done · `[ ]` pending (tag shows *when*: ← post-window 150W / ← next window)

Source of truth for progress. Full procedure: `docs/superpowers/plans/2026-06-12-section6-window-campaign.md`. Records: `results/campaign/*.md`.

> **Window status:** Piotr extended the max-perf window. Done inside it (0 overflows): fast-timing bucket (G3 flat/studio + RR/meadow re-anchors) ✅ and **fast-erf A/B** ✅. Remaining timed work (G1, G2 ladder, optional G8 perf re-anchor) needs builds/pre-flight first. Cap-immune work (G4/G6/G10 + builds) runs anytime.

---

## A. Setup & prep
- [x] `extract_k.py` (regression-validated: reproduces banked d12 k=1.98417)
- [x] `env_peakiness.py` + `env_peakiness.csv` (white 1×/0.6% · studio 538.8×/39.7% · meadow 1.53e5×/78.1%)
- [x] studio env wiring (`SG_ENV=studio` → ferndale_studio_01_4k)
- [x] calibrated builds stashed in `~/winbins/`, all furnace-PASS: **cloud 64/96 · tornado 112/384 · explosion 32/160 · bunny 80/528**
- [x] fast-erf build (`build-ferf/`, caps 64/96, `THESIS_ENABLE_FAST_ERF=ON`)
- [x] `main` fast-forwarded to campaign HEAD; plan updated (exec split + deprecated audit)

## B. GPU / bench settings
- [x] 350 W · SM 1800 · mem 9751 · persistence (verified live; window now closed)
- [ ] CPU governor=performance  ← before G1 (Mitsuba/absolute numbers)

## C. Experiments

### G1 — headline equal-quality vs Mitsuba  ← **~30–50 min** (measured probe), GPU window
- [ ] Mitsuba pre-flight (verify `cloud_meadow_seeds.sh`; match depth/res/cam/env; `SG_SHAPE=ellipsoids` re-gate)
- [ ] flat rung — **ours-final vs Mitsuba-analog** (ours-final = banked RIS-MIS arm; **no BARE, no deficit framing** — clean final-vs-Mitsuba)
- [ ] meadow showcase rung (+ Mitsuba-MIS for firefly/bias only)
- [x] bunny rung (ours-internal) — t_median **50.4 s** @64spp/512²/meadow, k=0.647; **0 overflows** (80/528 cap validated)
- [ ] money shots (final-showcase)
- **config:** analytic baseline (NOT icosphere — that's G8-only); MIS; calibrated caps. Probe: Mitsuba-meadow-cloud **4 s/render**, ours-bunny **52 s**.

### G2 — optimization ablations → `tab:wins`, `fig:rr-depth`
- [x] RR-depth timing re-anchor — **min at depth 12**, +4.7 % vs d5; `rr_depth.csv`/`fig:rr-depth` updated; provisional flag cleared
- [ ] merge-ladder: shadow-transmittance · skip-scan · dedup-bounce0 · any-hit fusion  ← needs worktree builds (post-window) then next window
- [x] fast-erf A/B — numerically free (max\|Δ\| 1.2e-6); **+0.9 %** time (within noise; dev ~1.5%) — `fast_erf.md`
- [ ] denoiser effective-RMSE vs 2048-spp GT

### G3 — RIS scene-dependence → `fig:ris-ksweep`  ✅ COMPLETE
- [x] meadow rung (2026-06-12; 1.48× at K=4–6, CI'd; k banked)
- [x] meadow timing re-anchor — re-confirmed **1.49× at K=4–6**, clean clocks
- [x] flat rung — **RIS loses** (best 0.55× at K=12; ~12× worse at K=1)
- [x] studio rung — **RIS wins 1.45×** (peak K=6)
- [x] assemble `fig:ris-ksweep` (3 envs; peakiness-monotonic, saturates ~1.45–1.49×, peak K=6)

### G4 — bottleneck profile (ncu)
- [x] cloud profile (`ncu_summary.md`, `roofline.csv`)
- [x] bunny profile — **more latency-bound than cloud**: occ 20.9%, sched-idle 70%, 5.42 active lanes, DRAM 1.8%; roofline AI 24.4 / 404 GFLOP/s — `ncu_summary.md`

### G5 — GAS / VRAM memory
- [x] GAS sizes (`gas_memory.csv`; cap-independent)
- [x] **G5b — peak VRAM (GAP flagged by Kacper 2026-06-13):** DONE (`vram.md`/`vram.csv`). Calibrated vs SAFE-512: cloud 578/1200 (−622), tornado 818/1200 (−382), explosion 600/1200 (−600), bunny 900/1200 (−300) MiB → cap calibration saves **0.30–0.62 GiB (25–52%)**. SAFE-512 is a flat 1200 MiB (reservation dominates, asset-independent). ours-vs-Mitsuba cloud: ours-calibrated **578** vs Mitsuba **838** (31% less); ours-512 1200 (43% more) → calibration is what beats the reference on memory.

### G6 — negative-result confirms  ✅ (`g6.md`)
- [~] wavefront ON/OFF — re-run BLOCKED: `deprecated-wavefront-phase1` binaries fault (illegal mem access, even single_gaussian/white_constant) on the current toolchain (branch predates asset/env reorg + OptiX). Cite dev §8.34 (100–1400× slower); megakernel is production. Branch preserved.
- [x] adaptive effective — CONFIRMED net loss: cloud-meadow 64spp, adaptive vs uniform **identical RMSE 0.1706**, adaptive 0.9% slower + extra W·H·16B buffer (at 1% threshold nothing early-stops on high-variance cloud → degenerates to uniform). Confirms §8.30.

### G7 — Mitsuba JIT/startup (#96)
- [x] measured (`jit_overhead.md`: ours 0.39 s vs Mitsuba 0.78 s warm / 2.20 s cold)
- [ ] wire numbers into Ch 3 + results/limitations (startup/iteration-latency advantage)  ← thesis edit, pending

### G8 — analytic vs icosphere
- [x] accuracy (3 assets, RMSE)
- [x] perf (cloud, 128/128 — ratio cap-robust)
- [x] perf re-anchor at 64/96 — ratios confirmed (analytic pays 1.2–1.6×; N=2 → 1.23×); N=3 kept at its valid 128/128 figure (not re-anchored) — `icosphere_port.md`

### G10 — Mitsuba parity gates (cross-renderer eligibility) — ✅ PASS
- [x] tornado parity — ratio **0.99911** (ours 0.9661 / Mitsuba-analog 0.9670) — PASS
- [x] explosion parity — ratio **1.00006** — PASS
- unblock (Kacper was right): use Jorge's **native PLY** in `unpacked/<a>/optimized_asset_pyr0/data/` + value-preserving `opacities_0→sigma_t_0` rename (gate confirms it's correct). **tornado + explosion now eligible for G1 cross-renderer claims.** `asset_parity.md`

## D. Thesis wiring (figures / tables / prose)
- [x] `fig:ris-ksweep` (3 envs, regenerated)
- [x] `rr_depth.csv` absolutes + `fig:rr-depth` (re-anchored, caption updated)
- [x] `sec:ris` prose + caption → flat loses / studio 1.45× / meadow 1.48× / saturation / peak K=6
- [x] `tab:wins` RR (+4.7%, min d12) + fast-erf (numerically free, ~1%)
- [x] Ch 3 JIT sentence → measured magnitude (0.4 vs 0.8/2.2 s) — **G7 documented**
- [x] `tab:icosphere` — decided: unchanged (valid 128/128; re-anchor confirmed ratios, N=3 not re-anchored)
- [ ] `tab:wins` ladder rows (shadow/skip-scan/dedup/fusion) — still dev §-numbers; re-measure = G2 ladder
- [x] `roofline.pdf` regenerated (cloud + bunny points)
- [x] `sec:bottleneck` — measured cloud+bunny profile + `fig:roofline` wired in (commit `1f11a1d`)
- thesis builds clean: 59 pp, 0 errors/undefined (commit `d48564b`)

## F. Render figures (graphics-thesis visuals — Kacper, 2026-06-13)
- [~] **Mitsuba comparison triptychs** (ours | Mitsuba | diff): **G10 tornado/explosion ✅ done** (`g10_triptych.png`; diff is 256-spp noise — thesis-final wants converged renders for a clean diff). Remaining: Ch5 cloud absorption/scattering (`fig:absorption-ladder`/`fig:scattering-ladder`), G1 cloud headline + firefly-crop vs Mitsuba-MIS
- [x] **G1 bias triptych** — ours-MIS (+0.4%) | Mitsuba-NEE (+156% over-bright) | Mitsuba-analog GT → `figures/g1_bias.pdf` (`g1_bias_triptych.py`, 16-seed means)
- [ ] **showcase beauty montage** — 4 assets env-lit → would be a NEW figure (`fig:showcase` is already the Ch5 cloud showcase); needs a thesis home + Kacper's nod before adding
- [x] **RIS equal-quality noise + firefly crop** (meadow) → `figures/ris_noise.pdf` (`ris_noise.py`; MIS|RIS K=6|ref, RMSE 0.173 vs 0.160 → variance²×time reconciles to 1.49×)
- [x] **icosphere N=3 sliver-artifact crop** vs smooth analytic → `figures/icosphere_sliver.pdf` (`icosphere_sliver.py`; absorption, pure-geometry diff, 3149 px |Δ|>0.05)
- [x] **denoiser** before/after/GT triptych → `figures/denoise.pdf` (`denoise_triptych.py`; RMSE 0.353→0.049, 7.2× lower)
- skip (not visual): RR-depth, fast-erf (numerically identical), ncu/roofline (already a plot)
- **WIRING STATUS:** RIS noise → `sec:ris` (06) ✅; icosphere sliver → `sec:icosphere` (06) ✅; G5b VRAM → `sec:gpu-impl` caps prose (04) ✅ — all built clean (59 pp, 0 undefined). **G1 bias triptych + denoiser triptych await Ch7** (`07-results.tex` is still a SCAFFOLD; they're the planned R5 firefly-3-way + R4 showcase — authoring Ch7 is the next thesis task, Kacper-reviewed).

## E. Deprecated-branch reruns
- [x] cap-staleness audit done — **no extra reruns**: cap-sensitive numbers are already covered by G2/G3/G6; the rest are cap-robust (ratios/counts/RMSE/huge-margin). **Watch:** `incremental-active-prims` (~16%, §8.23) in the G2 ladder may shrink at the smaller active cap.

## Standing protocol
- [x] every render checked for `Cap overflow` (driver-enforced); act on any hit (Kacper's directive). [G8 N=3 didn't fit the analytic's caps → N=3 left at its valid 128/128 figure; not a reported result.]
- **thesis prose wiring DONE** (G3/RR/fast-erf/G7 → committed `d48564b`, 59 pp clean).
- **HANDOFF → `results/campaign/g1_headline.md`** (full G1 + proceed-map + code fix). In short: **G1 done** — B: *Mitsuba-NEE +156% biased / ours correct* (clean discovery); speed **~59× clipped** (B-justified, no deficit framing); bunny ours-internal 50.4 s; **A (core sampler) blocked** — renderer has no true analog mode (`raygen.cuh:270–276` is unoccluded-single-scatter → 17× on dense cloud), small **code fix documented** to enable it. **Discovery wrecks nothing** — all reported runs used default NEE-on MIS (validated).
- **next:** [350 W] G2 ladder A/B (build 8 worktrees first) + denoiser (+ optional comparison-A after the pure-analog fix); [150 W] G5b VRAM, render figures (G1 bias triptych ready), G6, ladder/wavefront builds; thesis-wire G1+ladder+figures. **Pending Kacper:** G1 framing + whether to make the pure-analog code fix.
