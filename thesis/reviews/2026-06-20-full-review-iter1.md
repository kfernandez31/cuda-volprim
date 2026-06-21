# Full review — iteration 1 (2026-06-20)

First iterative review after the 4090 cross-architecture work + bunny parity + figure audit. Four parallel
reviewers (Ch4/5, Ch6 incl. new §6.9, Ch7/8/abstract/Ch1, Ch2/3 + global figures/defense). Prompt:
`thesis/reviews/2026-06-20-review-prompt.md`. Build: 73 pp, 0 undefined refs/citations.

**Verdict: 0 Blocking.** Every load-bearing number reproduces from `results/campaign/*` (the 4090 ladder
77.5/110.3/151.3/178.7× from recorded k/t; 59× = 110.59/1.887; bunny parity 0.99840; +156% NEE / +0.4%
MIS; VRAM rows; scaling b=0.40; SER table). Ch4 derivations + argmin/ADT proof match the source; SER
mechanism matches `raygen.cuh`; 4090 scoping is honest (Ada-only, 59× stays the 3090 headline, 179%
icosphere row bias-flagged).

## Should-fix
1. **`08-conclusion.tex:33-35` — the new ladder sentence conflates the GPU upgrade with SER.** "grows from
   the 59× of the 3090 to 110× with reordering" credits all of 59→110× to SER; in fact 59→77× is the
   faster Ada GPU and only 77→110× is SER (Ch6 §6.9 states this correctly). **Fix:** insert the 77× step
   ("…to 77× on the faster Ada GPU, and to 110× once reordering is enabled"). *[self-introduced this session]*
2. **`04-architecture.tex:89-90` — "replaces the reference's $N$" should be $H$.** Leftover from the
   ×N→×H audit; every other passage says $H$/$O(H)$. **Fix:** $N$ → $H$ traversals.
3. **`06-optimization.tex:~456` — SER "recovers a third to two-thirds of the frame time" overstates.**
   Actual fractional recovery (off−on)/off is 11% (tornado) to 40% (bunny). **Fix:** "an eighth to
   two-fifths," or restate as the 1.12–1.68× speedups.
4. **`06-optimization.tex:~93` — RR-depth +4.7% mislabeled "frame-time penalty."** It is an efficiency
   (k·t) penalty; in raw frame time depth-5 is actually *faster*. **Fix:** "efficiency penalty."
5. **`fig:showcase` (`05-validation.tex:278-283`) — caption/panel contradiction + rounding.** Caption says
   Mitsuba-analog "≈13.6 s wall" but the panel reads "≈9 s" (steady render); record is 13.5 s. Also caption
   "0.321 versus 0.321" vs scattering-ladder "0.321 and 0.320". **Fix:** reconcile timing (label vs caption)
   and use 0.321 vs 0.320.
6. **`07-results.tex:120-123` — denoiser numbers don't trace to a record.** Cited 16-spp vs 1024-spp,
   RMSE 0.353→0.049, 7.2×; banked run (`g2_denoiser.md`) is 64-spp vs 2048-spp, 0.178→0.034, ~28×.
   **Fix:** re-cite the banked numbers, or bank the 16-spp run. (Qualitative ~30× survives either way.)
7. **Orphan floats never `\Cref`'d** — `fig:mc-integ` (02:38), `fig:gmms` (02:182), `fig:optical-depth`
   (04:196), `tab:four-modes` (06:76). Examiners expect every float referenced. **Fix:** add `\Cref` at the
   natural call-out points.
8. **`fig:ris-ksweep` legend leaks code variable names** (`speedup_flat/studio/meadow`) vs caption
   flat/studio/showcase. **Fix:** regenerate legend "flat / studio / showcase".
9. **`fig:icosphere-sliver` panel title "N=3" → "ℓ=3"** (N is the primitive-count symbol elsewhere).
10. **Roofline cloud "16.5%" vs 16.0% bandwidth** — mixes NCU DRAM-throughput with the roofline-derived
    fraction. **Fix:** use 16.0% (or label it as the NCU figure).

## Defense gap (committee)
- **The +156% NEE bias is shown empirically but never *mechanistically* diagnosed**, nor explicitly stated
  to be the reference's intended/published configuration (not a setup artifact). This is the single
  likeliest tough defense question (the 59× rests on racing our best estimator against the reference's
  *worst unbiased* mode). **Fix:** add 1–2 sentences hypothesising the mechanism (e.g. NEE shadow-ray
  transmittance from a vertex inside overlapping primitives / double-counting at overlap — the same
  candidates already named for *our* residual at `05-validation.tex:218`) and noting the reference ran at
  its intended config.

## Polish
- SER range: "1.1–1.7×" (abstract `:38`, intro `:49`) vs "1.12–1.68×" (conclusion, Ch6) — unify.
- icosphere systematic: 0.16% (absorption, `sec:icosphere`) vs ~0.1% (scattering, `tab:ser-eq`/conclusion)
  — add a one-clause note that these are two regimes.
- Cloud SER times differ across tables: 3.27/2.30 (`tab:ser`) vs 3.29/2.31 (`tab:ser-eq`) — two sessions;
  optional footnote.
- "8-cell key" (`tab:ser` caption) — the `8` in `optixReorder(hint,8)` is hint *bits*; cell size is
  `SER_CELL_INV=8.0f`. Slightly imprecise; consider "spatial-cell key (8-bit hint)".
- `sec:analytic-od` — note the integral is over the 3σ support (the code truncates at `GAUSSIAN_DIAMETER_F`).
- single-Gaussian self-check "~2×10⁻⁵" is looser than the record (+1.7×10⁻⁵).
- tomographic "0.53 vs 0.31" (`05:57`) no longer traces to a campaign file — bank it.
- tornado called "sparse/thin" but has the 2nd-highest hit-cap (geometric overlap vs optical density) —
  one clause to distinguish.
- scaling "256× primitives → ~10× time" is ~9× (256^0.40); tighten.
- 6 small overfull hboxes (1.8–7.7 pt) on a forced full rebuild — cosmetic.
- `fig:rr-depth` y-axis "efficiency k*t" code-style → "k·t"; `fig:scattering-ladder` caption round
  0.3214/0.3201.

## Notes
- Related-work coverage (Ch2/3) judged adequate + fairly framed; no expected comparison missing.
- Several findings are figure-internal (legends/axis labels) → need plot-script regen, not just .tex edits.
