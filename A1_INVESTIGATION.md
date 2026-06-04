# A1 (per-step Rao-Blackwellization) — investigation → DEAD END

**Branch:** `feature/a1-per-step-rb`  ·  **Date:** 2026-06-04  ·  **Verdict: not worth implementing.**

## Premise (what A1 was supposed to be)
FINDINGS §8.5 measured CUDA ~2.85× noisier per sample than Mitsuba `volprim_prb` on the scattering
cloud and hypothesized the fix as **per-step RB**: "Mitsuba folds analytic segment transmittance
into throughput every bounce (`β*=seg_tr`); CUDA only does it at bounce 0 (`ENABLE_ANALYTIC_DIRECT`)."
§8.5 flagged this root-cause as **"never profiled."** This investigation tested it before touching
the validated core estimator.

## What the investigation found (4 experiments)

**1. The literal premise is false — both estimators are analog.**
Read both: our `sample_scattering_event` (argmin/ADT free-flight, `device/core/sampling.cuh:325`)
and Jorge's `volprim_prb.sample_segment` + `primitive_tracing` (`~/jorge/volumetric_primitives`).
Both sample a single scatter location from the transmittance CDF with a binary escape; Jorge's
`β*=seg_tr` is the free-flight distance-sampling accumulator, and his `weight_rr` is just RR
compensation — **neither folds transmittance into the path throughput in a way we don't.** There is
no "per-step RB" Mitsuba does that we're missing. SDTracking Thm 1 makes our argmin-min ≡ his
combined CDF in distribution. → the named fix does not exist.

**2. The gap is flat-lighting-only; the showcase already wins** (`renders/a1`, FINDINGS §8.15).
- Env + MIS (cloud meadow, the showcase): CUDA per-seed noise **0.025** vs Mitsuba **0.165 unclipped
  / 0.024 clipped** → CUDA **6× cleaner total** (firefly-free), bulk within ~5%.
- Constant env (σ=2 cloud): CUDA **0.0125** vs Mitsuba **0.0041** → CUDA **~3× noisier**.

**3. It's the per-vertex estimator, NOT argmin overlap** (`tools/refs/a1_isolate.sh`).
Single Gaussian (NO overlap), constant env, σ=2, alb 0.9, 6 seeds: CUDA **5.0× noisier**
(0.00643 vs 0.00128). A no-overlap scene still shows the gap → not an ADT/overlap effect.

**4. It's NOT the MIS env-IS overhead.** Rebuilt `ENABLE_MIS=false` (NEE only): noise 0.00607 —
only **1.06×** better. NEE-only is still **4.74×** noisier than Mitsuba-analog.

**5. The gap is in the converged multiple-scatter term** (`tools/refs/a1_depth_sweep.sh`).
Per-seed noise vs max depth (single-G const-env):

| depth | CUDA | Mitsuba | ratio |
|------:|-----:|--------:|------:|
| 1 | 0.00599 | 0.00375 | 1.60× |
| 2 | 0.00639 | 0.00147 | 4.34× |
| 4 | 0.00643 | 0.00128 | 5.02× |
| 8 | 0.00643 | 0.00128 | 5.02× |

CUDA's noise is **flat with depth** (set at low order); **Mitsuba's noise DROPS with depth** and
converges low. In a near-conservative constant medium the analog random walk self-averages (almost
every path escapes ≈ env → near-deterministic), a benefit a NEE-per-vertex estimator does not get.

## Why the obvious fix is worse, not better
The candidate "add the continuation-ray escape as an MIS strategy" (textbook BSDF-sampling-hits-
emitter, which Mitsuba MIS-combines) is **strictly worse than what CUDA already does**: CUDA's
phase-IS NEE (`raygen.cuh:195-203`, strategy A) estimates the same phase-sampled direct env but with
an **analytic** shadow-ray transmittance, vs the **stochastic** escape (`1 with prob T`) that the
continuation strategy would use. Same expectation, higher variance. So that rewrite would *raise*
variance on the direct term.

## Conclusion
- **Per-step RB (literal): impossible** — both estimators are analog.
- **The flat-env variance gap is a fundamental NEE-vs-analog tradeoff**, confined to the multiple-
  scatter term in conservative + constant-emitter media. Analog wins there; NEE wins for real env
  maps. There is no universal estimator that beats both.
- **It does not affect the showcase** (cloud + meadow + HG + MIS), where CUDA already match-and-beats
  Mitsuba (6× cleaner total, firefly-free, faster per-spp than its MIS — §8.11).
- The only way to win the flat-env case is to switch the indirect term to analog there — which would
  abandon the NEE advantage that wins env maps, i.e. a scene-adaptive hack that helps only the
  non-showcase regime.

**→ A1 shelved.** The remaining real lever for the flat-env equal-quality gap is the *throughput*
half (~1.93× per-spp), which is the **wavefront** refactor — independent of this estimator question.

## Footnote: a small mean residual (separate from variance)
At convergence the means differ ~0.5% (CUDA 0.9947 vs Mitsuba 0.9893 at σ=2 single-G). Consistent
with the low-σ interior (§8.13, +1.8e-4) and RGB-albedo (§8.14, B +0.0046) residuals — a small
CUDA-slightly-brighter systematic, candidate causes the `MIN_THROUGHPUT` cull threshold (CUDA 1e-4
vs Mitsuba `β<0.005`) or a minor multiple-scatter difference. Tracked as an OPEN item; not A1.

## Artifacts (on this branch)
- `tools/refs/a1_isolate.sh` — single-G no-overlap noise isolation.
- `tools/refs/a1_depth_sweep.sh` — bounce-depth variance localization.
- `renders/a1/` — the seed renders behind the tables above.
