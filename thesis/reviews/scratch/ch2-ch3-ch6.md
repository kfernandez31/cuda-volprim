# Examiner review — Ch 2 (Background), Ch 3 (Related Work), Ch 6 (Optimization)

Date: 2026-06-15. Scope: `chapters/02-background.tex`, `chapters/03-related-work.tex`,
`chapters/06-optimization.tex` (FRESH scrutiny — prior review excluded Ch 6 as WIP). Cross-checked
against `results/campaign/*` records, `thesis/FINDINGS.md` (§8.5/8.27/8.29/8.30/8.32/8.34/8.37),
`src/thesis/host/app/config.cpp`, and the reference integrator `~/jorge/volumetric_primitives/volprim`.
READ-ONLY pass; no edits made.

Severity buckets: **Blocking** (examiner would call overclaim/contradiction or a false code claim) /
**Should-fix** (misleading or imprecise vs the record) / **Polish**.

---

## BLOCKING

### B1 — Ch6:15 `sec:bottleneck` — false "profiling first" claim (regression: handoff said REMOVE)
> "Optimisation began with profiling, not guessing."

This inverts the actual chronology. FINDINGS §8.5 (the *first* optimisation analysis) explicitly
labels its levers **"never profiled"** (FINDINGS:416: "Levers (correctness-preserving, never
profiled)"); the §8.5 root-cause (per-step RB / collision-vs-track-length) was reached by *reasoning
from the reference code*, and the first ncu profile only appears at §8.28 — much later. The prior
handoff flagged this exact sentence for removal; it survived. It also self-contradicts the chapter's
own A1 autopsy (lines 339–360), which is a *reasoning-first* diagnosis, and the four-modes taxonomy
that explicitly admits non-measured justifications.
**Fix:** delete the sentence, or replace with something true, e.g. "The bottleneck was localised by
NSight Compute" (state the profile as evidence, not as the genesis of the optimisation work). Do not
claim profiling led the effort.

### B2 — Ch6:152 `sec:ris` (+ Ch4 §4.7) — "shipped behind a runtime flag" is FALSE in the binary
> "It is therefore shipped behind a runtime flag (default off, plain MIS; $K=6$ the measured default)"

The `--ris` / `--ris-candidates` CLI options are **not registered**. `Config` carries `use_ris_` and
`ris_num_candidates_` (`include/thesis/host/app/config.h:45-46`) and `renderer.cpp:217-218` plumbs them
to the device, but `src/thesis/host/app/config.cpp` (the CLI11 parser, lines 14–51) registers no
`--ris` option — verified by grep across all of `src/` + `include/`: zero `add_flag("--ris"...)` /
`add_option("--ris-candidates"...)` anywhere. So RIS is **unreachable at runtime** (stuck at the
`false`/`6` defaults); the headline 1.48× win cannot be turned on by a user of the shipped binary.
FINDINGS §8.37 (line 1517) *claims* the flag ("Gated behind a runtime flag `--ris` ... default OFF"),
so the prose faithfully reports the *intent* — but the code regressed. This is the prior review's
act-first #3, still unfixed.
**Fix:** the CODE, not the prose — add to `config.cpp` `render_group`:
`->add_flag("--ris", config.use_ris_, ...)` and
`->add_option("--ris-candidates", config.ris_num_candidates_, ...)`. (Out of this review's edit scope;
flagged for the author.) Until then, every "runtime flag" / "default off, opt-in" claim in Ch4 and Ch6
is false in the binary.

---

## SHOULD-FIX

### S1 — Ch6:202 `tab:complexity`, row "Boundary sort (scatter)" — the reference does NOT sort
> `Boundary sort (scatter)  &  $O(H^2)$ / $O(H\log^2 H)$  &  eliminated`

This misattributes a *sort* to the reference. Verified against the reference integrator
(`~/jorge/volumetric_primitives/volprim/integrators/common.py:482-510`): the next boundary is chosen
by an **inner linear running-min scan** (`is_closer = (v.t > seg_t0) & (v.t < vertex.t)`, an O(H)
scan repeated O(H) times → O(H²)), and `volprim_prb.py` walks an **unsorted** primitive list
(`primitives.value(it)`). No `sort`/`argsort`/`sorted` exists anywhere in the reference integrators
(grep: zero hits). The `$O(H^2)$` figure is correct (repeated running-min); the word "sort" and the
`$O(H\log^2 H)$` alternative both falsely imply a sorting algorithm. Load-bearing item #5 said this row
"was to be reworded" — it was not.
**Fix:** rename the row to "Boundary selection (scatter)" and drop the `$O(H\log^2 H)$`; e.g.
`Boundary selection (scatter) & $O(H^2)$ running-min & eliminated`.

### S2 — Ch4:7 + Ch6:193 — "sorted sequence" / "per-bounce sort" misattributed to the reference
- Ch4:7: "marches the ray segment by segment through the **sorted sequence** of primitive boundaries"
- Ch6:193: "removes the reference's per-bounce **sort** and root-find outright"

Same root cause as S1: the reference never materialises a sorted boundary sequence; it selects the
next boundary by running-min (common.py:495). The boundaries are *visited in increasing t* but via
repeated minimum-selection, not a sort. (NB: Ch3 was correctly fixed — Ch3:54-59 now says "selecting
the next boundary by a running minimum", no "sorted sequence". The fix didn't propagate to Ch4/Ch6.)
**Fix:** Ch4:7 → "marches the ray segment by segment, selecting each next boundary by a running
minimum"; Ch6:193 → "removes the reference's per-bounce boundary march and root-find". (The "sort-free"
*architecture label* — Ch4:166, Ch6 passim, abstract — stays fine: it describes the new design's
property, not a claim that the reference sorts.)

### S3 — Ch6:339-340 `sec:autopsies` adaptive — "~2× slower" headlines the worst operating point only
> "Per-pixel convergence stopping was a net loss ($\sim\!2\times$ slower at equal quality, plus
> firefly bias) on the high-variance cloud"

The "~2×" is defensible only at the §8.30 operating point that *raised the cap to chase higher
quality* (max2048, thr0.01: 599 s vs ~304 s uniform-1024). The *campaign re-run* — the more recent,
controlled, equal-quality measurement (`results/campaign/g6.md`) — found adaptive only **0.9% slower**
(23.94 vs 23.77 s, identical RMSE 0.1706) at the production operating point, because no pixel meets the
1% stop test and adaptive degenerates to uniform + Welford overhead. So the "~2×" overstates the
campaign's own finding by ~2×.
**Fix:** scope it: "a net loss (0.9% slower at the production point, up to ~2× when the sample cap is
raised to chase higher quality; degenerates to uniform on this frame-filling high-variance cloud),
plus firefly bias." The firefly bias is justified (§8.30 Δmean −0.0003…−0.0005).

### S4 — Ch6:347-348 A1 autopsy — "corrected its cause" understates that the §8.5 premise was RIGHT
> "A single-Gaussian depth sweep ... confirmed a real gap but **corrected its cause**"

The estimator-class explanation (track-length vs collision) is not a *correction of* the §8.5 premise —
it is the *mechanism of* it. FINDINGS §8.27 is explicit (FINDINGS:1011-1013): "The ORIGINAL §8.5
premise (Mitsuba folds analytic transmittance every bounce; CUDA only at bounce 0) was **RIGHT**";
what was wrong was the *intermediate* A1_INVESTIGATION dismissal ("both estimators are analog, no
difference"). Ch6 doesn't reproduce that dismissal, so it doesn't hard-invert the premise (it is better
than the appendix it replaced, which the prior review caught saying "the hypothesis ... was wrong") —
but "corrected its cause" still reads as if the original hypothesis was off, when it was directionally
right. NB this is *delicate*: do not flip it to "confirmed the hypothesis" without naming that an
intermediate step had dismissed it, or you lose the honest arc.
**Fix:** "confirmed a real gap and **identified its mechanism** (an intermediate analysis had wrongly
dismissed the effect; the sweep overturned that and confirmed the original reading): the two are
different estimator classes." Keep the rest.

### S5 — Ch6:329-332 cap-free streaming — numbers correct, but the source record is uncited/off-repo
The figures (+22% tornado, +16% explosion, +12% cloud, −2% bunny; bit-exact; the χ≈0 active-set
exclusion bug; "OptiX does not report hits at exactly t = t_max") all check out against
`memory/project_capfree_streaming.md` (and `capfree_b_gate.md`, referenced there) — but those are NOT
in `results/campaign/` or `FINDINGS.md` proper, so the chapter's strongest negative result has no
in-repo citation an examiner can follow.
**Fix:** add a `results/campaign/capfree.md` summary (or fold the gate doc into `FINDINGS §8.x`) so the
+22/+16/+12/−2 and the two outlived bugs are citable. (Content is correct; this is provenance only.)

---

## POLISH

### P1 — Ch6:18-19 "82–98% cache-resident" conflates L1 and L2
FINDINGS §8.29 (FINDINGS:672): "L1 hit is already **82–84%** and **L2 98.7%**". The "82–98%" range
silently spans two cache levels. Defensible but loose. **Fix:** "82–84% L1- and 98.7% L2-resident".

### P2 — Ch6 §6.8 `sec:icosphere` — scattering-path validation (0.073%) omitted
`tab:icosphere` + prose report only the *absorption* RMSE and the 0.16% absorption brightening. The
record (`icosphere_port.md`, "Scattering-path gate — PASS") additionally validated the port through the
*full scattering pipeline* (argmin + NEE/MIS shadow rays): converged means agree to +2.34e-4 on a 0.321
mean (**0.073%**). This strengthens "the port is correct" (not just absorption). **Fix (optional):** one
sentence in §6.8 noting the scattering gate passes at 0.073%.

### P3 — Ch6:18 "21–31% occupancy (asset-dependent)" vs the two profiled points
The two measured points are 31.2% (cloud) and 20.9% (bunny) — so "21–31%" rounds 20.9→21. Fine, but the
"asset-dependent" range is built from exactly two assets; an examiner may read "21–31%" as a measured
spread. **Fix (optional):** "20.9%–31.2% across the two profiled workloads".

---

## Ch6 AUTOPSY-JUSTIFICATION TABLE

| Autopsy (Ch6 §sec:autopsies) | Stated lesson | Record | Numbers match? | Justified? |
|---|---|---|---|---|
| Wavefront refactor | per-ray state in global memory is fatal; megakernel-shaped; rules out wavefront/ray-sort/neural-cache | FINDINGS §8.34 + g6.md | 100–1400× ✓, 352 B/ray ✓, L2 cliff ✓ | **YES** |
| Megakernel footprint reduction | data already cache-resident; cost is load latency not footprint | FINDINGS §8.29 | L1 82-84% / L2 98.7% ✓, local 1.2% of traffic ✓ | **YES** (P1: cache-level conflation) |
| Cap-free streaming scatter | cleaner memory shape buys nothing when registers (not local mem) bound occupancy | capfree memory/gate doc | +22/+16/+12/−2% ✓, bit-exact ✓, −8% L2/+6% instr ✓, 2 outlived bugs ✓ | **YES** (S5: off-repo source) |
| Adaptive sampling | net loss; degenerates to uniform on high-variance cloud + firefly bias | FINDINGS §8.30 + g6.md | "~2×" only at raised-cap point; campaign = 0.9% | **PARTIAL** (S3: 2× overstates campaign's own 0.9%) |
| A1 / per-step Rao–Blackwell | characterised trade-off (flat regime only), not a missed opt; hybrid open only if flat gap matters | FINDINGS §8.27 + §8.32 | 5× noisier ✓, 1.06× MIS-off ✓, estimator-class framing ✓ | **YES on substance** (S4: "corrected its cause" understates that §8.5 premise was RIGHT) |
| Icosphere (sec:icosphere) | refutes the hypothesis; analytic pays 1.17–1.58× for exactness; kept for correctness | icosphere_port.md + .csv | 1.17–1.58× ✓, RMSE 4e-2→2.5e-3→reversal ✓, N=2 0.16% ✓ | **YES** — honestly reported as a *refuted hypothesis*, not a win (Ch6:224,263,"refutes") |
| RIS (sec:ris) | 1.48× on meadow, K=6, scene-dependent (helps peaky, hurts flat) | ris_ksweep.md + §8.37 | 1.481 meadow ✓, flat ≤0.55× ✓, studio 1.45× ✓, K-plateau 4–6 ✓, 0.16/0.17 RMSE ✓ | **YES** — but B2: the "runtime flag" claim is false in the binary |

Three smaller autopsy one-liners (Ch6:361-366) — density-culling (§8.31), track-length combine
(§8.32), exit-caching (§8.35), env-IS alias (§8.36), Owen-Sobol (§8.20) — each match their FINDINGS
§ verdicts; "track-length combine — unnecessary, env transmittance already analytic" correctly mirrors
§8.32's "dead end, confirmed three ways". Path guiding (§8.38) "deferred not failed" matches §8.38.

**On the track-length hybrid "still open?" check (load-bearing #1):** Ch6:359-360 scopes the hybrid to
"should the flat gap ever matter" — i.e. open *only for the non-target flat regime*. This is consistent
with §8.27:1018-1020 (calls it "the open thesis-worthy direction if the flat-env gap is ever worth
closing") and does NOT present it as open beyond flat-env. §8.32 is mildly stronger ("dead end ...
confirmed three ways" — i.e. *unnecessary*, prize already captured), but Ch6 siding with §8.27's
flat-scoped "open" wording is defensible. The prior review's worry (the *appendix* presenting it as
unconditionally open) does NOT survive into the folded-in Ch6. **Not a finding** — flagged as verified.

---

## MOST LIKELY DEFENSE QUESTION — and is it pre-empted?

**Q (the icosphere result, turned against the thesis):** *"Your own Ch 6 A/B shows the tessellated
icosphere is faster at every tessellation level — the analytic sphere costs you 1.17–1.58× in frame
time. The reference (DSYG) already concluded tessellation wins by 4.96×. So your central architectural
choice is the slower one. Why is your renderer's headline 'faster than Mitsuba at equal quality' (Ch1/
Ch7) not simply an artifact of other optimisations masking a geometry choice you got wrong?"*

**Pre-empted? — YES, well.** Ch6 §6.8 is unusually honest here: it states the hypothesis is **refuted**
(lines 224, 263, "refutes the hypothesis under which the analytic sphere was chosen"), quantifies the
1.17–1.58× cost, *names the reference's mechanism* (RT-core triangles vs software sphere intersection,
line 265-266), and defends the choice on the **orthogonal correctness axis**: the analytic shell is
exact, the N=2 faceting bias (0.16% brightening) is an order of magnitude above the 10⁻⁴ energy
agreement the Ch 5 validation gates hold the renderer to (lines 283-287), and the sliver reversal at
N=3 (fig:icosphere-sliver) shows tessellation also *caps out on accuracy*. So the framing is "a genuine
accuracy↔performance trade, analytic kept for correctness at a now-quantified price" — not a dominance
claim. The committee-grade move would be to **add one sentence** tying it back to the headline: the
equal-quality win over Mitsuba is *despite* paying the geometry tax, and switching to N=2 tessellation
would widen the margin further if validation-grade exactness were relaxed (the chapter has the data —
N=2 is "the right shell where raw throughput matters", line 289 — but doesn't connect it to the Ch7
headline). That connection would fully disarm the question.

**Runner-up Q:** *"You claim RIS is shipped behind a runtime flag, but `--ris` isn't in your CLI —
isn't the 1.48× win unreachable in the actual binary?"* — **NOT pre-empted** (B2). This is a concrete,
verifiable, embarrassing gotcha. Fix the code before the defense.

---

## VERIFIED-CORRECT INVENTORY (no action)

**Ch 2 — all prior-review math MINORs FIXED (regression-check passed):**
- `02:31-32` — "error scales as $1/\sqrt{N}$, halving the noise costs four times the samples": now
  correctly $1/\sqrt{N}$ (was "$\sqrt{N}$"). FIXED.
- `02:25` — variance $O(1/N)$ now gated "(when $f/p$ has finite variance)". FIXED.
- `02:87-88` — delta tracking now estimates **transmittance $T = e^{-\tau}$** stochastically, not τ.
  FIXED.
- `02:139-140` — balance heuristic "within a small additive term of the **best achievable combination**
  of the given strategies": Veach-accurate (not "best individual strategy"). FIXED.
- `02:150` — mixture $\sum_{k=1}^{N}$: uses $N$, no longer collides with RIS $K$. FIXED.
- HG normalisation, Beer–Lambert, free-flight inversion τ(t)=−ln(1−ξ), erf closed form, OptiX GAS/IAS
  description, unnormalised-Gaussian-with-$w_k$ convention: all correct.

**Ch 3 — all prior-review "the reference sorts" claims FIXED:**
- `03:54-59` — now "selecting the next boundary by a **running minimum** ... and solving ... with a
  root-find" — accurate (verified vs volprim common.py:495). No "sorted sequence" survives. FIXED.
- `03:74-77` — "no marching and no root-finding": "sorting" dropped (was the prior :76-77 residual).
  FIXED.
- All other Ch3 "sort" mentions correctly refer to 3DGS rasterisation's depth sort or the "sort-free"
  framing. DSYG fairly represented (relightable, multiple-scattering, closed-form optical depth, BVH).
  Positioning adequate: NeRF→3DGS lineage, delta/decomposition tracking + SDTracking Theorem-1 min-of-
  free-flights, StochasticSplats (correctly positioned as a *rasteriser*, complementary), Mitsuba/
  Dr.Jit as validation reference, OpenVDB/NanoVDB voxel baseline. No committee-expected comparison is
  conspicuously missing.

**DSYG citation — FIXED:** `refs.bib:12-19` is now `@article{DSYG}`, `journal={ACM Transactions on
Graphics}`, `year={2025}`, `doi={10.1145/3711853}` — the published TOG version, not arXiv `@misc`.

**Ch 6 — quantitative cross-checks that PASS:**
- Roofline figure (fig:roofline) + §6.1 ncu narrative: cloud 31.2% occ / 45.9% SM / 16.5% DRAM / 6.95
  lanes / ≈1.0 TFLOP/s / AI 6.8; bunny 20.9% occ / 70% no-eligible / 1.8% DRAM / 5.42 lanes / ≈0.4
  TFLOP/s / AI 24 — **all match `ncu_summary.md` + `roofline.csv` exactly.** "5.4–6.9 of 32 lanes"
  spans bunny→cloud. Latency-bound / divergence-dominated / non-saturation framing is faithful.
- RR-depth (fig:rr-depth, tab:wins): basin 8–12 (≤1.2% spread), min at depth 12, +4.7% over depth 5,
  dev-era 11% revised down — matches `rr_depth.md` re-anchored table exactly.
- RIS K-sweep (fig:ris-ksweep): 1.481 meadow / 1.45 studio / ≤0.55× flat, plateau K=4–6, saturates
  studio→meadow, K=1 anchor 1.19× — matches `ris_ksweep.md` exactly.
- RIS noise (fig:ris-noise): 0.16 vs 0.17 RMSE consistent with the banked k (√(1.6975/64)=0.163 vs
  √(1.9842/64)=0.176). The −21% time / −14% noise decomposition matches ris_ksweep.md.
- Icosphere (tab:icosphere): 1.00/0.63/0.72/0.77/0.85× and RMSE column match `icosphere.csv`
  row-for-row; N=3 sliver reversal (~3150 px, max |Δ|≈0.25) matches the figure record.
- Per-asset overlap (tab:overlap): the seven rows + the "fine fit overlaps less than dense fit despite
  32× more primitives" point match the cap-estimator framing.
- SER (Ada-only, unavailable on Ampere 3090): correctly cited as a real-but-unavailable lever, twice.
- Mode taxonomy (tab:four-modes): the (I) row correctly emptied, pointing to sec:icosphere (the choice
  that turned out measurable) — consistent with the icosphere reclassification in icosphere_port.md.
