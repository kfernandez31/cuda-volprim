# Optimization Frontier — Remaining Ideas & Verdict

**Date:** 2026-06-07 · **Hardware:** RTX 3090 (Ampere, SM 8.6) · **Status of renderer:** thesis-ready
(beats Mitsuba on the env-map showcase, validated unbiased ≤1e-4 across the asset ladder).

This document is the **final optimization scope** for the project. After this, effort pivots to thesis
writing + breadth experiments (see §6). It records the small set of genuinely-unexplored ideas that
survived an adversarial review, the hard constraints any future idea must respect, and what is
**definitively dead** (do not re-attempt). It is the companion to the `FINDINGS.md §8` dead-end ledger.

---

## 1. TL;DR verdict

**The thin sliver paid off: ② volumetric RIS is a ~1.4× equal-quality WIN on env-map scenes (§8.37,
2026-06-08)** — though SCENE-DEPENDENT (~2.5× *worse* on flat/constant env), so it ships **runtime-gated
(`--ris`, default MIS)**, validated unbiased vs Mitsuba GT. It's an *algorithmic* win, not a hardware one —
it cuts the renderer's #1 cost (transmittance: 2 shadow rays → 1) AND lowers variance via product sampling
where the env is structured, and it surfaced+fixed a latent env-IS bug. The pure-perf /
structural micro-levers remain dead (§8.29–§8.36; ① exit-caching + ③ alias-table both <1 % nulls,
2026-06-08): there is **no path-changing *hardware/throughput* breakthrough on this card** — the one true
hardware lever (OptiX SER) is Ada-only, N/A on the 3090, and the megakernel sits at a defensible ~22 %
occupancy wall. **SHORTLIST NOW CLOSED (2026-06-08): ① null · ② RIS WIN (shipped) · ③ near-null (parked) ·
④ deferred (§8.38) · ⑤ dropped.** Effort pivots to thesis writing + breadth experiments (§6).
The remaining lever was **estimator quality** (RIS done; ④ guide-IS evaluated + deferred — the only
attack on the surviving multiple-scatter variance).

> Derivation: an adversarial multi-agent sweep built a 41-item tried-and-killed ledger, generated 71 raw
> candidates → 60 unique, and vetted each against the ledger + hard constraints. **5 survived as
> "uncharted-worth-trying", 29 were "marginal/low-ROI", 26 were already-dead or duplicates.** The 5
> survivors are below.

---

## 2. The hard constraints (any new idea must clear ALL of these)

These are *measured*, not assumed. They are why most ideas die.

1. **No per-ray global state.** Moving the ~352 B/ray path state (264 B of it the `active_prims`
   CompactSet) to global memory is fatal — §8.34 measured 100–1400× slowdown (super-linear L2 cliff,
   state streamed uncoalesced ×128 bounces). This kills: wavefront, neural radiance caches, ray-sorting /
   SER-substitutes, rasterized-first-bounce handoff, per-vertex global caches. *Survivors must keep
   per-ray state register-resident.*
2. **SER is unavailable.** The hardware fix for the 20.5/32-active-lane path-length divergence is
   Ada-only (SM 8.9+); the benchmark card is Ampere (SM 8.6). Only relevant if final benchmarks move to a
   40-series card.
3. **The bottleneck is structural.** Latency-bound on scattered global `Primitive`-struct loads at ~22 %
   occupancy, capped by 114 code-intrinsic registers (erf/phase chains) + warp divergence. Data is
   already 82–84 % L1 / 98.7 % L2 resident, DRAM ~1 % → footprint/cache/read-only-cache micro-ops are
   bounded to ~1 % (proven null 3× in §8.29).
4. **The ≤1e-4 bias gate is strict and unforgiving.** It killed adaptive sampling (−6e-4) and
   density-culling (+0.9 %); even `fast_acos`'s 6.8e-5 rad error became +3.5e-4 via the env gradient.
   Anything touching the estimator must clear furnace + multi-seed diff-of-means.
5. **Single-frame renderer.** No temporal reuse / no inter-frame caching.

---

## 3. The ranked shortlist (5 ideas, each with a cheap kill-test)

> **Discipline for all of them: run the kill-test BEFORE implementing.** Each idea has a cheap
> measurement that confirms or refutes its premise in minutes-to-hours. Time-box every one.

### ① Single-pass argmin exit-caching — ❌ TESTED, NOT A WIN (2026-06-08) — see FINDINGS §8.35
> **Verdict:** implemented (active-prims-only `float[128]` register/local cache), **bit-identical**
> (0/1.62M px), but **no speedup — a wash trending slightly slower** (A/B-interleaved cloud cam0:
> +2.3 % mean / +4.6 % median / +5.7 % min, within ~9 % run jitter). Measured at the admin-locked
> 150 W / 435 MHz operating point — the regime *most* favorable to caching — and it still didn't help;
> at full clocks (memory/occupancy-bound) the 512 B local spill is strictly worse. Closes the §8.29
> deferred load-COUNT lever and confirms the 22 %-occupancy wall a 4th way. Code reverted; main unchanged.

- **What:** `sample_scattering_event` (`device/core/sampling.cuh`) computes each prim's `t_exit` twice —
  once in the argmin pass (lines ~362–410) and again in the "rebuild `final_active_prims`" pass
  (~425–450). The rebuild re-issues the exact scattered `Primitive` transform loads that *are* the
  bottleneck. Cache `t_exit` from pass 1 in a small local array; replace pass 2's recompute with a
  register/local compare (`t_scatter_min <= cached_t_exit[i]`).
- **Targets:** per-spp throughput. **Distinct from the killed SoA/reorder/`__ldg` micro-ops** — those cut
  per-load *latency* (null); this removes whole loads (cuts load *count*), an untested axis. It is the
  argmin→rebuild-exit slice of the still-OPEN deferred item in §8.29.
- **Payoff:** ~2–6 % wall-clock on the cloud; could be ~0 % if those loads were already cache-resident
  at 22 % occupancy (in which case it's a 4th confirmation of the occupancy-wall thesis — still useful).
- **Unbiasedness:** bit-identical by construction (iteration order is stable: CompactSet iterates
  `data_[0..size_]`, HitBuffer SoA is index-stable) → auto-passes the gate.
- **Risk:** Low. Only real risk: the local-float scratch grows the per-ray frame and pressures the
  114-reg / 22 %-occupancy ceiling → keep the cache to **active-prims only** (that loop already computes
  the exit unconditionally; the hit loop computes it inside a conditional, so caching it there *adds*
  work for non-winning hits — leave it unless measured).
- **Kill-test:** cache only active-prims exits, build Release, render cloud cam0, **diff EXR vs baseline
  (must be bit-identical)**, compare wall-clock over 5 runs. Flat within ~1 % → dies cheaply. ≥2 % →
  extend to the hit loop and re-profile `long_scoreboard` in ncu.

### ② Volumetric RIS — ✅ SCENE-DEPENDENT WIN, validated + runtime-gated (2026-06-08) — see FINDINGS §8.37
> **Verdict:** built, validated, ship-ready (branch `feature/volumetric-ris`). Kill-test confirmed the
> premise (cutting 1 of 2 shadow rays = **−26 % frame**, vs ①/③'s <1 %). Product-RIS (K env-IS candidates
> → 1 reservoir → 1 shadow ray) is **~1.4× equal-quality on structured env** (meadow showcase: 0.83× cost
> × 0.84× variance) **BUT ~2.5× WORSE on flat/constant env** (no structure to product-sample + forgoes
> exact phase-IS). So it's **NOT a universal replacement** → gated behind a **runtime `--ris` flag**
> (default OFF = MIS). **Validated UNBIASED vs Mitsuba-analog GT** on the firefly-free constant-env rung
> (signed-mean Δ: RIS −8.5e-6, MIS −2.5e-5; both ≪1e-4) + furnace-exact. K-sweep: sweet spot **K=4–8**,
> default **K=6** (runtime `--ris-candidates`). **Bonus: surfaced + fixed a latent env-IS texel-center bug**
> (MIS-masked; also fixes ③). The FIRST frontier item to clear the bar. Not novel (Talbot 2005 / ReSTIR) —
> the contribution is the adaptation + measurement. Default build unchanged (MIS); RIS opt-in for env-maps.

### ② Volumetric RIS — product (phase × env) importance sampling, 2 shadow rays → 1 — *Medium effort, medium bias risk* — THE MOST THESIS-INTERESTING NEW IDEA
- **What:** Today's NEE/MIS fires **two** independent balance-MIS shadow rays per scatter vertex
  (`raygen.cuh:207` and `:217`), each a full `compute_transmittance_to_env` GAS descent. Replace with
  RIS: stream K *cheap unshadowed* env-IS candidates weighted by `w_i = phase(wi,dᵢ)·env(dᵢ)/pdf_env(dᵢ)`
  into a 1-survivor reservoir, then fire **one** analytic-transmittance shadow ray for the survivor with
  the unbiased RIS contribution weight.
- **Why it's novel here:** it attacks the **direction-sampling mismatch** that *neither* current strategy
  handles — env-IS ignores the peaked g=0.85 HG lobe; phase-IS ignores env structure; neither
  product-samples. This is **not** track-length (§8.32 attacks already-zero transmittance variance), not
  A1 (§8.27 is transmittance-folding), not adaptive, not splitting.
- **Why it clears the constraints:** the reservoir resolves **in-register within one vertex** (one rng
  draw + a running-sum register, no new buffers) → **no per-ray global state**, so §8.34 does not apply;
  no SER; single-launch compatible.
- **Payoff — two honest, separated wins:**
  - *Cost:* transmittance is ~85 % of frame time (§8.16) and fires ×2/vertex; **2 GAS descents → 1** is a
    direct cut on the renderer's #1 cost — plausibly double-digit % equal-quality wall-clock on
    scatter-heavy paths. Each removed sweep is an O(active_prims) erf loop on the `long_scoreboard` axis.
  - *Variance (bounded):* product-RIS sharpens the single-vertex direct term where the forward lobe
    points at structured env (the meadow sun = the showcase). **Small-to-zero** extra win on the dense
    cloud (§8.32: per-vertex direct is *not* the cloud's dominant noise; the surviving cloud variance is
    multiple-scatter, which RIS does not touch).
  - **Pitch:** an equal-quality *throughput* win (2→1 shadow ray) that matches MIS quality, plus a modest
    variance win on structured-env scenes — not a large cloud variance reducer.
- **Effort:** Medium — ~30–50 lines in the existing NEE branch (`env_is::sample/pdf`, `phase::eval`
  already exist). The real work is the correctness proof + furnace/multi-seed validation + a K sweep
  (K ≈ 4–16).
- **Risk:** Medium. (a) RIS contribution-weight bugs are subtle and the gate is strict (env-IS shipped a
  −23 % estimator bug once, §8.10). (b) Quality upside may be small on the cloud → the win must come from
  the cost cut. (c) K candidate evals add ALU/registers that could pressure the 22 % occupancy ceiling
  (or hide free under existing latency stalls).
- **Kill-test (no RIS code):** flip a one-line "1 shadow ray vs 2" flag — run a 1-shadow-ray NEE variant
  (e.g. env-IS-only) vs current 2-ray balance-MIS at fixed spp on meadow+cloud; record wall-clock +
  furnace-check the 1-ray variant. **If killing one ray gives a clear time drop (it should — transmittance
  ~85 % of frame), RIS is the device that recovers MIS quality at ~1-ray cost → build it. If the drop is
  negligible, the cost premise is false → RIS is dead before any code.**

### ③ Alias table (O(1)) for env-IS sampling — ⏸ TESTED, PARKED (2026-06-08) — see FINDINGS §8.36
> **Verdict:** implemented (Walker/Vose, branch `feature/env-is-alias-table`) + verified **correct**
> (furnace flat; 12-seed bias +8.1e-5 < 1e-4 gate). But **sub-1 % near-null**: ncu shows −0.56 % global
> loads / −0.87 % DRAM / −1 % duration and the **long_scoreboard stall unmoved (0.34→0.34)** — the env-IS
> search is only ~0.5 % of megakernel loads. No occupancy cost (global tables, not local spill → not
> *negative* like ①), but the <1 % env-only win doesn't justify +33–66 MB + perturbing the validated
> showcase by +8e-5. **Parked, not merged**; binary-search sampler stays the shipped path.

### ③ Alias table (O(1)) for env-IS sampling — *Low effort, small (~1–4 %), env-only*
- **What:** `env_is::sample` does **two per-sample binary searches** (`upper_bound`, `sampling.cuh:194–195`)
  over the global marginal/conditional CDF arrays — ~`log₂(H)+log₂(W)` *data-dependent* loads that can't
  coalesce/prefetch → they land on the `long_scoreboard` stall. Replace each with one Walker's-alias
  lookup (1–2 loads). Exactly equivalent in distribution.
- **Payoff:** small (~1–4 %), env-map scenes only. A genuinely different access pattern from the killed
  cache micro-ops. Bias-neutral by construction (provided sampled texel index + reported pdf match
  `env_is::pdf` bit-for-bit).
- **Effort:** Low (1–2 h): host-side alias table per env row + marginal; swap the searches for one indexed
  draw.
- **Risk:** Low; main risk is a within-noise null.
- **Kill-test:** stub the two `upper_bound` calls to return a constant index, render the meadow, diff
  wall-clock — this bounds the max achievable win. Barely moves → dead. Few % → build the real table.

### ④ Path guiding (shared grid as 3rd MIS strategy) — ⏹ EVALUATED, NOT PURSUED (2026-06-08) — see FINDINGS §8.38
> **Verdict:** scaffold + directionality diagnostic built (branch `feature/path-guiding`), but NOT pursued
> to a full build. Two blockers: (1) **no valid cheap test exists** — every NEE-harvest proxy measures the
> NEE-handled layer, not the multi-bounce tail guiding would help (the bounce>0 escape is already
> NEE-counted); a verdict needs the full multi-day oracle. (2) **Downside-skewed risk:** unlike RIS (which
> *removed* cost), guiding only *adds* cost (grid reads/atomics on the `long_scoreboard` bottleneck) → can
> be a **net slowdown**, exactly like adaptive sampling (§8.30) which net-lost ~2× here. Upside bounded low
> by §8.27/A1 (deep field near-diffuse). Full oracle deferred; scaffold kept for thin/structured-media future.

### ④ On-the-fly scatter-direction GUIDING (shared coarse SH grid as a 3rd MIS strategy) — *Medium-high effort* — THE ONLY LEVER LEFT FOR THE ACTUAL SURVIVING VARIANCE
- **What:** Add a shared coarse spatio-directional structure (e.g. 32³ spatial-hash grid × low-order SH
  of NEE-found env radiance) as a **third "guide-IS" MIS strategy** at deep continuation vertices,
  balance-combined with phase-IS and env-IS. Harvest the already-computed NEE env values (`T_a/T_b`,
  `raygen.cuh:206–220`) via atomicAdd — near-free to collect.
- **Why it's the right target:** it attacks the **multiple-scatter / path-length tail** (§8.32) — the
  *actual* surviving variance — which **nothing currently importance-samples** (the continuation
  direction is an unguided `phase::sample`).
- **Why it survives §8.34:** it is **one shared ~1.2 MB read-mostly structure** (high cache reuse, zero
  per-ray-count scaling), NOT per-ray state. (Prior sessions wrongly assumed any shared structure trips
  the §8.34 cliff — §8.34 forbids *per-ray* state streamed ×128, not a single L2-resident shared cache.)
- **Payoff:** modest, scene-dependent. ~1.1–1.4× equal-quality (RMSE²·time) on structured-lighting
  medium-density scenes with directional contrast; **likely near-neutral on the dense σ=7.5 albedo-0.9
  showcase cloud** (near-diffusive multiple scattering → coarse SH has little contrast). A variance lever,
  not a throughput lever — it cannot move the per-spp gap.
- **Effort:** Medium-high (2–4 days): global SH grid + spatial-hash index, atomicAdd accumulation,
  `guide::sample/pdf`, wire as a 3rd balance-MIS strategy, + validation.
- **Risk:** Medium. (a) Per-vertex SH reads + scattered atomicAdd writes land traffic on the exact
  `long_scoreboard` axis already the bottleneck — could eat the win. (b) Coarse SH may be too low-res for
  the near-diffusive cloud. (c) Unbiasedness: use the guide ONLY as an IS pdf inside MIS, never as a
  weight (same discipline as env-IS).
- **Kill-test (offline oracle):** do NOT build the atomic machinery first. Pre-render the cloud high-spp,
  bake a **static** 32³ × 2-band-SH grid (no atomics, no online learning), add guide-IS as a read-only 3rd
  MIS strategy, measure RMSE²·time vs current 2-strategy MIS on the cloud AND one structured
  medium-density scene (bunny + meadow, σ=2). This is the best case (perfect guide, zero learning cost).
  **If even the oracle doesn't beat MIS by >~1.15× → kill.** If it wins clearly on the structured scene →
  build the online version and re-measure with write traffic + ncu `long_scoreboard` delta.

### ⑤ Epanechnikov-kernel primitive (no-`erf` transmittance) — *Larger commitment; parked*
- **What:** add a compact-support shell primitive with **closed-form cubic optical depth** — no `erf` in
  the transmittance hot path — to cut the dominant erf/arith cost directly.
- **Why parked:** it changes the **primitive model** away from DSYG Gaussians, so it's a representation +
  Mitsuba-parity risk, not a drop-in tweak. Real compute upside, but a much bigger commitment than ①–④
  and it touches the validated core. Only worth it as a deliberate research direction, not a quick win.

---

## 4. Recommended sequencing (if any optimization is done at all)

Time-box each behind its kill-test; stop at the first that fails its premise.

1. ~~**① single-pass argmin**~~ — ❌ DONE (2026-06-08): bit-identical but no win (wash trending slightly
   slower); load-COUNT reduction fails for the same occupancy-wall reason latency micro-ops did. FINDINGS §8.35.
2. ~~**② volumetric RIS**~~ — ✅ DONE (2026-06-08): the kill-test confirmed −26 % headroom, and RIS
   delivered **~1.4× equal-quality** (0.77× cost × 0.84–0.93× variance) + fixed a latent env-IS bug.
   The single most defensible new algorithmic contribution; ship candidate (branch). FINDINGS §8.37.
3. ~~**③ alias table**~~ — ⏸ DONE (2026-06-08): correct but sub-1 % near-null, long_scoreboard unmoved;
   parked, not merged (FINDINGS §8.36). Park **④ guide-IS** unless ② leaves time — ④ is the only remaining
   lever for the real surviving variance but its oracle kill-test must show >~1.15× before any build.
4. **⑤ Epanechnikov** — only as a deliberate research direction, not a quick win.

---

## 5. Definitively dead — DO NOT re-attempt

All measured; see `FINDINGS.md §8`:
- Megakernel footprint: SoA / field-reorder / `__ldg` (§8.29 — bit-identical, no wall-clock; 3× null).
- Single-pass argmin exit-caching (§8.35 — bit-identical, no win / wash-trending-slower; load-count
  reduction also bounded by the 22 % occupancy wall — the 4th confirmation).
- Env-IS alias table (§8.36 — correct, bias +8e-5 < gate, but sub-1 % near-null; long_scoreboard unmoved,
  env-IS search is ~0.5 % of loads — parked on a branch, binary-search sampler stays shipped).
- Path guiding (§8.38 — deferred, not pursued: no valid cheap test, downside-skewed risk like adaptive
  §8.30 which net-lost ~2×, upside bounded low by §8.27 near-diffuse deep field; scaffold kept on a branch).
- Adaptive sampling (§8.30 — net loss, SIMT-divergence-gated, +bias).
- Density-contribution culling (§8.31 — redundant with the BVH 3σ bound).
- Track-length × argmin combine (§8.32 — env transmittance already analytic).
- A1 per-step Rao-Blackwellization (§8.27 / `A1_INVESTIGATION.md` — refined to a fundamental
  collision-vs-track-length tradeoff; the flat-env gap is NOT a bug and not worth closing).
- **Wavefront / any global-per-ray-state architecture (§8.34 — 100–1400× slower).**

---

## 6. The actual highest-ROI remaining work (the pivot)

Further optimization has **low marginal thesis value** — the renderer already clears its "good enough"
bar. The bulk of remaining effort should go to:

1. **Writing.** Including the **dead-end ledger itself** — the proven nulls (§8.29–§8.34) demonstrate
   rigor and are a genuine thesis contribution, not filler.
2. **Lock down MEASURED RTX 3090 numbers** — replace any extrapolated/interpolated figures with measured
   ones (warmup + GPU-idle, multi-seed where variance matters).
3. **Breadth: validate on 2–3 assets** (beyond the cloud) under varied conditions (σ, albedo, env) — adds
   volume and generality to the empirical chapter.
4. **Consolidate branches to a shippable main** and document the two sub-percent open correctness items
   (low-σ interior +1.8e-4 §8.13; RGB-albedo B-channel +0.0046 §8.14).

> The honest framing for the thesis: the renderer is a *correct, validated, Mitsuba-beating* GPU
> volumetric path tracer whose optimization space has been **exhaustively mapped** — the wins that exist
> (anyhit fusion, RR=12, QMC, analytic RB direct) are in; the levers that don't pay (footprint, adaptive,
> wavefront, …) are measured and explained. That completeness is the result.
