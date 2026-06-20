# Whole-thesis review — consistency, figures/captions, writing/structure (2026-06-15)

**Scope:** abstract + Ch1–8 + front matter + all ~20 figures (PNG/PDF inspected) + built `thesis.pdf`.
Three dimensions: (C) cross-chapter number consistency, (D) figures & captions, (E) writing/structure.
Read-only. Line numbers are CURRENT (post 2026-06-10 review).

**Build status:** `latexmk -pdf thesis.tex` → **clean**. "All targets up-to-date", exit 0.
`thesis.log`: **0 undefined references, 0 multiply-defined labels, 0 undefined citations**. 69 pages.
Only benign warnings: one memoir "unused global option", and **12 overfull hboxes** (all cosmetic;
largest 45.8 pt, mostly Ch5/Ch6 tables and the $600^3$ math line; none breaks the margin visibly).

**Regression check of the 2026-06-10 review:** §9.2 (`\Cref`/`\cref`) is **RESOLVED** — CONVENTIONS.md
line 23 was amended to "use `\Cref{}` everywhere as house style"; text has 170 `\Cref`, 0 `\cref`; doc
and text now agree. **Do not re-flag.** §9.1 mixture-count `K→N` at bg:148 is **FIXED** (now `N`,
bg:150). Act-first #2 "purge sorting" is **MOSTLY FIXED** (abstract, Ch3, Ch8 cleaned) — one residual
survives (B-list below). Ch5 absorption-ladder caption attribution (old M3) is **FIXED** (caption now
says pair/cloud vs Mitsuba, matching the figure's row labels). `--ris` CLI is described as a runtime
flag (architecture:437) — code-side fix is out of this review's scope (text-only review).

---

## BLOCKING (a number that disagrees, a caption that contradicts its figure, a broken ref)

**None.** No load-bearing number disagrees across chapters; every figure inspected supports its
caption; no reference is broken or undefined. The headline figures all reconcile (see tables below).

The one item that *looks* like a cross-chapter disagreement but is **not** (numbers each correct,
different quantities) is the bunny active-set cap 320-vs-245 — filed under Should-fix S1 because it is
a cross-reference clarity problem, not a wrong number.

---

## SHOULD-FIX (undefined axis/metric, unreferenced figure, notation collision, structural gap)

### S1. Bunny cap "320 suggested" (Ch4) vs `tab:overlap` "245" — same sentence, looks contradictory
`04-architecture.tex:413-415`: *"the whole-bbox bound over-sized the bunny's active set four-fold
(\num{320} suggested vs.\ \num{71} measured)"* — yet the immediately preceding clause points the reader
to `\Cref{tab:overlap}`, whose bunny "Max point overlap" row reads **245**, not 320.
Both numbers are correct but are *different columns* of the estimator (`caps_table.csv`: bunny
`active_max=245`, `cap_active=320`): 245 is the raw max overlap the table prints; 320 is the estimator's
margined **cap suggestion**. A reader cross-checking the cited table will see 245 and be unable to
reconcile the prose's 320. **Fix:** write "the estimator's suggested **cap** (\num{320}, the margined
round-up of the table's \num{245} raw overlap) vs.\ \num{71} measured" — or move the `\Cref{tab:overlap}`
pointer off this clause. (The ray-entry pair 387-vs-464 in the next clause is unambiguous: 387 is the
table's raw value, 464 the measured worst.)

### S2. Five figures are never `\Cref`'d in the body (unreferenced — do not "earn their place")
Grep confirms zero in-text `\Cref` to: `fig:mc-integ` (bg), `fig:gmms` (bg), `fig:optical-depth`
(arch TikZ), `fig:argmin` (arch TikZ), `fig:per-ray-state` (arch TikZ). Each has only its `\label`.
The three Ch4 TikZ figures are *intended* (per the task brief) but being intended ≠ being referenced;
with `[htp]` placement an unreferenced figure can float far from its context. The surrounding prose
already describes exactly what each shows, so the fix is one cross-ref each, e.g.:
- `02-background.tex` ~:41 (after the MC estimator) → `(\Cref{fig:mc-integ})`.
- `02-background.tex` ~:174 / :180 (the per-primitive optical-depth / overlap sentence) → `(\Cref{fig:gmms})`.
- `04-architecture.tex` ~:122-133 (the erf optical-depth derivation) → `(\Cref{fig:optical-depth})`.
- `04-architecture.tex` ~:183-189 (the argmin rule / "single linear pass") → `(\Cref{fig:argmin})`.
- `04-architecture.tex` ~:350-370 (per-ray state paragraph) → `(\Cref{fig:per-ray-state})`.
(For comparison, every Ch5/6/7 figure *is* referenced; `fig:bvh-dragon` and `fig:flicker` are too.)

### S3. Residual "sorting" mischaracterisation of the reference at `04-architecture.tex:7`
*"The reference~\cite{DSYG} marches the ray segment by segment through **the sorted sequence of
primitive boundaries**"*. This attributes a *sort* to the reference, which it does not do
(running-min selection — verified in the prior review against `volprim/integrators/common.py`). It
**contradicts Ch4's own corrected framing**: `fig:pipeline` caption (:60-61) and §adt (:188) both say
"selecting the next boundary by a **running minimum**". This is the same residual the 2026-06-10 act-first
#2 cleaned out of the abstract/Ch3/Ch8 but missed here. **Fix:** "marches the ray segment by segment
between primitive boundaries, selecting the next by a running minimum, and root-finds the scatter
distance" — mirror the wording already used 50 lines later.

### S4. `N` overloaded a *third* way in Ch6 (icosphere subdivision level) — notation collision
CONVENTIONS reserves `N`=total primitives (and `A`/`H`/`K`/spp). Ch6 `tab:icosphere` and §icosphere
use `N` for the **subdivision level** ("subdivision $N$: 20, 80, 320, 1280", :234; rows `icosphere
$N{=}0..3$`, :249-252). In a chapter whose `tab:overlap` and prose use `N`=primitive count throughout,
the same glyph now means "icosphere subdivision index". **Fix:** rename the subdivision index (e.g. `\ell`
or "level $L$") in the table header, the four rows, and :234/:273/:279/:284/:289. (The earlier-flagged
bg:20 MC-sample-count `N` and bg:148 mixture `K` are separate items — see S5.)

### S5. MC sample-count `N` (bg:20-32, val:144) still collides with primitive `N` — old §9.1 item, unfixed
`02-background.tex:20` `\langle F\rangle=\frac1N\sum_{i=1}^N`, :25-32 "$O(1/N)$ … $1/\sqrt N$" use
`N`=sample count, while bg:148/197 use `N`=primitives. `05-validation.tex:144` `k=\mathrm{RMSE}^2\cdot N`
is again sample-count `N`. CONVENTIONS mandates **spp** for sample count. The 2026-06-10 review flagged
this (§9.1) and it remains. **Fix:** use spp (or `n`) for the estimator sample count in bg §MC and at
val:144; reserve capital `N` for primitives thesis-wide. (Low-stakes since context disambiguates, but
the convention is written and currently violated.)

### S6. Ch7 chapter intro miscounts axes and omits the headline section (signposting)
`07-results.tex:9-16`: *"The chapter reports **four axes** …"* then lists **five** (firefly, denoise,
memory, startup, generalisation). It also opens the list *after* skipping §7.1 (Equal-quality
performance — the `59×` headline, `sec:results-perf`), so the chapter's single most important section
is unsignposted in its own intro. **Fix:** "four further axes … *beyond the headline equal-quality
comparison (\Cref{sec:results-perf})*" — i.e. name §7.1 first, then either say "five" or fold one axis,
and keep the count honest. (Also §7.8 scaling is a 7th section, called out separately, which is fine.)

### S7. `tab:wins` "denoiser ~30× effective" has no bridge to the body's 7.2×
`06-optimization.tex:106` lists "OptiX denoiser (showcase) ~30× effective" (sourced §8.22); the only
denoiser number in the body is Ch7's **7.2×** RMSE reduction (16→1024 spp), and Ch7:111 says the
equal-quality gain "is recorded in `\Cref{tab:wins}`". So the reader meets 7.2× in prose and 30× in the
table for "the denoiser" with no derivation of the 30× anywhere. The two are different metrics
(RMSE-drop on a 16-spp cloud vs equal-quality effective-sample gain on the showcase) — defensible, but
the 30× should either get a one-clause justification in Ch7 §denoising or the table cell should name its
metric ("~30× effective spp, showcase"). **Fix:** add the metric/scope to the cell or a sentence in §7.3.

---

## POLISH (tense, wording, micro-consistency)

- **P1 — tense (old §9.3, partly unfixed).** Ch5 reports measured results in present tense pervasively:
  `:166` "The renderer **matches** this analytic value", `:210` "passes this furnace test", `:215`
  "**matches** to within noise", `:266` "**matches** the converged image". CONVENTIONS:11 reserves past
  tense for what was measured; §5.1 (`:22` "were measured") already uses past. Ch7 mostly complies
  ("renders in", "was checked"). Decide the rule once; if the timeless-property voice is deliberate,
  amend CONVENTIONS — otherwise sweep Ch5 to past. (Abstract `:31` "it reaches … faster" is the same
  call; prior review flagged it.)
- **P2 — RIS rounding `1.4×` vs `1.48×`.** Abstract:35 and Ch8:21 say `1.4×`; Ch6 (the source) says
  `1.48×` six times. Internally consistent (1.4 ≈ 1.48 rounded), but the abstract/conclusion could match
  Ch6's precision ("`1.48×`") for a tighter cross-document trace; at minimum keep them identical to each
  other (they already are). Not an error — flag only because the brief asked to trace the 1.4× headline.
- **P3 — "meadow" vs "showcase" used interchangeably for the same environment.** `fig:ris-ksweep`
  legend says `speedup_meadow` while its caption + Ch6 prose call the high-peak case "showcase";
  Ch5/Ch7 captions say "meadow environment" for the showcase scene. Harmless (they are the same HDR),
  but a reader may wonder if "meadow" and "showcase" are two scenes. Consider one term, or a one-line
  "(the meadow HDR, our showcase environment)" gloss at first use.
- **P4 — `fig:absorption-ladder` caption claims view-independence the single-view figure can't show.**
  `:196` "The renderer matches **view-independently**." The figure shows one camera per rung. The claim
  is true (Ch5:184 establishes it across 24 views) but the *figure* doesn't depict it. Soften the
  caption to what the panel shows ("…to its closed-form / Mitsuba reference; view-independence is
  established in the text") or drop "view-independently" from the caption.
- **P5 — `tab:vram` (Ch7) and `tab:overlap` (Ch6) print different cap values for the same assets**
  (tornado hit 384 vs estimator 432; explosion 160 vs 176; bunny 528 vs 496/387) with no explicit
  "estimator-ceiling vs measured-calibrated" bridge in Ch7. Each table *is* labelled (Ch6 "predicted
  offline by the cap estimator"; Ch7 "calibrated"), so it is not wrong — but one sentence in §7.4
  ("the calibrated caps below are tighter than the offline ceilings of \Cref{tab:overlap}, measured
  in-render") would pre-empt the apparent mismatch. Polish.
- **P6 — `acknowledgements.tex` "Piotr Rybicki" vs MEMORY "Prybicki".** Acks say **Rybicki**; the
  external memory note spells it "Prybicki". The thesis is self-consistent (Rybicki throughout); flag
  only to confirm the spelling is correct before submission. No thesis edit needed if "Rybicki" is right.
- **P7 — title page** `thesis.tex:75` advisor "Prof.\ Dr.\ P.\ Didyk" (prior review's "P. K. Didyk"
  is **FIXED** — now "P. Didyk", matching acks "Piotr Didyk"). Date "Luxembourg, June 2026" is
  deliberate (per prior review). No action.

---

## (C) CROSS-CHAPTER NUMBER-CONSISTENCY TABLE

| Quantity | Value(s) | Appears in | Verdict |
|---|---|---|---|
| Equal-quality headline | **59×** | abstract:31, results:25/45/49/92 | **CONSISTENT** (same value, same scope = env-lit showcase vs analog; results:45-49 adds the env-IS caveat the abstract omits but scopes via "environment-lit showcase") |
| RIS win | **1.48×** Ch6 / **1.4×** abstract+Ch8 | opt:139/149/165/173/182; abstract:35; concl:21 | CONSISTENT (rounding; P2) |
| RIS plateau / default K | K=4–6 plateau, **K=6** default | opt:140/151/152/165/178 | CONSISTENT |
| RIS K=1 anchor | 1.19× (time only) | opt:144 | single source, OK |
| NEE energy bias | **+156 %** (mean 0.8199 vs 0.3201) | results:78/89; fig:g1-bias (0.820 vs 0.320) | CONSISTENT (fig & text & %) |
| MIS vs GT offset | **+0.4 %** (0.3214 vs 0.3201) | val:229 fig; results:76/87; fig:g1-bias; fig:scattering-ladder (0.3214/0.3201) | CONSISTENT across 4 sites |
| Furnace: Mitsuba-NEE bias | **≈6.5 %** | val:210 | single source, OK |
| Scattering-ladder SE ratio | **~68×** | val:231/232; fig (68×) | CONSISTENT (distinct from 59×; correctly not conflated) |
| Flat-env per-sample speed | **~3×** | results:35 | single source |
| Flat-env per-sample variance | **~5×** | results:37; opt:343 (§A1) | CONSISTENT |
| Flat-env net equal-quality | **~0.6×** | results:40 | single source |
| Memory: cloud tuned/Mitsuba | **578 / 838 MiB** | abstract:32-33; arch:365-366; results:132; tab:vram; tab:asset-cost | **CONSISTENT** (5 sites) |
| Memory: safe build | **1200 MiB** flat | arch:362/398; results:143-146/150/157/230/273; fig:scaling | CONSISTENT |
| Memory below ref by | **31 %** (and safe exceeds by 43 %) | results:132/158 | CONSISTENT |
| Memory saving range | **0.30–0.62 GiB (25–52 %)** | arch:363; results:151/160; tab:vram | CONSISTENT |
| Per-asset calibrated caps | cloud 64/96, tornado 112/384, explosion 32/160, bunny 80/528 | tab:vram | CONSISTENT w/ `cap_calibration.md` |
| Estimator-ceiling overlap | cloud 45/96, tornado 84/340, explosion 23/136, bunny 245/387 | tab:overlap | CONSISTENT w/ `caps_table.csv` raw cols (≠ tab:vram by design; P5) |
| Bunny estimator vs measured | 320 cap / 71 measured (active); 387 / 464 (entries) | arch:414-415 | numbers correct but cross-ref to tab:overlap (245) is confusing — **S1** |
| Cloud worst measured | 45 overlap / 85 entries | arch:407-408 | CONSISTENT (85 measured → 96 estimator/cap) |
| Scaling exponent (synthetic) | **0.40** (t∝N^0.40) | results:226/239; fig:scaling | CONSISTENT (fig title "t∝N^0.40") |
| Scaling exponent (production) | **— not stated** | (prompt expected 0.71) | NOT IN TEXT; assets explicitly *not* read as a curve (39× prims→15× time). No finding. |
| Asset primitive counts | cloud 652, tornado 768, explosion 1024, bunny 25600 | val:182; opt:405-411; results:55/245/257-260; concl:47; arch:412 | **CONSISTENT** everywhere |
| WDAS fits | 768 / 4096 / 24576 | tab:overlap; opt:393-394 | CONSISTENT |
| BVH per-prim / range | ~0.16 KB/prim; 0.10 MB (cloud) → 3.97 MB (bunny) | results:130/233-234/275; fig:scaling | CONSISTENT |
| HG anisotropy | **g = 0.85** | bg:121; val:229/243/265/277 | CONSISTENT |
| Albedo | **0.9** (scatter); **0** (absorption); **1** (furnace) | val:158/203/215/229/243; results:263 | CONSISTENT |
| spp / resolution | **64 spp**, **512²** (showcase/scaling); 128 spp (icosphere/abs ladder) | val:278; results:56/218/264; opt:256 | CONSISTENT (128 spp clearly scoped to icosphere/ladder) |
| σ_t scale | 7.5 (cloud showcase) / 10 (asset-cost) / 60 (exploratory) | val:69-72; results:264 | CONSISTENT (each scoped) |
| Icosphere ref speedup / best shell | 4.96× / 320 tris (N=2) | opt:230/281/289; tab:icosphere | CONSISTENT |
| Analytic-sphere price | 1.17–1.58× | opt:265; tab:icosphere (0.63–0.85× rel) | CONSISTENT (1/0.63=1.58, 1/0.85=1.17) |
| Faceting bias (N=2) | ~0.16 % | opt:285; fig:icosphere-sliver | CONSISTENT |
| Denoiser RMSE drop | **7.2×** (0.353→0.049) | results:107/117; fig:denoise (0.353/0.049, 7.2×) | CONSISTENT |
| Denoiser "effective" | **~30×** | tab:wins:106 | un-bridged to 7.2× — **S7** |
| RR depth | 5→12, basin 8–12, +4.7 % (5:+5.0/16:+4.9) | opt:84-89/102/125-126; fig:rr-depth | CONSISTENT (fig min at 12) |
| Startup latency | 0.39 s vs 0.78 s warm / 2.20 s cold | results:169-173; rel:97 | CONSISTENT |
| Wavefront slowdown / per-ray state | 100–1400× slower; ~352 B/ray | opt:314/331/383; arch:431 | CONSISTENT |
| Occupancy / roofline | 21–31 % occ; cloud 1.0 TFLOP/s@6.8, bunny 0.4@24 | opt:17/27/30/42-44; fig:roofline | CONSISTENT (fig matches) |
| Generalisation ratios | tornado 0.99911, explosion 1.00006 | results:186; fig:generalisation (0.9991/1.0001) | CONSISTENT (rounding) |
| Voxel-grid convergence | 128³→600³: RMSE 0.072→0.057, mean 0.394→0.401→0.416 | val:102; fig:voxel-gt | CONSISTENT |
| Overlap-scatter residual | +2e-4 cluster / +1e-4 cloud, <0.2 % peak | val:217-219; concl:54-57 | CONSISTENT (now disclosed in both — old C1 fixed) |
| Low-density brightness systematic | +1.8e-4 | concl:57 | single source (Ch8 limitations); OK |

**No row disagrees.** Two rows (caps tables P5, bunny 320/245 S1) need a cross-reference gloss, not a
number change.

## (D) PER-FIGURE CHECK TABLE

| Figure | File:line | Referenced? | Supports caption? | Notes |
|---|---|---|---|---|
| fig:mc-integ | bg:43 | **NO** | yes | textbook MC plot — **S2** (add `\Cref`) |
| fig:gmms | bg:182 | **NO** | yes | ray through mixture — **S2** |
| fig:bvh-dragon | bg:208 | yes (:201) | yes | OK |
| fig:flicker | rw:39 | yes (:31) | yes | OK (popping → global sort, correct) |
| fig:pipeline | arch:65 | yes (:29) | yes | reference vs ours; "running minimum" wording correct |
| fig:optical-depth | arch:163 | **NO** | yes (whitened-frame geometry, axes/symbols defined) | TikZ intended — **S2** |
| fig:argmin | arch:246 | **NO** | yes (argmin of free flights; matches §adt) | TikZ intended — **S2** |
| fig:per-ray-state | arch:399 | **NO** | yes (H×6B / A×2B, VRAM floor) | TikZ intended — **S2** |
| fig:voxel-gt | val:120 | yes (:98/:114) | yes (×5, 600³, silhouette band visible) | OK |
| fig:absorption-ladder | val:197 | yes (:189) | mostly | "view-independently" not shown by single-view panel — **P4**; row labels (analytic/Mitsuba) now match caption (old M3 fixed) |
| fig:scattering-ladder | val:236 | yes (:224) | yes | fig shows 0.3201/0.3214, "agree 0.4 %", "68× tighter" — all match text/caption |
| fig:showcase | val:283 | yes (:271) | yes | left clean / right firefly-dominated; means 0.321/0.321 |
| fig:roofline | opt:47 | yes (:30) | yes | axes (FLOP/B, GFLOP/s) + both roofs labelled; cloud/bunny points match |
| fig:rr-depth | opt:127 | yes (:84) | yes | metric "k·t" defined in caption; min at 12; 5/16 worse |
| fig:ris-ksweep | opt:167 | yes (:146) | yes | flat<1 all K, studio 1.45, showcase 1.48, plateau 4–6 — match; legend "meadow"=showcase (**P3**) |
| fig:ris-noise | opt:183 | yes (:170) | yes | 0.173/0.160, 9.6/7.7 s → recompute to 1.48× (verified) |
| fig:icosphere-sliver | opt:302 | yes (:276) | yes | 3149 px (~3150), RMSE 8.2e-3 (=tab N=3), ×4; slivers visible |
| fig:denoise | results:119 | yes (:106) | yes | 0.353→0.049, 7.2× — exact match |
| fig:generalisation | results:205 | yes (:186) | yes | 0.9991/1.0001, ×10, 256 spp; flip noted in caption |
| fig:scaling | results:235 | yes (:215) | yes | t∝N^0.40, flat 1200 MiB, BVH 0.16 KB/prim, 0.10/3.97 MB — match |

**Figures that don't earn their place:** none are decorative/redundant, but the **five unreferenced
figures (S2)** technically don't earn their place *as cited evidence* until they're `\Cref`'d — the fix
is trivial (one cross-ref each) and all five are content-justified by adjacent prose.

**No caption claims more than its figure shows**, with the one soft exception **P4** (absorption-ladder
"view-independently" on a single-view panel).

## (E) WRITING & STRUCTURE

- **Abstract ⟷ body:** every abstract claim has an in-thesis home with matching numbers — 59× (Ch7),
  1.4×/RIS (Ch6, P2), 578/838 MiB (Ch7), energy-bias (Ch7), firefly-free (Ch5/7), startup implied. The
  abstract's "measured across time and image quality" (memory dropped from the phrasing) is now honest —
  the old §1 "measured across … memory" overclaim is gone (memory results now exist in Ch7). **Good.**
- **Ch1 contributions ⟷ delivery:** all four bullets land. (1) single-trace + argmin architecture → Ch4.
  (2) from-scratch CUDA/OptiX path tracer → Ch4 §gpu-impl. (3) differential-validation doubling as
  unbiasedness evidence → Ch5. (4) GPU perf study + RIS win + negative-results ledger → Ch6/Ch7. The
  Condor-attribution hedge ("an approach the reference's authors anticipated but did not implement") is
  present and consistent with Ch4 §adt "Novelty and prior art". **Aligned.**
- **Signposting:** strong overall (each chapter intro foreshadows; CONVENTIONS' "foreshadow the
  contribution" rule is followed). Two gaps: **S6** (Ch7 intro miscounts axes + omits §7.1) and the
  Ch7 tables' missing estimator-vs-calibrated bridge (**P5**).
- **Redundancy:** the per-ray-state / VRAM-floor story is told in arch §gpu-impl, opt §memory, and
  results §memory — this is deliberate (architecture → why it's load-bearing → measured), not
  duplication; numbers agree. The "megakernel-shaped / latency-bound" thesis recurs in arch, opt
  §bottleneck, opt §autopsies, concl — consistent and intentional (it's the through-line). No section
  merely repeats another.
- **Front matter:** title/author/advisor/co-advisor/date OK; acks consistent (Didyk/Condor/Rybicki/
  family). P6/P7 are confirm-only.

---

## SUMMARY FOR CALLER
- **Blocking: 0.** No cross-chapter number disagrees; no caption contradicts its figure; build is clean.
- **Should-fix: 7** — S1 bunny cap 320-vs-table-245 cross-ref confusion; S2 five unreferenced figures
  (incl. the 3 Ch4 TikZ); S3 residual "sorted sequence" mischaracterising the reference at arch:7
  (contradicts Ch4's own "running minimum"); S4 `N` overloaded as icosphere subdivision in Ch6; S5
  MC-sample `N` vs primitive `N` (old §9.1, unfixed); S6 Ch7 intro "four axes" miscount + headline §7.1
  unsignposted; S7 denoiser "~30× effective" un-bridged to body's 7.2×.
- **Polish: 7** — P1 Ch5 present-tense for measured results; P2 1.4×/1.48× rounding; P3 meadow/showcase
  naming; P4 absorption-ladder "view-independently" caption; P5 cap-table estimator-vs-calibrated bridge;
  P6 Rybicki spelling; P7 title page (confirm-only, prior fix verified).
- **Most important consistency note:** there is **no DISAGREEMENT** — the 59×, 578/838 MiB, asset counts,
  caps, 0.40 exponent, g/albedo/spp all match everywhere. The two cap *tables* (Ch6 estimator ceiling vs
  Ch7 calibrated) intentionally differ and are labelled; S1 is the only spot where two values for "the
  same thing" sit in one sentence and need a one-word gloss.
- **Prompt's "0.71 production exponent" is absent** from the current text (only 0.40 synthetic); assets
  are deliberately not read as a scaling curve. Not a finding.
- **Build:** `latexmk` clean — 0 errors, 0 undefined refs/citations, 0 multiply-defined; 69 pp; 12
  cosmetic overfull hboxes.
- **Scratch path:** `thesis/reviews/scratch/consistency-figures-writing.md`.
