# Thesis Design Spec — *Efficient Volume Rendering Through Primitive-Based Kernel Mixture Volumes*

**Author:** Kacper Kramarz-Fernandez · **Advisor:** Prof. Dr. Piotr K. Didyk · **Co-advisor:** Jorge Condor
**Institution:** Faculty of Informatics, USI Lugano (EUMaster4HPC double degree: U. Luxembourg HPC → USI Computational Science)
**Date:** 2026-06-08 · **Status:** design locked — ready for `writing-plans`

---

## 0. Purpose of this document

The agreed plan for *writing* the thesis: what it argues, how it's structured, which experiments produce its
figures/tables, and how the dead ends are written up. Produced by a brainstorming pass over the existing 31pp
LaTeX draft, `FINDINGS.md`, `OPTIMIZATION_FRONTIER.md`, the historical optimization docs (`optimizations.md` et al.,
recovered from git history), and the current code. The companion **execution plan** (chapter-by-chapter writing
tasks + experiment runs) is produced next via the `writing-plans` skill.

---

## 1. What the thesis is (and isn't)

- **Is:** a **performance-engineering thesis** — a from-scratch, production-quality CUDA/OptiX path tracer that
  realizes DSYG's analytic Gaussian-volume formulation, validated against Mitsuba and then systematically optimized.
- **Isn't:** a novel-media-extension thesis. The original proposal's "explore one extension"
  (emissive/SGGX/parametric/fluids) was **not pursued and is explicitly out of scope** — the supervisors have
  accepted this.
- **Contribution boundary:** Jorge owns the physics/math; **Kacper owns the systems realization.** That boundary
  *is* the contribution.
- **Bar:** a rigorous engineering MSc thesis (research novelty not strictly required) — **but it has genuine novelty
  anyway:** the **ADT scatter-sampling scheme (§2.1) is the headline algorithmic contribution**, with volumetric
  product-RIS as a second. This carries the thesis well past the minimum bar and is the kernel of any future paper.

## 2. Contribution statement

1. **A novel single-trace, sort-/march-/bisection-free rendering architecture for ray-traced Gaussian kernel-mixture
   volumes**, in two complementary halves:
   - **(N1) Single-trace anyhit-buffer collection with analytic exits** — one OptiX trace per scattering event; an
     **anyhit-only** program (no closesthit) with **backface culling on** buffers *all entry hits*, and exits are
     computed in **closed form** (analytic sphere/ellipsoid intersection) rather than re-traced. Collapses the O(N)
     BVH traversals of conventional segment-marching to **one traversal**.
   - **(N2) Analog decomposition tracking (ADT) scatter sampling** — each collected primitive draws an *independent*
     analytic free-flight sample (closed-form erf optical depth) and the **argmin** is the scatter event (SDTracking
     Theorem 1: the min of independent free-flights equals the combined-medium free-flight). Eliminates the
     **boundary-sorting + bisection/Newton root-find** for the scatter point.

   Together they replace the segment-marching + sorting + bisection of DSYG's own (Mitsuba) reference — **genuine
   novelty the DSYG authors wanted but did not realize** (sort-free in spirit like Stochastic Splats, but for
   volumetric *path tracing*, not rasterization). *Honest framing:* the building blocks (ADT theorem; anyhit
   collection; backface-cull; analytic intersection) exist in the literature — the contribution is their **synthesis
   into a sort/march/bisection-free volumetric path tracer for this representation, realized and validated.**
2. A from-scratch, production-quality **CUDA/OptiX path tracer** realizing this scheme (atop DSYG's analytic
   Gaussian-volume formulation) at high performance.
3. A **differential-validation methodology** establishing physical correctness against Mitsuba — born precisely
   because the supplied voxel-grid references were unreproducible; it doubles as the **unbiasedness proof of the ADT
   scheme** (the §2/§8 ladder).
4. A systematic **GPU performance-engineering study**: a *second* algorithmic win (volumetric product-RIS) plus a
   rigorously profiled **ledger of negative results**, measured across **time, quality, and memory**.

## 3. Context / motivation (mined for the thesis body)

- Background: U. Warsaw informatics BSc; strong CUDA, no prior graphics. Sought a CUDA-heavy challenge → Didyk/Condor;
  agreed to a **performance-engineering** take on DSYG ("make it faster"). Learned OptiX/graphics from zero (RTI1W +
  Yuksel lectures) while building — a legitimate explanation of the engineering scope/timeline.
- **The validation pivot (first autopsy):** Jorge supplied voxel-grid `.exr` references that could never be
  reproduced in Mitsuba. Rather than chase them, Kacper pivoted to *"run Mitsuba directly, run mine against it,
  quantify the gap."* This is the **origin and justification of the entire differential-validation methodology** —
  not an apology, a methodological narrative.
- Personal context (ADHD/perfectionism/full-time job) explains the *calendar* only → **excluded from the body**;
  at most one line in acknowledgements. The *technical* reasons (learning OptiX, the reference dead-end) stay.

## 4. Framing (LOCKED: ①)

**Build → Validate → Optimize** chronological spine, threaded with **two through-lines**:
(a) *differential validation* as the methodological backbone; (b) *negative results / "autopsies"* as a deliberate
contribution. (Alternatives considered: ② methodology-first, ③ pure perf case-study — both folded in as through-lines.)

## 5. Constraints

| Constraint | Value |
|---|---|
| Length | ~80pp (soft target — substance over length; 72 or 94 both fine) |
| Draft deadline | **~end of June 2026** to Piotr (EOM + 1–2 days slack OK) |
| Review | July: iterate w/ Piotr & Jorge + university upload lead time |
| Defense | **28 Aug 2026** |
| Effort | daily, part-time alongside a full-time job; experiments scheduled via Claude remote control |
| Operating point | **ALL FULL BLAST** — every *reported* number at the 3090's full clock (coordinate with admin "Prybicki"); dev-time 150 W numbers stay as record |
| Timing method | equal-quality (noise²·time) where throttle/variance bites; A/B-interleaved for fair timing |
| Writing surface | **in-repo LaTeX** (USI `memoir` template); compile locally |
| Open-source | **yes** — reproducibility note + repo link |

**Writing strategy:** *reuse-and-elevate, not greenfield.* The 31pp front-half + FINDINGS §8 hold most of the raw
material; new writing concentrates in Ch 4 (architecture) + Ch 7 (breadth/scaling/memory) + Ch 8.

## 6. Chapter skeleton (~80pp)

| # | Chapter | ~pp | Source / status |
|---|---------|----:|-----------------|
| 1 | **Introduction** | 5 | reuse + retarget existing intro (future→past tense) |
| 2 | **Background** | 12 | reuse + **extend**: add the volume rendering equation / free-flight / phase-fn (HG) / NEE / MIS / analytic erf optical-depth the draft lacks (PBRT-sourced) |
| 3 | **Related Work** | 8 | reuse + tighten (NeRF · 3DGS · DSYG · Mitsuba · volumetric MC) + **ADD the novelty's lineage: SDTracking / decomposition tracking + Stochastic Splats (sort-free)** |
| 4 | **Renderer: Architecture & Implementation** | 18 | **NEW** — the systems core, built around the **flagship novel architecture**: (N1) single-trace **anyhit-buffer collection** (backface-cull → entries only) with **analytic exits** (1 vs N traversals) + (N2) **ADT argmin** scatter sampling (sort-/march-/bisection-free; SDTracking Theorem 1). Prior-art positioning vs DSYG/Mitsuba marching + Stochastic Splats; the analytic-erf inversion that enables it. Plus GAS/IAS, data structures, megakernel design. Sources: code + `abstract_kindof.txt` + `optimizations.md` Big-O |
| 5 | **Validation Methodology & Correctness** | 14 | reuse-and-elevate FINDINGS §0–§8.14 — *differential-validation* through-line |
| 6 | **Performance Engineering & Optimization** | 18 | reuse-and-elevate §8.15–§8.38 + App A — wins + RIS + autopsies (four-mode) + memory |
| 7 | **Results** | 10 | reuse + **NEW breadth/scaling/memory/firefly runs** |
| 8 | **Conclusion & Future Work** | 5 | **NEW** (summary · §9 limitations · the original extensions as future work) |
| — | *Back matter* | — | Bibliography + **Appendix A** (A1 full investigation) + declaration of originality |

**Body ≈ 90pp → trims to ~80.** Structural choices: Background **before** Related Work (draft has them reversed,
with overlap — split & dedup); the old "Planned contributions" section becomes the real Ch 4–8.

## 7. Front-half plan (Ch 1–3: a *quarry*, not a foundation)

**Cut / compress:** BVH construction mechanics (median splits, depth heuristics, dragon figure) → assume BVH, 1
sentence + cite · the K-d/BSP/quadtree/octree taxonomy → 1 sentence · generic "applications of volume rendering /
radiance fields" bullet lists → crisp motivation · the duplicate Mitsuba paragraph · the rasterization
vertex→fragment→depth-buffer deep-dive → "rasterization approximates, ray tracing simulates" · NeRF internals
(layer counts, MipNeRF %-faster) → trim to context.

**Keep + tighten:** radiance-field framing · splatting→ray-tracing shift · the DSYG / GMM-for-volumetric-RT section
(the basis) · the flicker motivation · MC integration · GMM math · the rendering equation.

**Expand / add (the real gap, PBRT-sourced):** the **volume** rendering equation · transmittance · **free-flight
sampling** · **phase functions (HG)** · **NEE + MIS** · DSYG's **analytic erf optical-depth**. (Fix the Epanechnikov
formula error; resolve the SfM/abstract TODOs.)

## 8. Optimization triage — four justification modes

The test for running an ablation: **would the result teach the reader anything?** If not, it is *justified, not ablated.*

| Mode | When | Examples |
|---|---|---|
| **(M) Measured ablation** | genuine design choice, non-obvious outcome | RIS vs MIS (+K-sweep), RR-depth, adaptive (autopsy), denoiser, NEE/MIS, firefly clamp, shadow-transmittance, **all autopsies** |
| **(C) Complexity-argued** | naive/accidental baseline → obvious form; Big-O *is* the justification; report incidental speedup, **no regression** | per-bounce O(N)/O(n²) containment-scan fixes (§8.19/§8.23), BitVector/CompactSet O(1) sets, compact `HitBufferSoA` |
| **(S) Standard practice** | textbook; cite, don't measure | multi-streams, FMA/unroll, `float4` coalescing, pinned/async, fast-math, `tex2D`, GAS compaction + `PREFER_FAST_TRACE`, OptiX-IR |
| **(I) Infeasible to ablate** | no valid A/B possible; justify the non-experiment | analytic vs tessellated (icosphere) — abandoned pre-"massive-refactor", uninformative + infeasible |

**Ch 6 spine:** *"we measured what was informative, complexity-argued what was merely made-correct, cited the
standard, and said plainly what we couldn't test."*

> **The novel rendering architecture** — (N1) single-trace anyhit-buffer collection with analytic exits + (N2) ADT
> argmin scatter sampling (§2.1) — is the thesis's **headline contribution**, *not* a set of optimizations, so it is
> **not** a four-mode triage row. It is described & justified in **Ch 4** (prior-art positioning vs DSYG/Mitsuba
> marching + SDTracking Theorem 1 + Stochastic Splats), proven unbiased in **Ch 5**, and its de-facto perf A/B is the
> **cross-renderer comparison vs Mitsuba's segment-marching** (Ch 7). The complexity wins (1 vs N BVH traversals;
> O(N+A²)→O(N+A); sort O(H²)→0; bisection→0) are *supporting evidence*, not the framing.

**Inventory sources:** the FINDINGS §8 ledger + the recovered `optimizations.md` (≈36 early opts + Big-O tables) +
the historical `OPTIMIZATION_TODO.md` / `plans/*` (reference) + a current-code verification pass.

**Reconciliation flags (use FINDINGS verdicts, not the stale catalog's):**
- Adaptive sampling: catalog "done win" → **§8.30 net loss, default-off.**
- Exit-caching: catalog "done" → **§8.35 null** (confirm same code path when writing).
- Narrower index types (uint16): proposed → **reversed** (#62 widened to uint32 for spp ceiling).
- Sobol: "future, 2–4×" → **§8.20 no win, reverted.**

**Current-code verification (already done):** BitVector still exists but is **size-dispatched with `CompactSet`**
(>256); hit record is now **`HitBufferSoA`** (not the old 12-byte unified record); **"Welford"/"Morton" names gone**;
confirmed present — builtin **analytic sphere**, GAS **compaction** + `PREFER_FAST_TRACE`, **anyhit-only + backface-cull**,
full **fast-math** flag set, `tex2D` env, async transfers.

> **OPEN:** the full opt inventory awaits **Kacper's code-completeness pass** — the list above is the union of the
> docs + verification, but may be incomplete. Every catalogued opt is checked against current code before it enters
> the thesis (no claiming removed opts).

## 9. Experiment plan

**Three metric axes:** time / quality / **memory**. **Operating point:** all full-blast (see §5).

**Assets:** hero = **652-Gaussian cloud** · breadth = **Disney cloud (wdas8_gauss, 24.6k) / bunny (25.6k) / smoke
(16.4k)** (+ **embergen** optional 4th) · stress = synthetic **`stress_N`** · invariants = **furnace** + single/cluster.

**Selection rule:** the full *axes × assets × experiments* grid is the **upper bound (a menu, retained in the spec as
"design space considered"), not a run list.** Run each experiment only where its result is informative:
- **Ablations → one hero asset (cloud), time + quality** (ratios are what matter); memory axis only when the opt
  *is* a memory opt.
- **Multi-asset / memory / scaling axes live in the Results experiments**, where they are the point.
- **Two cross-asset spot-checks** (headline speedup on a 2nd asset; the generalization set) guard against
  cherry-picking.
- **Validation is scene-specific by construction**, quality axis only.

**Right-sized run list** (✦ = genuinely new compute; rest = reuse or full-blast re-run):

**Ch 5 — Validation** (quality vs Mitsuba, ladder scenes; all reuse): furnace energy (§8.1) · absorption ladder
single→overlap→cloud (§2–7) · scattering ladder vs Mitsuba-analog (§8.2–8.4) · features env/HDR/HG/MIS/RGB
(§8.6–8.14) · money shot (§8.11).

**Ch 6 — Optimization** (hero cloud, time + quality): RR-depth sweep (§8.33) · RIS-vs-MIS + K-sweep, env vs flat
(§8.37) · adaptive on/off (§8.30) · denoiser spp-curve (§8.22) · fast_erf (§8.21) · the autopsies (§8.34 / A1 /
alias / exit-cache / density / Sobol) · ncu bottleneck profile (§8.28/8.29). **+ memory:** GAS compaction
before/after ✦ · wavefront RayState footprint ✦ (quantifies the §8.34 autopsy).

**Ch 7 — Results:**
| | Experiment | Asset(s) | Axes | Status |
|---|---|---|---|---|
| R1 | final perf vs Mitsuba (equal-quality) | hero + 1 breadth spot-check | time+quality | extend §8.17 |
| R2 ⭐ | **generalization** (visual + RMSE vs Mitsuba/volprim_prb) | **all 4 assets** | quality+time+memory | **NEW** ✦ |
| R3 | scaling sweeps | `stress_N` + 4 assets as points | **time & memory vs N** | partly NEW ✦ |
| R4 | showcase finals (money-shots, denoiser, firefly) | hero + 1–2 breadth | quality (renders) | NEW ✦ |
| R5 | firefly 3-way (ours / Mitsuba-analog / Mitsuba-MIS) | meadow env scene | quality (max, p99.9/p99.99, firefly-pixel count, crops) | **NEW** ✦ |
| R6 | memory footprint breakdown (GAS/primitives/env/film/denoiser + peak VRAM) | Disney cloud | memory | NEW ✦ |
| R7 | vs-Mitsuba peak VRAM | 1–2 scenes | memory | NEW ✦ |

**Methodology notes:** equal-quality (noise²·time) for timing; furnace as the reference-free bias gate; vs-Mitsuba
always same operating point; ncu for occupancy/latency profiling; memory via `cudaMemGetInfo` high-water + OptiX AS
size queries + RAII-buffer accounting. R6's memory-vs-N curve realizes the `structure.md` "Density (???)" item.

## 10. Autopsy write-up approach (the negative-results contribution)

Each dead end written as: **hypothesis → cheap kill-test → measurement → why it failed → transferable lesson.**
The ledger (wavefront §8.34, A1 §8.27/App A, adaptive §8.30, density-cull §8.31, track-length §8.32, exit-cache
§8.35, alias §8.36, Sobol §8.20, footprint §8.29, path-guiding §8.38 deferred) is framed as a **deliberate
scientific contribution** ("what doesn't work on this algorithm class, and why, grounded in profiling"), not filler.
The structural lesson — *this algorithm is megakernel-shaped; any design moving per-ray state to global memory is
fatal* — is the unifying thread, made quantitative by the wavefront memory plot (R-mem).

## 11. Bibliography plan

Cite the **seminal source for each technique used** (standard practice; the bar is "can explain & used it," which
holds — implemented + validated). Only cite what's defensible at the viva; no padding. Generate BibTeX **and verify
each entry's author/year/venue** at population time.

| Technique | Canonical citation |
|---|---|
| Rendering equation | Kajiya 1986 |
| MIS, balance/power heuristic | Veach & Guibas 1995 (+ Veach thesis 1997) |
| Russian roulette | Arvo & Kirk 1990 |
| Radiative transfer / VRE | Chandrasekhar 1960; Novák et al. 2018 *(cited)* |
| Henyey–Greenstein phase fn | Henyey & Greenstein 1941 |
| Delta / Woodcock tracking | Woodcock 1965 |
| **Analog decomposition tracking (the novel core, §2.1)** | **Kutz et al. 2017** — `SDTracking.pdf` (Theorem 1) |
| Sorting-free splatting (spirit / related) | Stochastic Splats — `stoch-splats.pdf` (verify authors/year) |
| RIS | Talbot, Cline & Egbert 2005 |
| Weighted reservoir sampling | Chao 1982 / Vitter 1985 |
| ReSTIR lineage | Bitterli et al. 2020 |
| Sobol / Owen scrambling | Sobol′ 1967; Owen 1998 |
| BVH + SAH | MacDonald & Booth 1990 (replaces lecture-slides cite) |
| Shader Execution Reordering (noted N/A on Ampere) | NVIDIA SER whitepaper 2022 |
| PBRT (Background backbone) | Pharr, Jakob & Humphreys, 4th ed. 2023 |
| WDAS Disney cloud asset | Walt Disney Animation Studios 2017 |
| Stanford bunny asset | Turk & Levoy 1994 |

## 12. Logistics & assets

- **PBRT** symlinked at `references/pbr-book` (gitignored; copyrighted 2.2 GB mirror of 3e+4e) — the Background math source.
- **Plot pipeline:** none exists yet → establish a small matplotlib step (via `tools/refs/.venv`) turning experiment
  CSVs into publication-quality figures.
- **Repo layout:** thesis LaTeX in-repo (extract from the zip into a working dir); this spec at
  `docs/superpowers/specs/`; FINDINGS / OPTIMIZATION_FRONTIER / ASSET_TAXONOMY already under `thesis/`.

## 13. Open threads / next actions

1. **Kacper:** code-completeness pass on the §8 optimization inventory (flag anything missed/removed).
2. **Kacper + Prybicki:** schedule the full-blast measurement window(s).
3. **Kacper:** confirm the precise **novelty boundary for BOTH halves** — (N1) single-trace anyhit-buffer +
   analytic-exit collection, and (N2) ADT free-flight-argmin scatter sampling: first to apply to ray-traced Gaussian
   kernel-mixture volumes? what did Jorge *propose* vs *not realize*? Plus the final **inverse-CDF formulation**
   (free-flight `−log(1−χ)` per SDTracking Theorem 1 vs raw density CDF — `CLAUDE.md` open question; the validated
   state should resolve it; verify in code when writing Ch 4).
4. **Then:** `writing-plans` → execution plan (chapter-by-chapter writing tasks + the ✦ experiment runs + the
   full-blast re-measurement campaign).

> Commit of this spec is **user-gated** per `CLAUDE.md` (no auto-commit).
