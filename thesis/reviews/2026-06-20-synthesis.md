# Jury review — synthesis (2026-06-20)

Three independent persona sessions (Condor / Didyk / Talbot) reviewed the full thesis against the source +
banked data. Inputs: `2026-06-20-{condor,didyk,talbot}-review.md`. **All three verified every
clock-independent load-bearing number reproduces exactly.** All three verdicts: **minor revisions.**

## Grade matrix (1–5)
| # | Dimension | Condor | Didyk | Talbot | median |
|---|-----------|:--:|:--:|:--:|:--:|
| 1 | Technical correctness | 4 | 5 | 4 | **4** |
| 2 | Experimental rigour / methodology | 3 | 4 | 4 | **4** |
| 3 | Honesty & claim calibration | 5 | 5 | 5 | **5** |
| 4 | Relevance & scope | 4 | 4 | 4 | **4** |
| 5 | Argumentation & significance | 4 | 4 | 4 | **4** |
| 6 | Related work & positioning | 4 | 4 | 4 | **4** |
| 7 | Writing & style | 4 | 4 | 4 | **4** |
| 8 | Professionalism & presentation | 4 | 4 | 4 | **4** |
| 9 | Cross-thesis consistency | 4 | 3 | 4 | **4** |
| 10 | Defense-readiness | 3 | 3 | 4 | **3** |

**Honesty is a unanimous 5** — the thesis's signature strength. **Defense-readiness is the floor (3)** —
driven almost entirely by one gap: the NEE-bias mechanism. Verdict: a strong, MSc-worthy, *defensible*
thesis that needs one focused revision pass, of which exactly one item is defense-critical.

## Blocking (defense-critical)
- **B1 — the +156 % NEE-bias mechanism is missing.** (Condor B1 + Didyk B1; underlies Talbot's framing.)
  The 59× headline AND the whole Ada ladder rest on "the reference's only unbiased mode is the noisy
  analog one, because its NEE is +156 % energy-biased." The *number* reproduces; the *mechanism* is six
  words ("an over-estimate from its direct-lighting term"), and the furnace +6.5 % → cloud +156 % gap
  (~24×) is unbridged. **Condor — who wrote `volprim` — confirms the bias is INTRINSIC and handed us the
  mechanism:** `volprim`'s NEE MIS-combines an *analytic, deterministic* shadow-ray transmittance against
  an *analog-survival* continuation term on bare directional pdfs → the two strategies don't share a
  sampling measure → a direct-light surplus survives at interior vertices; stock `prbvolpath` ratio-tracks
  its NEE shadow ray and passes the furnace. Magnitude bridge: furnace is optically thin (continuation
  escapes, +6.5 %); the σ×7.5 cloud is optically thick (continuation collapses, the analytic NEE surplus
  stays finite at every interior vertex and compounds over scattering order → +156 %). He verified
  `volprim` ran at its documented defaults (Gaussian kernel, bisection, max_depth=128, which only darkens).
  *Fix:* a dedicated subsection — furnace invariant (reference-free, depth-invariant proof) → mechanism →
  cloud magnitude — cross-referenced from the 59× and the Ada ladder; record that `volprim` ran at intended
  config; bank the furnace EXR. **This is the single most dangerous question in the defense, and the answer
  is now in hand.**
- **B2 — `tab:overlap` ↔ `tab:vram` contradict on the bunny.** (Didyk B2.) `tab:overlap` lists bunny at
  245/387 with a caption claiming the estimator sizes the caps; `tab:vram` ships it at 80/528; Ch4 §4.6
  says the in-render *counters* (not the estimator) are the sizing authority. Reader sees 245 vs 80
  unreconciled. *Fix:* add a measured column (estimate vs measured vs shipped) or reword the caption to
  "estimated 3σ overlap; in-render counters set the shipped caps where they differ (see §4.6)" +
  cross-ref `tab:vram`.

## Should-fix — grouped
**Headline rigour (Talbot):**
- **59× has no confidence interval** while the 100×-smaller RIS win does (S1) → bootstrap a 95 % CI over the
  16 banked seeds, state the resample count.
- **3090 Mitsuba time is a reused wall-clock approximation** (S2) → state the headline is *variance-dominated*
  (k 58.6×, t-ratio ≈1) and point to the 4090 row (clean JIT-excluded t) as the timing anchor; or pin a clean
  3090 render.
- **Clip convention spans 34×, chosen post-hoc** (S3) → name the operation precisely ("per-pixel radiance
  clipped at a single global 99.9th-pct threshold *before* inter-seed variance") + "most conservative of the
  firefly-discounting conventions."

**Reference accuracy & credit (Condor):**
- **DSYG mis-stated as always root-finding** (S1) — it uses the closed-form inverse for single-Gaussian
  segments, bisects only in overlaps; reframe the argmin's delta as eliminating the *overlap-regime*
  root-find (more accurate AND a stronger claim). Fix abstract/§4.1/tab:complexity (§3.2 already correct).
- **Credit over-attribution** (S2) — intro + conclusion credit *both* architecture halves to Condor; scope
  to the sampler half only (abstract/§4.4 already correct).
- **Kernel generality over-claim** (S3) — the *forward* optical depth is closed-form for Epanechnikov too,
  but the argmin needs a practical closed-form *inverse*, which is Gaussian-specific (DSYG uses Newton–Raphson
  even for one Epanechnikov). Scope "applies to either kernel" to the forward integral; fix §2.5 + §8.2.
- **Heavy-overlap residual undecided** (S4) — run the cheap test (re-render cluster rung with higher-precision
  `erf⁻¹`/float64); if residual shrinks → attribute to `erf⁻¹` precision (cite DSYG §5.1's Kirk-2007 note);
  else the independence/containment hypothesis stands. Replace "X or Y" with a decided answer.

**GPU terminology (Talbot — "reads as not knowing where your data lives"):**
- **"shared memory" is wrong** (S4) — zero `__shared__` in device code; the 264 B per-ray state is *local
  memory* (L1/L2-resident). → "registers and per-thread local memory (L1/L2-resident), no global round-trips."
- **"8-cell key"** (S5, also iter1) → "256-bucket (8-bit) coherence key on a ⅛-unit spatial grid"; relabel
  the ablation rows as grid resolutions (⅛, ¼, 1/16).
- **"coalesced" per-thread buffer** (P2) → the SoA benefit is L1 footprint, not cross-thread coalescing.

**Consistency (Didyk/Talbot):**
- **Complexity notation** (Didyk SF2, iter1): O(H)/O(H²), "reference's N"→H, prose O(N+A) vs fig O(A+H) →
  state once: reference H traversals → O(H²) comparisons; ours O(N+A+H) base, O(A+H) optimised.
- **Converged-mean precision** (Didyk SF3, iter1): 0.4 % vs displayed 0.321/0.320 → use 4 sig figs
  (0.3214/0.3201) everywhere the 0.4 % appears, incl. fig:showcase caption (0.321 vs 0.321 → vs 0.320).
- **tab:wins RR +4.7 % vs fig +5.0 %** (Talbot S6) → set to +5.0 % (the +4.7 % is a stale contaminated run).
- **flat rung 2.90 vs banked 2.85 s** (Talbot P3); **~3× per-sample** rests on an unrecorded flat-env Mitsuba
  time (Didyk SF7) → present as a lower bound or record it.
- **SER over-repeated + range 1.1–1.7 vs 1.12–1.68** (Didyk SF8, iter1) → cut the redundant intro paragraph,
  unify to 1.12–1.68×.
- **fast-erf terminology drift** (Didyk SF6) → one canonical term, defined once.

**Professionalism (Didyk):**
- **Internal-doc leak: "FINDINGS" column + ~14 `(§8.x)` pointers** (SF1) — most visible professionalism
  defect; strip the column, convert real pointers to in-thesis `\Cref`/footnotes.
- **Ch6 title "Optimization" → "Optimisation"** (SF4) — propagates to header + ToC.
- **Uncited headline datasets** (SF5) — `\cite` WDAS cloud (`WDASCloud2017`) + Stanford bunny (`TurkLevoy1994`).
- **Abstract scaffold comments** (`% What is my topic?…`) left in source (P5) — delete.

**Evidence / banking:**
- **Bank the furnace EXR** (Condor S5/B1) + the **16-spp denoiser inputs**; furnace is load-bearing for B1.
- **Wavefront 100–1400×** is a one-shot 14×-spread number (Talbot S7) → lead the bullet with the cap-free kill
  (rigorous), demote 100–1400× to an explicitly-superseded anecdote.
- **Denoiser "~30× sample-equivalent"** doesn't match the figure's own 52× (Talbot P4, iter1) → source ~30×
  to the g2 sweep or drop the effective-spp claim; and the 16-spp/1024-spp numbers don't trace to a record.
- **Power-law t∝N^0.40 on n=4** (Talbot P1) → "sub-linear (power-law fit, n=4, R²=0.96)".
- **VRAM per-process vs GPU-wide** (Talbot S9) → name as a threat to validity ("different queries, margins
  indicative not exact").

**Significance framing (Didyk's 2nd objection):**
- **Attribute the headline correctly** — the 59× is delivered by a textbook MIS estimator + the reference's
  bias; the *novel* architecture is ≈0.6× at equal quality on its own. Argue the architecture's worth on its
  *true* grounds (march-/sort-/root-find-free, structurally faster per sample, *enables* the megakernel and
  thus the SER lever; the negative ledger maps the ceiling). Foreshadow once in the intro.

## Polish
Register lapses ("the big one", "don't", caption "wiggles in"); "artifact"→"artefact"; Monte-Carlo
hyphenation; BRDF acronym not introduced; orphan bib entries (`PBRT4`,…); 6 cosmetic overfull hboxes
(widest tab:icosphere 7.74 pt); "82–98 % cache" → "L1≈82 %, L2≈99 %"; divergence "5.4–6.9 lanes" scope to
the two profiled assets.

## Structural recommendations (convergent)
1. **Consolidate the bias story into one subsection** (Condor #1, Didyk) — closes B1.
2. **Move the 4090 ladder `tab:ser-eq` into Ch7 beside the 59×** (Condor #3, Didyk) — keep the SER *mechanism*
   in §6.9; co-locate all equal-quality results. *(Both examiners independently asked for this.)*
3. **Consolidate the two memory discussions** (§6.10 + §7.4) (Didyk) — also gives B2 one home.
4. **Demote Ch3 startup numbers to a forward-ref** to §7.5 (Didyk).
5. **Reorder the autopsy ledger by evidential strength** — lead with the cap-free kill, demote wavefront
   (Talbot #4).
6. **Add a headline statistics line** (k_ours, k_analog, CIs, t-ratio, 59×±CI) (Talbot #1).
7. **No cuts** to the negative-results ledger or the "isolating the sampler" / flat-env paragraphs — all
   three flag these as the honest heart of the thesis. Trim `tab:overlap` to 2 rows (Condor #2).

## Defense prep — the openers
- **Condor:** "Show me the mechanism for the +156 % NEE bias — why 2.56× here vs +6.5 % furnace, and how do
  you know it's intrinsic, not a misconfiguration of my code?" → answer now in hand (B1); play the furnace
  as the reference-free, depth-invariant proof.
- **Didyk:** same NEE question + "attribute the headline correctly — your novelty is the architecture
  (≈0.6× on its own), the 59× is MIS + the reference's bias."
- **Talbot:** "Put a 95 % CI on the 59×, and tell me which clip convention and which reference render-time
  produced it." → bootstrap the CI, name the clip op, state variance-dominated + point to the 4090 timing.

**The single most dangerous question:** Condor on the NEE mechanism. It is also the most closable — the
author of the reference has supplied the mechanism; it needs ~half a page + one banked render.
