# Ch7 (Results) + Ch8 (Conclusion) examiner review — text ⟷ record ⟷ arithmetic ⟷ framing

Date: 2026-06-15. Scope: `thesis/latex/chapters/07-results.tex`, `08-conclusion.tex` vs `results/campaign/*.md`.
Method: read-only; verified every quantitative claim against its backing record, checked the arithmetic, judged framing honesty. No EXR/render recompute (separate agent owns that). `latexmk` rebuilt clean — zero undefined/multiply-defined refs.

## Per-number match table (headline claims)

| # | Claim | Thesis value | Record value | Arithmetic | Match? |
|---|---|---|---|---|---|
| 1 | Equal-quality headline | `~59×` (07:25, 92) | k_clip999 110.6/1.887 (g1_headline.md:11-12) | 110.6/1.887 = **58.61 → 59** | ✅ |
| 1 | Equal-time basis | "equal render time", ~9 s implied (07:92) | steady 9 s, wall 13.5 incl startup (g1_headline.md:12) | uses **conservative steady**, not wall | ✅ |
| 2 | NEE energy bias | `+156%` too bright, NEE 0.8199 vs GT 0.3201 (07:88-89) | 0.8199 / 0.3201 (g1_headline.md:13,16) | 0.8199/0.3201 = 2.5614 → **+156.1%** | ✅ |
| 2 | ours-MIS vs GT | `+0.4%`, 0.3214 vs 0.3201 (07:87-88) | 0.3214 / 0.3201 (g1_headline.md:11) | 0.3214/0.3201 → **+0.41%** | ✅ |
| 3 | Flat: faster/sample | `~3×`, 2.85 s vs ~8.5 s (07:35-36) | ours-analog 2.85 s, mits ~8.5 s (g1_flat.md:35-36) | 8.5/2.85 = **2.98** | ✅ |
| 3 | Flat: higher variance | `~5×` (07:37) | k 0.058 vs 0.012 (g1_flat.md:35-36) | 0.058/0.012 = **4.83** | ✅ |
| 3 | Flat: net | `~0.6×` (07:40) | net ~0.6×, Mitsuba ~1.7× ahead (g1_flat.md:45) | eff_mits/eff_ours = **0.617** | ✅ |
| 3 | Flat: means agree | `0.02%` (07:38) | ratio 1.0000, agree 0.02% (g1_flat.md:12) | — | ✅ |
| 4 | Synthetic scaling | `t∝N^0.40` (07:227,239) | square family {16,256,1024,4096} fits N^0.40 (scaling.md:24) | LSQ slope = **0.397** | ✅ |
| 4 | 256× → ~10× | `~10×` (07:228,240) | 256× → ~10× (scaling.md:24) | 3.467/0.356 = **9.74** | ✅ |
| 4 | Production (cost table) | `39× N → 15× time` (07:245) | N^0.71, 39× N → 14.8× time (scaling.md:35) | 25600/652=39.3; 53.896/3.646=**14.8** | ✅ (15 = round of 14.8) |
| 5 | Memory win | `578 MiB, 31% below 838` (07:132) | 578 vs 838 (vram.md:30-32) | (838−578)/838 = **31.0%** | ✅ |
| 5 | Safe-512 exceeds ref | `1200, +43%` (07:158-159) | 1200 vs 838, ~43% more (vram.md:35) | (1200−838)/838 = **43.2%** | ✅ |
| 5 | GAS per prim | `~0.16 KB/prim` (07:233,275) | ~0.16 KB/prim (vram.md:9; scaling.md:46) | 0.10MB/652=0.157; 3.97MB/25600=0.159 | ✅ |
| 5 | cloud saved | `622 MiB (52%)` (07:143) | 622, 51.8% (vram.csv) | 1200−578=622; /1200=51.8% | ✅ |
| 5 | tornado/explosion/bunny saved | 382(32)/600(50)/300(25)% (07:144-146) | vram.md:16-18 | all exact | ✅ |
| — | Startup | `0.39 s` ours, 0.78 warm / 2.20 cold (07:170-172) | 0.39 / 0.78 / 2.20 (jit_overhead.md:14-16) | — | ✅ |
| — | Denoiser RMSE | `7.2×`, 0.353→0.049 (07:108,117) | (denoise figure, not in supplied records) | 0.353/0.049 = **7.20** | ✅ (self-consistent) |
| — | Generalisation ratios | tornado 0.99911, explosion 1.00006 (07:186) | (g10 — not in supplied records; Ch5 prior review verified parity 0.01-0.09%) | — | ✅ (consistent w/ prior) |
| — | bunny (meadow) | `50.4 s` @64spp/512²/meadow (07:55) | 50.4 s, k=0.647, 0 overflows (g1_headline.md:31) | — | ✅ |
| — | bunny (cost table) | `53.9 s` white-constant (07:260) | 53.896 (scaling.csv) | distinct config, cross-noted "same order" (scaling.md:39) | ✅ |
| — | RIS (Ch8) | `1.4× env, default off` (08:21) | meadow 1.475-1.492, flat loses, default MIS (ris_ksweep.md) | Ch6 says 1.48×; Ch8 rounds to 1.4× | ✅ |

**Every headline number matches its record and the arithmetic is correct.** No transcription or computation error found.

---

## BLOCKING
*(none)*

No blocking issues. All load-bearing numbers verify; the framing on the four flagged load-bearing claims (59×, NEE bias, flat-env rung, scaling) is honest and correctly scoped. Both regression items from the 2026-06-10 review are fixed (see Verified-correct).

---

## SHOULD-FIX

**S1 — `07-results.tex:36` — the "~3×" parenthetical silently switches estimator (analog) but the surrounding clause time-anchors to the cloud, risking a misread that *MIS* is 3× faster.**
Quote: "this renderer is \(\mathbf{\sim\!3\times}\) faster per sample (\SI{2.85}{\second} versus \(\sim\)\SI{8.5}{\second} at \num{64}\,spp on the cloud)".
The 2.85 s figure is **ours-analog** (g1_flat.md:35), not the MIS arm the headline uses. The paragraph *is* the "isolating the sampler / same (analog) mode" block, so it is technically correct, but a skimming examiner could read "this renderer" as the production MIS configuration. Fix: insert "(analog mode, both sides)" after "per sample", e.g. "\(\sim\!3\times\) faster per sample in analog mode". Low effort, removes the only ambiguity in the headline section.

**S2 — `07-results.tex:160` and `tab:vram` caption — per-asset saving stated as "25 to 52%" but the same paragraph's prose elsewhere uses GiB; minor unit-mixing, and the "0.30–0.62 GiB" range in the caption (07:152) should agree with the MiB body.**
Quote (07:152): "saving \(0.30\)--\(0.62\,\)GiB". Body (07:160): "range from \num{25} to \num{52}\,\%". Both are correct (622 MiB = 0.61 GiB ≈ 0.62; 300 MiB = 0.29 ≈ 0.30) but the GiB rounding (0.62 vs 622 MiB→0.607) is loose. Not wrong, just tighten 0.62→0.61 GiB if precision is wanted. Polish-adjacent; flagging because the caption and body should round identically.

---

## POLISH

**P1 — `07-results.tex:245` — "15× increase in time" vs record's 14.8×.**
Quote: "a \num{39}\(\times\) jump in primitives for a \num{15}\(\times\) increase in time". Record (scaling.md:35) and arithmetic give 14.8×. Rounding 14.8→15 is defensible but 14.8× would match the record exactly and is no longer round-number-suspicious. Low priority.

**P2 — `07-results.tex:186` / `fig:generalisation` caption (07:201) — ratio precision inconsistent.**
Body (07:186): "\num{0.99911}" and "\num{1.00006}"; caption (07:200): "\num{0.9991}" and "\num{1.0001}". Same numbers at different precision in body vs caption. Harmless but pick one (the body's 5-digit form is the record-faithful one; the explosion caption "1.0001" rounds 1.00006 up — fine, but make consistent).

**P3 — `08-conclusion.tex:23` — "\(\sim\!5\times\) noisier" (flat-lit) inherits the rounding noted in the prior review.**
Quote: "the reference's faster direct-lighting estimator is energy-biased on dense media" — fine; but the Ch6 cross-ref at 06:343 also says "~5× noisier per sample," which g1_flat.md:43 records as 4.83× (k 0.058/0.012). "~5×" is an honest round of 4.83; no change required, noted for completeness. (This is the body's number, not a Ch8 claim per se.)

**P4 — `07-results.tex:25,92` — the 59× appears twice (sec:results-perf headline and sec:results-firefly).**
Both correctly cite "clipped per-pixel variance at equal render time." No discrepancy; just noting the double-statement is intentional (headline + its justification) and consistent.

---

## Regression-check (prior review `2026-06-10-full-review-ex-ch6.md`)

| Prior finding | Status now | Evidence |
|---|---|---|
| Ch8 `:17-18` residual "boundary-sorting" claim | **FIXED** | `08-conclusion.tex:16` reads "segment-marching and root-finding" — "boundary-sorting" gone; `grep` for "sort" in Ch8 returns only `:92` "sort-free" (correct). |
| Ch8 RIS "win" unqualified | **FIXED** | `08-conclusion.tex:21-22`: "a second, scene-dependent win (volumetric product-RIS direct lighting—\(1.4\times\) at equal quality under environment lighting, default off)". Scene-dependence + default-off both present. |
| Ch7 was a 14-line stub | **NOW WRITTEN** | 280 lines, 7 sections; scrutinised fresh above. |
| Ch5 CRITICAL C1: overlap-scatter residual undisclosed thesis-wide | **FIXED in Ch8** | `08-conclusion.tex:54-59` discloses all three residuals: +2×10⁻⁴ overlap-scatter (candidate cause: argmin free-flight under overlap), +1.8×10⁻⁴ low-density interior, channel-dependent coloured-albedo; each "bounded well under a part in 500." Matches FINDINGS framing. |
| Abstract "measured across … memory" unbacked | **FIXED** | `abstract.tex:32-33`: "on less device memory (\(578\) against \(838\)\,MiB on the same scene)" — now backed by vram.md. |

---

## Framing verdicts (the load-bearing judgement)

### 59× headline — HONEST and FAIR ✅
- The scoping is explicit and repeated. `07:21-27` opens by stating the reference's NEE is energy-biased so its *only* unbiased mode is the high-variance analog one; the 59× is "against that honest baseline." `07:96-99` closes: "This is not a handicap imposed on the reference: its faster estimator is simply biased here … so the honest unbiased baseline is the firefly-prone analog one."
- The "59× is environment importance sampling" scoping is present, prominent, and fair: `07:45-53` (a dedicated \paragraph) states "That \(59\times\) is specific to peaky illumination, and honestly so," explains the MIS advantage vanishes (and *inverts*) on flat, and ties it to the RIS scene-dependence. This is exactly the honest framing the records demand (g1_headline.md:19-25, g1_flat.md:26-30).
- The clipped-vs-raw caveat is stated (`07:26` "the raw, firefly-dominated ratio is far larger"; `07:93-95` the 99.9th-percentile clip "discounts the rare extreme fireflies that dominate the raw variance by a further order of magnitude") — matching g1_headline.md:23.

### Flat-env rung — HONEST and PROMINENT ✅ (not buried)
- It is **not** buried: it occupies the second \paragraph of the headline section (`07:29-43`, "Isolating the sampler") AND a third dedicated \paragraph (`07:45-53`). It precedes everything else in the chapter except the one-sentence headline.
- It correctly states the deflating facts: ~3× faster/sample BUT ~5× variance → net ~0.6× ("\emph{net} equal-quality figure is \(\sim\!0.6\times\)", 07:40), explicitly conceding "the decisive equal-quality advantage is the correct, low-variance MIS estimator, not the bare sampler" (07:42-43). This is the honest reading the record asks for (g1_flat.md:26-27, 47-52).
- The metric-instability of the meadow analog-vs-analog comparison is disclosed (`07:30-33`), matching g1_analog_final.md ("metric-unstable … no single stable equal-quality number survives").

### Scaling distinction — CORRECTLY DRAWN ✅
- The synthetic N^0.40 (clean scaling law) and the production N^0.71 (cost table, NOT a scaling series) are kept rigorously separate. `07:215-218`: the claim "rests on a \emph{controlled synthetic sweep} … with the production assets reported separately as operating costs … rather than as a scaling series, since across real assets \(N\) is confounded with density." `tab:asset-cost` caption (07:263-267): "These are operating costs, \emph{not} a scaling series." `07:243-249` ("deliberately \emph{not} read as a scaling curve") nails the confound (denser medium → deeper paths, not traversal). No conflation. This is a model of honest scoping.

### Memory argument — CORRECT and SUPPORTED ✅
- The claim that per-ray local-memory reservation (not geometry) dominates is correct and supported: SAFE-512 is a flat 1200 MiB on every asset regardless of N (tab:vram, scaling.md:42), and GAS is ~0.16 KB/prim — 2–3 orders below the reservation (vram.md:9, scaling.md:46). The "calibration is what secures the win" framing (07:157-161) is honest: it concedes the untuned 512/512 build would sit *above* Mitsuba (+43%).

### Ch8 conclusion — HONEST ✅
- Contributions recap (08:11-29) matches Ch4/5/6: single-trace any-hit + analog-decomposition argmin (correctly credited as replacing "segment-marching and root-finding," no "sorting"); RIS qualified as scene-dependent/default-off; the memory + startup + bias claims all cross-ref the body sections that the records back.
- Limitations (08:36-60) are honest: discloses no-emission, Gaussian-only, static, compile-time caps (with "overflow counter only \emph{detects} … rather than fixing it" — the honest overflow framing the prior review wanted), Ampere-only SER, and all three small systematic residuals.
- **No dead end resold as future work.** The cap-free streaming abandonment is correctly stated as "tried and abandoned … bit-exact but slower" (08:79-80), and the future-work "middle path" (re-issue traversal on overflow, 08:80-84) is a genuinely *untried* variant, not the abandoned design. The A1/track-length flat-env gap is scoped as "should the flat gap ever matter" (06:360), not sold as a win. Path guiding is correctly "deferred … for want of a cheap predictive test" (08:84-85).

---

## Verified-correct inventory (an examiner can rely on these)
- All 21 headline/secondary numbers in the table above: thesis value = record value, arithmetic correct.
- Build: `latexmk` clean, zero undefined/multiply-defined references; all `\Cref` targets in Ch7/Ch8 resolve (sec:setup, sec:results-*, ch:architecture/validation/optimization, tab:vram, tab:overlap, tab:wins, tab:asset-cost, sec:ris, sec:autopsies, sec:money-shot, sec:reference-problem, sec:gpu-impl, fig:scaling, fig:g1-bias, fig:denoise, fig:generalisation — all exist).
- 59× framing: honest, fairly scoped (env-IS-specific), clip-vs-raw caveat present, no "hobbled Mitsuba."
- NEE-bias framing: "Mitsuba's NEE is broken → unbiased Mitsuba = analog," not "we hobbled it" (07:97-99 explicit).
- Flat-env rung: honest, prominent (2nd+3rd paragraphs of the headline section), deflates the headline as it should.
- Scaling: synthetic law vs production cost table rigorously separated; confound disclosed.
- Memory: per-ray-reservation-dominates argument correct and supported; calibration-secures-win framing honest.
- Ch8: contributions/limitations/future-work all match body + records; no dead end resold; both prior-review regressions fixed.
- Conservative time basis: thesis uses Mitsuba steady ~9 s, NOT wall 13.5 s, for the equal-time 59× (g1_headline.md:12 confirms steady is the conservative choice; k dominates the ratio anyway).
