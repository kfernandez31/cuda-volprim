# Headline 95% CI — bootstrap-convention resolution (Talbot round-2 B-T1)

Banked 2026-06-21. Resolves Talbot's round-2 Blocking finding B-T1, which claimed the printed
headline CI **[54, 63]** does not reproduce from the documented method (he got **[57, 60]**).

## Finding: the printed [54, 63] is correct and reproduces the documented method.
Talbot ran a **paired / common-index** bootstrap; the footnote specifies an **independent per-arm**
bootstrap. The independent one is the statistically correct convention for two *independent* experiments
and it reproduces [54, 63] exactly.

## Reproduction (fast Gram-matrix bootstrap, identical to recomputing per-pixel variance)
Data: `results/campaign/g1_seeds/` — 16 `cuda_seed*.exr` (ours-MIS) + 16 `mits_seed*.exr` (Mitsuba-analog).
k = mean over pixels+channels of inter-seed variance (ddof=1) x 64 spp; clip = global 99.9-pct per arm,
computed ONCE, then variance. Point estimate reproduces to the digit: **k_ours=1.8872, k_mits=110.594 -> 58.60x**.

| bootstrap convention | 95% CI | matches |
|---|---|---|
| **INDEPENDENT per arm** (each arm its OWN resample index set) | **[54.3, 63.0]** | printed **[54, 63]** ✓; stable B=2000/20000/50000 |
| COMMON-INDEX / paired (both arms share ONE index set) | [56.9, 60.4] | Talbot's [57, 60] |
| pixel+seed double bootstrap | [54.9, 62.3] | Talbot's reported alt |

## Why INDEPENDENT is correct (and paired is wrong here)
S = k_M/k_O is a ratio of two variance-parameters, each estimated from 16 i.i.d. seed renders. The two
arms are **independent experiments**: `cuda_seed03` and `mits_seed03` share only an integer RNG seed but
are different renderers with different RNG streams and algorithms -- there is no matched-pair structure.
Var(log S) = Var(log k_O) + Var(log k_M), which the independent bootstrap reproduces exactly. A common
(shared) resample index set forces k_O(c) and k_M(c) to move together, injecting a spurious +Cov term
that subtracts from Var(log S) -> an artificially **narrow** [57, 60]. Paired resampling is valid only
for matched-pair data, which this is not. -> Print the wider, correct [54, 63].

## Thesis action taken
- Headline CI **unchanged at [54, 63]** (it is correct).
- Footnote (`07-results.tex`) disambiguated: "each arm's 16 seeds are resampled under its OWN independent
  index set ... a single shared index set would spuriously correlate numerator and denominator and report
  a narrower interval." Forecloses the misreading.
- P-T3 also addressed in the same footnote: the Ada row pins per-sample time at ~1.3x (Mitsuba slower),
  so the variance-only 59x is a conservative floor.

## Defense one-liner (Talbot's opening question)
"The footnote specifies each arm is resampled under its own independent index set, because the two
renderers are independent experiments with no per-seed pairing. [57, 60] is the common-index/paired
bootstrap, which correlates the arms and understates the interval; [54, 63] is the correct independent
bootstrap, and it reproduces the documented recipe exactly."
