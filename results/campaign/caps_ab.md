# Active-set cap sensitivity A/B (64 vs 128), measured 2026-06-11

**Question.** Cloud's measured worst-case overlap is 45; `MAX_ACTIVE_PRIMS=128` is 2× the estimator's
suggestion (64). Is the headroom free, as `constants.cuh` asserts, or a hidden cost?

**Method.** Two full builds differing only in the constant (64 vs 128; `HIT_BUFFER_CAPACITY=128`
both). Correctness gate first: 1024-spp seed-42 scattering render at 64 vs the saved 128 render —
means equal to 6 decimals (signed-mean Δ −9×10⁻¹¹), per-pixel diffs at float-rounding scale
(max 7×10⁻⁵; recompile under fast-math reorders FMA contraction); zero overflows. Then an
**interleaved** timing A/B (cloud-meadow scattering, 64 spp, 3 rounds alternating builds, same GPU
state) — interleaving was essential: same-day GPU state varied ±6 % and a *desktop session on the GPU*
made the whole day ~3× slower per spp than the previous evening, which initially masqueraded as a 2×
"cap regression."

**Result.**
| build | times (s) | median |
|---|---|---|
| 128 | 24.78, 27.25, 27.49 | 27.25 |
| 64  | 25.02, 26.86, 24.49 | 25.02 |

Between-arm difference < within-arm jitter → **no measurable timing effect** of 64 vs 128.

**Conclusion (for Ch 4).** The two caps are asymmetric: the **hit buffer** (6 B/entry) is the binding
per-ray buffer — oversizing it is expensive (128→256 measured ~6× on the cloud) — while the
**active set** (2 B/entry CompactSet) tolerates 2× headroom at measured-zero cost. So the estimator's
tightness matters for the hit buffer; the active-set cap needs only adequacy. Repo keeps 128/128 for
the cloud (adequate + free headroom); per-asset sizing still applies to denser assets (bunny 320/496).

**Method caveat for the campaign:** timing at the un-pinned 150 W operating point swings ~3× with
ambient GPU state (desktop session, clocks). All reported timings must come from the locked-clock
window; interleave any small-effect A/B (re-confirms the §8.35 lesson and the spec §4 requirement).
