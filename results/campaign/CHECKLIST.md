# Section-6 Experiment Campaign — Running Checklist

**Updated:** 2026-06-13 15:40 CEST · **Legend:** `[x]` done · `[ ]` pending (tag shows *when*: ← post-window 150W / ← next window)

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

### G1 — headline equal-quality vs Mitsuba  ← next window (3–5 h, cleanest GPU)
- [ ] Mitsuba pre-flight (verify `cloud_meadow_seeds.sh`; match depth/res/cam/env; `SG_SHAPE=ellipsoids` re-gate)  ← post-window
- [ ] flat rung (bare → final-validation → Mitsuba-analog)
- [ ] meadow showcase rung (+ Mitsuba-MIS for time/firefly/bias only)
- [ ] bunny rung (gated on G10 parity)
- [ ] money shots (final-showcase)

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
- [x] done (`gas_memory.csv`; cap-independent)

### G6 — negative-result confirms
- [ ] wavefront ON/OFF (one point)  ← post-window 150W
- [ ] adaptive effective  ← post-window 150W

### G7 — Mitsuba JIT/startup (#96)
- [x] measured (`jit_overhead.md`: ours 0.39 s vs Mitsuba 0.78 s warm / 2.20 s cold)
- [ ] wire numbers into Ch 3 + results/limitations (startup/iteration-latency advantage)  ← thesis edit, pending

### G8 — analytic vs icosphere
- [x] accuracy (3 assets, RMSE)
- [x] perf (cloud, 128/128 — ratio cap-robust)
- [x] perf re-anchor at 64/96 — ratios confirmed (analytic pays 1.2–1.6×; N=2 → 1.23×); N=3 kept at its valid 128/128 figure (not re-anchored) — `icosphere_port.md`

### G10 — Mitsuba parity gates (cross-renderer eligibility)
- [ ] tornado parity (energy-ratio, absorption)  ← post-window 150W (power-immune)
- [ ] explosion parity  ← post-window 150W

## D. Thesis wiring (figures / tables / prose)
- [x] `fig:ris-ksweep` (3 envs, regenerated)
- [x] `rr_depth.csv` absolutes + `fig:rr-depth` (re-anchored, caption updated)
- [x] `sec:ris` prose + caption → flat loses / studio 1.45× / meadow 1.48× / saturation / peak K=6
- [x] `tab:wins` RR (+4.7%, min d12) + fast-erf (numerically free, ~1%)
- [x] Ch 3 JIT sentence → measured magnitude (0.4 vs 0.8/2.2 s) — **G7 documented**
- [x] `tab:icosphere` — decided: unchanged (valid 128/128; re-anchor confirmed ratios, N=3 not re-anchored)
- [ ] `tab:wins` ladder rows (shadow/skip-scan/dedup/fusion) — still dev §-numbers; re-measure = G2 ladder
- [x] `roofline.pdf` regenerated (cloud + bunny points)
- [ ] `sec:bottleneck` prose → bunny vs cloud contrast (Kacper's WIP section; numbers ready in `ncu_summary.md`)
- thesis builds clean: 59 pp, 0 errors/undefined (commit `d48564b`)

## E. Deprecated-branch reruns
- [x] cap-staleness audit done — **no extra reruns**: cap-sensitive numbers are already covered by G2/G3/G6; the rest are cap-robust (ratios/counts/RMSE/huge-margin). **Watch:** `incremental-active-prims` (~16%, §8.23) in the G2 ladder may shrink at the smaller active cap.

## Standing protocol
- [x] every render checked for `Cap overflow` (driver-enforced); act on any hit (Kacper's directive). [G8 N=3 didn't fit the analytic's caps → N=3 left at its valid 128/128 figure; not a reported result.]
- **thesis prose wiring DONE** (G3/RR/fast-erf/G7 → committed `d48564b`, 59 pp clean).
- **next action:** G6 wavefront/adaptive confirms + G10 parity gates (both cap-immune) + prep builds (ladder/BARE/wavefront/adaptive). G1 + G2 ladder A/B await a new max-perf window. (G4 bunny ncu ✅ done.)
