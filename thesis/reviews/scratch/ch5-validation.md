# Ch5 (Validation) — logic / attribution / sufficiency review (2026-06-15)

**Scope:** validation LOGIC and SUFFICIENCY of evidence; every attribution checked against the
records. No rendering (separate agent owns GPU repro). READ-ONLY.

**Target:** `thesis/latex/chapters/05-validation.tex` (current line numbers; the prior review's
numbers drifted — the chapter was substantially rewritten and is now ~285 lines).

**Records cross-checked:** `results/campaign/{g2_ladder,voxel_gt,voxgrid_DECISION,g1_analog,
g1_analog_final,advol_gt,g1_headline}.md`; `thesis/FINDINGS.md` §§7, 8.0–8.4, 8.11, 8.13, 8.14;
`scripts/plots/scattering_convergence.py`; `test/scenes/single_gaussian.cpp`;
`08-conclusion.tex`. Build verified clean.

**Bottom line up front:** the chapter is in strong shape. Every CRITICAL/MAJOR item from the prior
review (`2026-06-10-full-review-ex-ch6.md` §5: C1, C2, M1, M2, M3, M4) is **FIXED** in the current
text. The overlap-scatter residual is now honestly disclosed in Ch5 *and* carried into Ch8
limitations. The scattering-unbiasedness evidence is sufficient for the (correctly magnitude-bounded)
claim it makes. No Blocking items. A handful of Should-fix precision items remain.

---

## EXPLICIT VERDICTS (the three the task demands)

1. **Is the scattering-unbiasedness evidence SUFFICIENT?** — **YES, for the claim as now worded.**
   The claim is correctly scoped to "unbiased to within ${\sim}10^{-4}$" (`05:221-224`), not absolute
   unbiasedness. The evidence has three independent legs: (a) the **furnace invariant** — a
   reference-free analytic energy-conservation check that *does* exercise the full argmin scatter / NEE
   / multi-bounce machinery (FINDINGS §8.1: CUDA mean 1.00001, SEM 6e-5, "no detectable energy bias");
   (b) the **Mitsuba-analog differential** with a properly-justified ground truth (Mitsuba-NEE is itself
   +6.5% energy-biased, §8.1, so the unbiased analog mode is the only valid reference — this is a
   genuine strength, not a hobble); (c) **mean convergence** to the same value (`fig:scattering-ladder`,
   0.4% agreement, both unbiased). Crucially the chapter does **not** overclaim absolute unbiasedness —
   it discloses the one convergence-stable residual and bounds it. That is exactly the right
   epistemic posture: a sampler-bias of order 10^-4 in heavy overlap is *admitted*, and everything
   else is shown bias-free to the noise floor. SUFFICIENT.

2. **Is the overlap-scatter residual NOW honestly disclosed?** — **YES, in both required places.**
   - Ch5 `05:215-224`: "leave one small, convergence-stable residual in the *overlap* region — a
     $+2\times10^{-4}$ core difference on the cluster (about six standard errors), echoed as
     ${+}1\times10^{-4}$ in the densest part of the cloud. It is below $0.2\%$ of the peak and confined
     to vertices where many primitives overlap; its likely cause is the argmin free-flight distribution
     under heavy overlap … recorded as an open item." This matches FINDINGS §8.3 (+0.0002 core, ~6 SEM,
     convergence-stable, ≤0.16% peak, candidate = argmin under overlap, OPEN) and §8.4 (+1e-4 at 3.3σ
     in the densest core) **exactly**.
   - Ch8 `08-conclusion.tex:54-59`: limitations bullet "a ${+}2\times10^{-4}$ difference at
     heavily-overlapped scatter vertices (candidate cause: the argmin free-flight distribution under
     overlap, \Cref{ch:validation})". Carried into the thesis-wide limitations. **C1 FIXED.**
   The disclosure STRENGTHENS the chapter (as the prior review predicted) — `05:224`: "That the
   methodology resolves a $10^{-4}$ effect at all is itself a measure of its sensitivity."

3. **Is "${\sim}10^{-4}$" SUPPORTED and honestly magnitude-bounded?** — **YES.** It is sourced and
   bounded from both sides. Lower-bounding the resolution: the cloud cross-RMSE-vs-spp fit gives a
   measured systematic floor (§8.4: global systematic −1.6e-5 ± 8e-6, "essentially zero ~5 decimal
   places"; direct 16-seed measurement, not extrapolation). Upper-bounding the residual: the cluster
   residual is +2e-4 (≤0.16% peak), the cloud echo +1e-4 (3.3σ, densest core only). The chapter's
   "unbiased to within ${\sim}10^{-4}$" sits correctly between the resolved-zero global floor and the
   bounded overlap residual. The qualifier "a grossly wrong free-flight distribution would shift the
   converged mean far more, and it does not" (`05:222-223`) is the honest logical form — it claims
   *bounded* bias, not *zero* bias.

---

## BLOCKING

**None.** No item rises to "an examiner would call this overclaiming or a contradiction." The single
most dangerous item (C1, the undisclosed residual cited as proof) is fully repaired.

---

## SHOULD-FIX

### S1 — Differential-validation principle: stated correctly, but the cloud-rung wording slightly
   over-credits the analytic adjudication (residual of prior M1/M3, mostly fixed).
- **`05:182-189`** (the cloud paragraph + `fig:absorption-ladder` discussion). Current text is
  **honest and now correct**: "It matches the reference across all $24$ camera views---against
  Mitsuba, with a float64 brute-force analytic transmittance adjudicating the edge band on a
  representative view" and the ladder summary "full cloud (reference + analytic spot-check)". This is
  exactly what FINDINGS §7 records (24-view = CUDA vs `volprim_prb`; the float64 BF reference
  adjudicated only the single-view edge band, §7 "RESOLVED 2026-06-02"). **M1 FIXED, M3 FIXED.**
- The one residual nit: `fig:absorption-ladder` **caption** (`05:194-196`) says "The single Gaussian
  is held to its closed-form transmittance; the pair and cloud are compared against Mitsuba with
  analytic spot-checks." Quote: *"the pair and cloud are compared against Mitsuba with analytic
  spot-checks."* This is now accurate (no longer claims per-scene RMSE vs closed-form for all three).
  **No change strictly required** — flagging only because "analytic spot-checks" (plural) for the pair
  is generous: the *pair* rung's analytic check is the collinear-stack closed form (`exp(-min(cap,K)/K·τ₀)`,
  §6) which is a degenerate stress variant, not the plotted overlapping-pair scene. Optional tightening:
  "…compared against Mitsuba, with closed-form checks on the single Gaussian and the collinear stress
  variant." Low priority; the body text (`05:188`) already says "overlapping primitives (analytic +
  reference)" which is defensible because §4 of FINDINGS does record analytic agreement for the n5/two
  clusters (mean diff −7.8e-7 / +2.1e-6).

### S2 — `sec:voxel-gt` is INCLUDED and framed correctly, but `voxgrid_DECISION.md` flatly
   contradicts the chapter (stale decision file — not a thesis defect, but will mislead the next reader).
- **`results/campaign/voxgrid_DECISION.md:1-6`**: *"do NOT include any voxel-grid comparison, and no
  mention of AdVol, in the thesis … The thesis validation rests instead on the analytic closed-form
  ground truth, the Mitsuba-volprim differential comparison, and the energy-conserving furnace
  invariant."* But Ch5 §2.1 `sec:voxel-gt` (`05:82-121`) **does** include the absorption voxel-grid
  cross-check with `fig:voxel-gt`. The task prompt confirms this is the *intended* state (absorption
  voxel-GT IN, scattering voxel-GT OUT), so **the chapter is correct and the DECISION file is stale**.
  Fix: the DECISION file is a record, not thesis source — out of this review's edit scope — but it
  should be reconciled (it currently reads as "exclude *all* voxel-grid", whereas the actual decision
  was "include absorption, drop scattering"). Flagging so it's not mistaken for a live contradiction.
- The chapter's framing of the scattering DROP is **correct and well-sourced** (`05:106-109`):
  "Scattering is deliberately not cross-checked this way: a dense-grid scattering reference for this
  high-dynamic-range medium is either block-biased, with a coarse local majorant, or firefly-limited,
  with an exact global one." This matches `voxgrid_DECISION.md:16-31` (the clean/unbiased/tractable
  trilemma for the HDR cloud) precisely. The numbers in `05:100-104` (128³→600³: RMSE 0.072→0.057,
  mean 0.394→0.401 toward analytic 0.416; median pixel diff zero) match `voxel_gt.md:90-106` exactly.
  **No change to the chapter.** The remaining scattering evidence (furnace + Mitsuba-analog differential
  + mean convergence) is judged ADEQUATE — see Verdict 1.

### S3 — `fig:showcase` "${\approx}9$ s" / "0.321 versus 0.321": supported, but reconcile against the
   superseded §8.11 "+0.9%" so a transcriber doesn't trip.
- **`05:280-282`** (`fig:showcase` caption): "each a single \num{64}-spp frame at matched render time
  ($\approx\SI{9}{\second}$). The two agree on the converged image (means \num{0.321} versus
  \num{0.321})." Both numbers are sourced to the **16-seed converged** campaign data:
  `g1_headline.md:11-12` (ours-MIS 0.3214 / Mitsuba-analog 0.3201, both ~9 s) → 0.3214/0.3201 rounds to
  0.321/0.321 and the gap is 0.4% (= `fig:scattering-ladder`). **Supported.** Caveat: FINDINGS §8.11
  (`570`) reports a *different* number, "+0.9%" (CUDA-MIS 0.3215 vs Mitsuba-analog 0.3242), from an
  earlier 256-spp/3-seed render. The 0.4% (16-seed) supersedes the 0.9% (3-seed) — the analog
  reference's firefly tail biases the small-sample mean upward, which §8.11 itself notes ("+0.9% is
  Mitsuba-analog's unconverged firefly tail"). **The chapter is right to use 0.4%.** No chapter change;
  flagging only that §8.11's 0.9% and the figures' 0.4% must not be read as a contradiction (they are
  the same quantity at different convergence).

### S4 — `05:166` single-Gaussian absorption precision: aligned with the record, prior MINOR resolved.
- **`05:163-166`**: "The renderer matches this analytic value to about $10^{-5}$---the pixel-filter
  jitter floor … the small residual that remains is pixel-filter variance, not bias." This matches
  FINDINGS §7 (CUDA matches truncated truth to 2e-5 analytic; the ~2e-3 edge was box-AA jitter →0 as
  1/√spp). The prior review's MINOR ("matches to within floating-point tolerance overstates") is
  **FIXED** — the text now says 10^-5 jitter floor, not "floating-point tolerance." Good.

---

## POLISH

### P1 — Feature-validation "three small residuals" list: accurate; the low-σ residual is now disclosed.
- **`05:246-249`**: coloured-albedo "small channel-dependent residual … together with the
  overlap-region scattering residual … and a low-density brightness systematic, it is one of three
  small residuals documented as known limitations." Cross-check: matches Ch8 `08-conclusion.tex:54-59`
  which lists exactly these three (overlap +2e-4, low-density +1.8e-4, coloured-albedo). The
  low-density systematic (§8.13: +1.8e-4 at 21σ, low-σ interior, OPEN) — flagged UNDISCLOSED by the
  prior review's M2 — is now **disclosed** thesis-wide. **M2 (disclosure half) FIXED.** Coloured-albedo
  residual matches §8.14 (sub-percent, MIS config). Good.

### P2 — `05:242-244` HG scope: now correctly "forward scattering, $g = 0.85$".
- **`05:242-243`**: "the Henyey--Greenstein phase function (forward scattering, $g = 0.85$)". The
  prior review's M4 ("backward g<0 never validated post-fix; don't claim 'forward and backward'") is
  **FIXED** — the text claims forward only. Cross-checked: FINDINGS has no post-fix backward-HG rung;
  g=−0.85 appears only as the §8.9 pre-fix sign diagnostic. Correct. `fig:scattering-ladder` caption
  (`05:229`) and `fig:showcase` (`05:277`) both say g=0.85. Consistent.

### P3 — `fig:scattering-ladder` "${\sim}68\times$" vs script docstring "~70×": same number, not a
   discrepancy.
- **`05:231-234`** caption: "the *estimate* of that mean tightens ${\sim}68\times$ faster here … its
  mean-estimate standard error ${\sim}68\times$ wider at equal sample count." Sourced to
  `scripts/plots/scattering_convergence.py:58` (`{ms/os_:.0f}×`, computed from the 16-seed per-seed
  image-mean std-devs). The script docstring (`:6`) says "~70x" as a round-number gloss; the rendered
  caption uses the precise computed 68. Both from the same data. **No action.** (If a reader diffs the
  docstring against the caption they'll see 70 vs 68 — harmless, but the docstring could be aligned to
  "~68×" for tidiness; out of chapter scope.)

### P4 — `05:14-17` + `05:235-239` intro/figure "uninformative per-pixel images" framing is honest.
- The intro (`05:14-17`) now states the unbiasedness evidence as "the scattering ladder and the
  furnace invariant" — the prior review's **C2** (the unsound "discrepancy would appear first on
  scenes with a known analytic answer", which conflated absorption-only analytic scenes with the
  argmin sampler) is **FIXED**: there is no such sentence in the current text. The figure caption's
  candid note "their per-pixel images are uninformative because scattering under a near-constant
  environment is almost invisible and the analog reference is firefly-limited" (`05:234-235`) is an
  honest disclosure of why the single/cluster rungs are shown as mean-convergence rather than
  per-pixel diffs. Good — non-overclaiming.

### P5 — `05:270` straight-quote nit (prior review's only-occurrence NIT) — verify.
- Prior review flagged `:97` straight quotes around "differential". Current text uses LaTeX quotes
  (`05:43-44` ``differential''-style is present via \emph in the rewrite; the literal straight-quote
  occurrence appears gone). The remaining quote-like marks (`05:140` ``match to within Monte Carlo
  noise'', `05:180` "differential" cuts both ways at `05:179`) — check `05:179` uses
  ``...''/\enquote rather than ASCII ". Low priority typography.

---

## VERIFIED-CORRECT (no action) — regression inventory

| Prior-review item | Status in current text | Evidence |
|---|---|---|
| **C1** overlap-scatter residual undisclosed + cited as proof | **FIXED** | `05:215-224` discloses it (matches §8.3/§8.4 exactly); `08:54-59` carries it to limitations |
| **C2** unsound intro unbiasedness logic (analytic scenes never exercise argmin) | **FIXED** | `05:14-17` now scopes evidence to "scattering ladder and furnace invariant"; the offending sentence is gone |
| **M1** cloud absorption "vs analytic across all views" overclaim | **FIXED** | `05:184-189`: "against Mitsuba, with … analytic … adjudicating the edge band on a representative view" (= §7) |
| **M2** view-independence mis-attributed to ladder (cam 0 only) | **FIXED** | ladder cloud claim is cam-0-scoped; `05:266` showcase claims "across four well-separated views" (= §8.13 cams 0/6/12/18); low-σ residual disclosed |
| **M3** `fig:absorption-ladder` caption promised per-scene RMSE vs closed-form for all 3 | **FIXED** | caption (`05:194-196`) now "pair and cloud … against Mitsuba with analytic spot-checks" |
| **M4** "forward and backward HG" (backward never validated) | **FIXED** | `05:242-243` "forward scattering ($g=0.85$)" only |
| Furnace test framing (Mitsuba NEE +6.5%, analog as GT) | CORRECT | `05:208-212` = §8.1 exactly |
| Cap-overflow story `exp(-min(cap,K)/K·τ₀)` | CORRECT | `05:174-180` = §6 / FINDINGS:148-156 |
| Bias/variance decomposition E[(A−B)²]=bias²+noise | CORRECT | `05:131-135`, methodology = §8.0 |
| Equal-quality k=RMSE²·N, O(1/√N) | CORRECT | `05:142-146` |
| Differential method = "structured diff = bug, converging = noise" | CORRECT & consistently applied | `05:42-44`; reference confirmed sort-free (running-min, `volprim/integrators/common.py:487` "Find the closest vertex greater than the segment t0" in a while-loop — marching, no global sort) |
| `sec:voxel-gt` absorption numbers (RMSE 0.072→0.057, median 0, analytic 0.416) | CORRECT | = `voxel_gt.md:90-106` |
| `sec:voxel-gt` scattering-drop justification (trilemma) | CORRECT | = `voxgrid_DECISION.md:16-31` |
| `fig:scattering-ladder` 0.4% + 68× + both means | CORRECT | computed in `scattering_convergence.py:57-58` from 16-seed g1 data |
| `fig:showcase` 0.321/0.321, ≈9 s, 64 spp | CORRECT | = `g1_headline.md:11-12` (16-seed) |
| Setup (RTX 3090 / cc8.6 / 24 GB, OptiX 9, CUDA backend, full-clock) | CORRECT | `05:22-31` |
| Build | CLEAN | `latexmk` EXIT=0, 0 undefined refs on settled pass |

---

## CONSISTENCY / SUFFICIENCY NOTES (the load-bearing claims, examined hardest)

**(1) Differential methodology — stated correctly, applied consistently.** Principle at `05:42-44`:
"a small, converging difference is strong evidence of correctness, while a structured difference
localises a bug." Applied: absorption (analytic adjudicates) → furnace (reference-free) → Mitsuba-
analog (structured residual *is* localised — to the overlap core — and disclosed, not hidden). The
methodology's *sensitivity* is itself demonstrated (it resolves 10^-4). One subtle correctness point
the chapter gets right: it never claims the differential alone proves the *sampler* unbiased — it
pairs the differential with the reference-free furnace, which is the only leg that the argmin sampler
cannot "agree-by-shared-bug" with Mitsuba on. That closes the "both share a wrong model" loophole that
`sec:voxel-gt` raises for absorption.

**(2) Absorption ladder — attribution now honest.** Single = closed-form (10^-5). Pair/cloud = Mitsuba
+ analytic spot-checks (closed-form adjudicates single + collinear edge band; §7 24-view is vs
Mitsuba). The prior review's central absorption-ladder finding (caption claimed per-scene RMSE vs
closed-form for all three) is repaired. **Honest now.**

**(3) Scattering unbiasedness — sufficient AND honestly bounded.** See Verdicts 1+3. The evidence is
furnace (analytic, exercises argmin) + mean convergence (0.4%) + Mitsuba-analog differential, with the
one residual disclosed and bounded ≤0.16% peak. The "${\sim}10^{-4}$" is the resolution floor, not a
hand-wave. The chapter does not claim absolute unbiasedness.

**(4) Absorption voxel-grid cross-check — correctly framed, NOT flagged as the (deliberately-dropped)
scattering version.** `sec:voxel-gt` is the absorption leg (IN); the scattering voxel-GT drop is
explained at `05:106-109` and justified by `voxgrid_DECISION.md`. Per the task's explicit instruction,
the scattering voxel-GT is NOT flagged as missing — the remaining scattering evidence is adequate
(Verdict 1). The only loose end is the stale `voxgrid_DECISION.md` wording (S2), which is a record
file, not thesis source.
