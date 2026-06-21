# Examiner review — Pierre Talbot (GPU performance-engineering)

*Persona P3 of the 2026-06-20 board. Reviewed as a parallel-computing / GPU-architecture examiner:
benchmarking methodology, statistical treatment, roofline/occupancy/divergence analysis, complexity,
the SER/megakernel/wavefront reasoning, and GPU terminology. Unmoved by the rendering aesthetics. Every
clock-independent load-bearing number was re-derived from `results/campaign/*` and the algorithm/SER/
complexity claims cross-checked against `device/`, `include/`, `cmake/`. Local frame times were neither
trusted nor re-run; the GPU is power-capped at 150 W and the 4090 figures are archived.*

---

## Bottom line

From a systems-rigor standpoint this is a **strong** thesis that I would pass with minor revisions. The
benchmarking hygiene is well above the typical MSc bar — within-seed-block interleaving, an explicit
clock-independent metric, base-clocked `ncu` profiling that is immune to the 150 W cap, a genuine
on/off-ablation discipline, and a negative-results ledger that is itself a contribution. **Every
clock-independent load-bearing number I checked reproduces**: the roofline points for all four assets, the
59× clipped-variance ratio, the four-row 4090 ladder (77.5/110.3/151.3/178.7×), the SER A/B table, the
scaling exponent (b = 0.397, R² = 0.956), the VRAM table, and the per-asset overlap caps. The
algorithm in the source matches the prose — `min`-of-independent-free-flights with a span-restricted
`erfinvf`, no marching, no root-finding — and the latency-bound / divergence-dominated diagnosis is
correctly measured **and** correctly interpreted.

**On the metric itself:** k·t is the *correct* equal-quality figure of merit, not merely a convenient one.
For two unbiased estimators with Monte-Carlo variance ∝ 1/N, time-to-a-target-variance is proportional to
(per-sample variance constant) × (time per sample) = k·t, so the ratio (k·t)_ref/(k·t)_ours is exactly the
equal-quality speedup. The thesis derives this correctly (`05-validation.tex:126-133`), requires both
estimators unbiased before equating RMSE²·N with inter-seed-variance × spp, and confirms unbiasedness in
Ch5. The one fragility is intrinsic to the *workload*, not the metric: the analog reference's firefly
heavy tail strains the finite-variance assumption underlying 1/N, which is exactly why clipping is needed —
and exactly why the clip convention (S3) and the missing CI (S1) matter. The metric is sound; its
*quantification* is where I push.

**There are no Blocking defects in my domain.** Where I push hard is on three things a GPU examiner will
not let pass quietly: (1) the **headline 59× carries no confidence interval** while the 100×-smaller RIS
win does, and it is a variance ratio of a heavy-tailed estimator whose value swings 34× with the clip
convention; (2) the **3090 reference timing is a reused wall-clock approximation**, so the time half of
the headline is the softest input in the thesis (the *4090* probe, ironically, has cleaner timing than the
pinned headline); and (3) a handful of **GPU-mechanism terminology slips** ("8-cell" key, "shared memory",
"coalesced" local buffer) that read as imprecision about the hardware. All three are cheap to fix and none
overturns a conclusion.

---

## 1. Grades

| # | Dimension | Grade | Justification |
|---|-----------|:---:|---|
| **2** | **Experimental rigor & methodology** *(owned, in depth)* | **4** | Interleaved A/B within each seed block, clock-locking, an explicit clock-independent ratio metric, `ddof=1` sample variance, 16-seed flagship sweeps, base-clocked `ncu`, bit-identity gates for SER, and a rigorously-killed cap-free autopsy. Held back from 5 by: **no CI on the headline 59× or the 4090 ladder** (while RIS has one); the **Mitsuba 3090 render time is a reused wall-clock estimate** (~9 s / ~8.5 s, never cleanly pinned); the RR-sweep *timing* half is single-seed median-of-5 yet captioned "16 seeds"; the RIS bootstrap is 50 resamples and the method is unstated in the thesis. |
| **1** | **Technical correctness & soundness** *(owned as systems-correctness, in depth)* | **4** | The argmin sampler, O(1) active-set inheritance, bit-vector/compact-set 256 threshold, 6-byte SoA hit record, any-hit-only megakernel, analytic exits, ~352 B per-ray state, and the roofline/occupancy/divergence numbers are all faithfully implemented and correctly reasoned (verified in source). Docked from 5 only by GPU-mechanism wording that is imprecise about the hardware ("8-cell", "shared memory", "coalesced"; see Findings) and by the reference's O(H²) complexity being *asserted*, not measured. |
| **9** | **Cross-thesis consistency** *(owned, in depth)* | **4** | Headline k (1.99 / 1.887) and the 59× are identical across Ch5/6/7; the 4090 ladder is consistently scoped as an Ada probe; VRAM and overlap tables agree across Ch4/6/7. Two real blemishes: **tab:wins says RR "5→12 +4.7%" but fig:rr-depth and prose say +5.0%** (a stale contaminated-run number), and the flat rung is quoted as **2.90 s** in the thesis vs **2.85 s** in the banked data. |
| 3 | Honesty & claim calibration | 5 | Model calibration: the bare sampler is admitted as a net **0.6× equal-quality loss**; raw (2000×) vs clipped (59×) disclosed and the conservative figure chosen; the 4090 never folded into the headline; the icosphere is faster yet rejected for correctness; memory is stated as "mixed" (ours *above* Mitsuba on two assets). |
| 4 | Relevance & scope discipline | 4 | Cleanly scoped as a single-GPU performance study with an explicit Ada cross-arch probe; no scope creep. |
| 5 | Argumentation & significance | 4 | The perf narrative (bottleneck → taxonomy → wins → negatives → cross-arch) is coherent and the negative ledger is sold as a real result. (Didyk's call; brief.) |
| 6 | Related work & positioning | 4 | Wavefront (Laine 2013), decomposition tracking (Kutz 2017), and the megakernel framing are the right systems anchors. (Brief.) |
| 7 | Writing & academic style | 4 | Clear and formal; the only writing defects in my remit are the GPU-terminology slips. (P4's call; brief.) |
| 8 | Professionalism & presentation | 4 | Tables/figures legible and honestly captioned; the roofline caption correctly frames itself as a non-saturation argument. (Brief.) |
| 10 | Defense-readiness | 4 | Systems story is defense-ready *except* it walks in without a CI on its headline number and with two or three terminology phrasings I will ask about. (Didyk's call; brief.) |

---

## 2. Findings

### Blocking
**None in the GPU performance-engineering domain.** Every load-bearing clock-independent number reproduces
from `results/campaign/*`, the algorithm matches the source, and the architectural reasoning is sound.
This is a genuine finding, not a courtesy: I tried to break the headline arithmetic and the roofline
diagnosis and could not.

### Should-fix

**S1 — The headline 59× has no confidence interval, yet the 100×-smaller RIS win does.**
`07-results.tex:25-31` reports "roughly **59×** faster at equal render time" with only a "~". The 59× is a
ratio of two inter-seed *variance* estimates (k_ours = 1.887, k_analog = 110.6) from 16 seeds each
(`g1_headline.md:11-12`). A variance ratio from 16 samples of a **heavy-tailed** estimator (the analog
reference is firefly-dominated, k_raw = 3899) has a wide F-type sampling distribution; a ±30–40 %
interval would not surprise me. Meanwhile the *secondary* RIS result earns a bootstrap CI
(`06-optimization.tex:143`, "[1.47, 1.49]"). The asymmetry is indefensible for a performance thesis: the
number the whole thesis is built on is the *only* one without an uncertainty estimate.
*Fix:* the 16 seeds are banked (`g1_seeds/`); bootstrap a 95 % CI on (k·t)_ref/(k·t)_ours exactly as the
RIS sweep already does, and report it next to the 59×. State the resample count.

**S2 — The Mitsuba 3090 render time is a reused wall-clock approximation; the time half of the headline is unpinned.**
`g1_headline.md:12,26` records the analog reference at "~9 s (wall 13.5 incl startup)" and explicitly notes
"a clean steady-state Mitsuba render time would tighten the number". The flat rung reuses the *same*
~8.5 s for a *different* environment (`07-results.tex:41-45` footnote; `g1_flat.md:36`). So both 3090
time-ratios rest on a reference time that was never cleanly measured. The conclusion survives because k
dominates (k-ratio 58.6× × t-ratio ≈ 1.0), and the **4090 ladder fixes this properly** — a warmup render
excludes JIT and gives a clean steady-state t_mits = 4.347 s (`g1_4090.md:10,24`). The irony is that the
*scoped probe* has better timing methodology than the *pinned headline*.
*Fix:* either pin a clean steady-state Mitsuba 3090 render (warmup + median, as on the 4090), or state
explicitly that the 3090 headline is a **variance-dominated** figure with t-ratio ≈ 1 and point to the
4090 row as the timing-rigor anchor. One sentence closes it.

**S3 — The 59× depends on the clip convention, which spans 34×, and the convention is chosen post hoc.**
`07-results.tex:30-31` discloses raw ≈ 2000× vs clipped 59×, but the independent recompute
(`g1_headline.md:87-90`) shows a *third* convention — clipping the per-pixel k-array rather than the
radiance — gives **~1520×**, not 59×. So the same "99.9th percentile" phrase yields 59×, 1520×, or 2000×
depending on *what* is clipped. The headline uses the most conservative (radiance-clipped, then variance:
`run_g1_flat.sh:49`), which is the honest choice — but a reader cannot tell that three conventions exist
or why this one. *Fix:* in the footnote, name the operation precisely ("the per-pixel radiance is clipped
at a single global 99.9th-percentile threshold *before* the inter-seed variance is taken") and note in one
clause that it is the most conservative of the firefly-discounting conventions considered.

**S4 — "registers and shared/local memory" — there is no shared memory in the renderer.**
`04-architecture.tex` (megakernel rationale, ~line 410): "the megakernel keeps it in registers and
shared/local memory, paying zero round-trips." A `grep` for `__shared__` over `device/` and `include/`
returns **zero hits**. The persistent per-ray state (264 B `CompactSet` active set + hit buffer) is far
too large to be register-resident and lives in **local memory** (per-thread, L1/L2-cached, DRAM-backed) —
which the author's own profiling acknowledges (latency-bound on local/global loads). The *argument* (no
global round-trips per bounce) is correct; invoking "shared memory" and "registers" for a 264 B array is
not. A parallel-computing examiner reads this as not knowing where your own data lives.
*Fix:* "keeps it in registers and per-thread local memory (L1/L2-resident), paying no global round-trips."

**S5 — "8-cell spatial key" misdescribes the SER coherence key.**
`06-optimization.tex:439` (tab:ser caption): "the **8-cell** spatial key gives 1.42×". The code
(`raygen.cuh:141-150`) quantises the scatter position to a **1/8-unit grid** (`SER_CELL_INV 8.0f`), spatial-
hashes it, and folds to `& 0xFFu` — an **8-bit, 256-bucket** coherence key — then calls
`optixReorder(ser_hint, 8)` (8 = number of hint *bits*). "8-cell" conflates the grid resolution
(SER_CELL_INV = 8, which is what the 4-/8-/16-"cell" ablation sweeps) with the key itself, and reads as
"8 buckets," which would be a near-useless reorder. The ablation and the numbers are sound; only the name
is wrong. *Fix:* "a 256-bucket (8-bit) coherence key hashed from the scatter position on a ⅛-unit spatial
grid"; relabel the ablation rows as grid resolutions (⅛, ¼, 1/16 unit).

**S6 — tab:wins RR penalty (+4.7 %) contradicts fig:rr-depth and the prose (+5.0 %).**
`06-optimization.tex:107` (tab:wins) and :93 give the depth-5→12 penalty as **+4.7 %**, but :91 and
fig:rr-depth give **+5.0 %**, and the re-anchored CSV (`rr_depth.csv`) yields +4.96 % → +5.0 %. The +4.7 %
is the *superseded contaminated-run* value (`rr_depth.md:27`) carried forward by mistake; it undersells
the measured penalty. *Fix:* set tab:wins and :93 to +5.0 % to match the figure and the re-anchored data.

**S7 — The wavefront "100–1400×" is a one-shot, unreproducible, 14×-spread number.**
`06-optimization.tex:330-335`: "100–1400× slower (a wide range across configurations; the branch no longer
builds … so this stands as a one-time figure)." A 14× spread on a non-reproducible single measurement is
the weakest-evidenced number in the thesis. It is honestly flagged, and the *conclusion* (externalising
per-ray state is fatal) is independently corroborated by the rigorously-killed cap-free streaming autopsy
(bit-exact, interleaved, +12–22 %). *Fix:* lead the bullet with the cap-free result as the load-bearing
evidence and demote "100–1400×" to an illustrative, explicitly-superseded anecdote — don't let a 14×-spread
one-shot carry the headline of "the big one".

**S8 — The RR-sweep advertises "16 seeds" but its time axis is single-seed.**
`06-optimization.tex` fig:rr-depth caption: "16 seeds per depth". True for k (the variance term), but the
`eff = k·t` basin multiplies that by a *time* term that is single-seed, median-of-5 (the 16-seed timing was
discarded for contention contamination; `rr_depth.md:59-69`). Defensible — interleaved time is low-variance
and k needs the seeds — but the caption overstates the timing rigor. *Fix:* "k from 16 seeds; relative
frame time the median of 5 interleaved rounds."

**S9 — VRAM head-to-head mixes a per-process query with a GPU-wide poll.**
`07-results.tex:171-173` (tab:vram footnote) discloses that "ours" uses the per-process
`nvidia-smi --query-compute-apps` while Mitsuba was "polled GPU-wide on an otherwise-idle GPU". These are
not the same measurement, and Mitsuba's 806 MiB is numerically identical across tornado/explosion/bunny
(`vram_mits_suite.csv`, `ser_ab.md:40`) because it is framebuffer-dominated — so the bunny "900 > 806"
comparison is two methods, not one. The direction of bias is caveated (conservative for the win), which is
what rigor requires, but the asymmetry should be named as a threat to validity, not just a footnote.
*Fix:* add half a sentence: "the two are different queries, so the margins are indicative, not exact."

### Polish

**P1 — "power law" over-reads four points.** `07-results.tex:273-275`: t∝N^0.40 is fitted on **n = 4**
square grids (R² = 0.96). The arithmetic is exact (I get b = 0.397, R² = 0.956) and the rectangle exclusion
is principled (the full 7-point series is non-monotonic; including the rectangles drops R² to 0.83). But
four points cannot distinguish a power law from any smooth sub-linear curve. The fully-defensible claim —
"geometry cost is sub-linear in N" — is already present (:283-284). *Fix:* "sub-linear (power-law fit,
n = 4, R² = 0.96)".

**P2 — "coalesced" for a per-thread local buffer.** `04-architecture.tex:93`: the SoA hit buffer is laid
out "so that the sampler's sequential scan over hits is coalesced." Coalescing is a *cross-thread* (warp)
concept; `HitBufferSoA` is per-thread local memory, where the hardware already interleaves a thread's array
across lanes regardless of AoS/SoA. The real SoA benefit here is **L1 footprint** (scanning `t_hit` without
loading `prim_idx`), not inter-thread coalescing. *Fix:* "so that scanning the distances touches half the
cache footprint."

**P3 — flat rung 2.90 s vs banked 2.85 s.** `07-results.tex:41` quotes 2.90 s ("our locked-clock median");
`g1_flat.md:35` banks t_med = 2.85 s. Reconcile or note the re-run.

**P4 — "~30× sample-equivalent" is bolted onto a figure whose own numbers give 52×.**
`07-results.tex:123`: the denoiser figure reports 0.353→0.049 (7.2×) against a 1024-spp reference, then
claims "~30× sample-equivalent" — but (0.353/0.049)² ≈ 52×. The ~30× actually comes from a *different*
experiment (the g2 √spp-extrapolation vs a 2048-spp GT = 27.9×). *Fix:* source "~30×" to the g2 sweep, or
drop the effective-spp claim from this figure.

**P5 — "82–98 % cache-resident" compresses two cache levels.** `06-optimization.tex:19-20`. The source
(§8.29) is **L1 ≈ 82–84 % / L2 ≈ 98.7 %**; the "82–98 %" range reads as one metric spanning 82→98.
*Fix:* "L1 ≈ 82 %, L2 ≈ 99 % resident".

**P6 — divergence "5.4–6.9 lanes" is the two fully-profiled assets, but tornado measures 7.23.**
`06-optimization.tex:28-29`. The bracket is correct for cloud (6.95) and bunny (5.42); tornado's 7.23
(`g4_tornado_roofline_1034.csv`) sits outside it. The text scopes the claim to the fully-profiled pair, but
a reader may take "5.4–6.9" as all four. *Fix:* "5.4–6.9 of 32 lanes on the two fully-profiled assets."

---

## 3. Structural recommendations

1. **Add a one-row statistics line to the headline (Ch7).** A single table or sentence giving k_ours, k_analog,
   their 95 % CIs, the t-ratio, and the resulting 59× ± CI would convert the thesis's weakest-quantified
   claim into its best-quantified one. This is the single highest-value structural change in my domain. (S1)

2. **Promote the 4090's clean timing to anchor the 3090 headline, don't just corroborate it.** §6.9
   (`06-optimization.tex:462-481`) already establishes that k is GPU-independent (1.887/110.59 reproduce
   bit-for-bit) and that the 4090 timing excludes JIT. Use that explicitly to caveat the soft 3090 t_mits
   instead of leaving the two stories adjacent but unconnected. (S2)

3. **Merge the FLOP-estimation method note into the roofline figure's body or a footnote, and keep it.**
   `06-optimization.tex:46-50` is exactly right — pipe-counter × active-lane estimation, erf uncounted,
   presented strictly as a non-saturation argument. This is the correct way to use an uncertain roofline and
   should be *more* prominent, not buried in a caption: it pre-empts the obvious "your FLOP count is wrong"
   objection.

4. **Reorder the autopsy ledger by evidential strength, not narrative drama.** Lead with the cap-free
   streaming kill (rigorous: bit-exact, interleaved, profiled) and the footprint-reduction null (measured),
   and demote the wavefront "100–1400×" to a corroborated-but-unreproducible anecdote. Currently the least
   reproducible number is labelled "the big one" (`06-optimization.tex:329`). (S7)

5. **No cuts.** Unlike a typical over-long thesis, nothing in my domain is filler. The taxonomy table
   (tab:four-modes), the negative ledger, and the cross-arch probe all earn their place. The icosphere A/B
   (`tab:icosphere`) is the cleanest experiment in the thesis — keep it exactly as is.

---

## 4. Strongest objection (my opening question at the defense)

**"Put a 95 % confidence interval on your 59×, and tell me which clip convention and which reference
render-time produced it."**

The 59× is the number this entire thesis is organised around, and as a systems empiricist I cannot accept
a headline performance claim that walks in without an error bar — *especially* when you computed a bootstrap
CI for a 1.48× direct-lighting tweak two chapters earlier. The figure is a ratio of two inter-seed variance
estimates from sixteen seeds, and one of them (the analog reference) is a heavy-tailed, firefly-dominated
estimator whose sample variance is itself high-variance — the kind of quantity where a naïve point estimate
can be off by a third. On top of that, the value moves from ~2000× (raw) to 59× (radiance-clipped) to
~1520× (k-array-clipped) depending on a clip convention the reader never sees defined, and the *time* half
of the ratio rests on a Mitsuba render time your own notes admit was "wall, including startup" and "would
tighten the number" if measured cleanly. So: is it 59×, or is it 40×–80×, and how much of that interval is
sampling noise in k versus an unpinned reference clock?

**How the author should pre-empt it.** The honest answer is strong and the fixes are nearly free — which is
exactly why the omission is conspicuous. (a) Bootstrap the CI over the 16 banked seeds (the RIS sweep
already does this; `ris_seeds_meadow/` is the template) and print "≈59× (95 % CI …)". (b) State that the
headline is **variance-dominated**: k contributes 58.6× and the time ratio is ≈ 1, so the conclusion is
robust to the soft 3090 reference timing — and point to the 4090 row, where a JIT-excluded steady-state
time gives a clean 77×, as proof that the timing softness does not drive the result. (c) Name the clip
operation precisely and call it the most conservative of the conventions considered. Do those three things
and the objection collapses from "your headline is unquantified" to "your headline is conservative and
well-bounded" — which, on the evidence I re-derived, it genuinely is. The substance survives; the
presentation is what is currently undefended.

---

*Verification appendix (clock-independent, re-derived this session): roofline.csv → cloud 1012 GFLOP/s @
6.75 FLOP/B (2.8 % peak), bunny 404 @ 24.4 (1.1 %), tornado 66 @ 11.3 (0.19 %, 0.64 % DRAM), explosion 324
@ 16.6 — all consistent with `06-optimization.tex:30-31,43-50`. Occupancy/scheduler/divergence
(`ncu_summary.md`): cloud 31.2 %/52.9 % idle/6.95 lanes, bunny 20.9 %/70.4 %/5.42 — match §6.1. 4090
ladder (`g1_4090.md`): (110.59·4.347)/(1.887·3.285)=77.5×, /(1.887·2.309)=110.3×, /(1.653·1.922)=151.3×,
/(1.652·1.628)=178.7× — all match tab:ser-eq. k bit-identical across 3090/4090. ddof=1 confirmed in
`extract_k.py:35` and `run_g1_flat.sh:48-49`. Scaling OLS on the 4 square grids: b=0.397, R²=0.956. SER
A/B (`ser_ab.md`): 1.42/1.12/1.41/1.68× match tab:ser. Source: argmin = `min` of span-restricted
`erfinvf` free flights with no marching/root-finding (`sampling.cuh:310-433`, `primitive.h:198-236`);
active-set inheritance real (`sampling.cuh:338-355`); 0 `__shared__` in device code; SER key is 8-bit/
256-bucket on a ⅛-unit grid (`raygen.cuh:141-150`).*
