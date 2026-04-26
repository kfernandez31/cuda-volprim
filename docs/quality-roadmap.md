# Quality Roadmap — Realistic Plan

Goal: push from "median thesis renderer" to "better than 3/4ths." Scoped to what's achievable solo in finite time with the current codebase.

Estimates are calendar-days assuming focused work (1 person, ~6h/day), including debugging.

---

## Tier 0 — Must-do (correctness & calibration)

These are not optional. Without them, no amount of fancy sampling rescues the asset.

### 0.1 Resolve sigma convention with Jorge
- **Cost:** 0 dev days. Send `docs/jorge.md`, wait for reply.
- **Realistic:** yes — already drafted.
- **Risk:** he may not have time. Fallback: document the empirical 2.2 multiplier as a thesis artifact.

### 0.2 Land NEE on `feature/pre-nee`
- **Cost:** 1.5–2 days. Reimplement using `feature/test-suite` (commits `d19ea98`, `675b88b`) as reference. Conflicts with our CompactSet + escape-split refactor.
- **Realistic:** yes. Plan in `docs/nee.md`.
- **Validation cost:** +0.5 day to render before/after on a scattering scene (need albedo > 0).

### 0.3 Build a scattering validation scene
- **Cost:** 0.5 day. Just override albedo to ~0.9 on cloud asset, render with NEE on/off.
- **Realistic:** yes.

**Tier 0 total: ~3 days.** This alone gets us to "competent."

---

## Tier 1 — High ROI, low risk

### 1.1 Denoiser auxiliary buffers (albedo + normal)
- **What:** Pass guide AOVs to OptiX denoiser. Currently we feed radiance only.
- **Cost:** 1 day. The denoiser API supports it directly; we already have the data at first hit.
- **Payoff:** visibly cleaner edges at low SPP, no extra trace cost. Free quality.
- **Realistic:** yes. Pure infra.

### 1.2 MIS between NEE and free-flight
- **What:** Multiple Importance Sampling weights when both NEE and the implicit BSDF/free-flight escape can hit the env.
- **Cost:** 1.5 days. Power heuristic, careful pdf bookkeeping.
- **Payoff:** removes the "double-count or drop" dilemma from `675b88b`; standard correctness.
- **Realistic:** yes, but needs careful testing — easy to introduce energy bias.

### 1.3 Anisotropic phase function (Henyey-Greenstein)
- **What:** Replace isotropic `1/(4π)` with HG, parameterized by `g`.
- **Cost:** 0.5 day. ~30 lines including importance sampling.
- **Payoff:** clouds actually look like clouds (forward-scattering silver lining, etc.). Big visual win.
- **Realistic:** yes. Trivial.

**Tier 1 total: ~3 days. Combined with Tier 0: ~6 days for a clearly above-average renderer.**

---

## Tier 2 — Differentiators (pick one or two)

These are what move you past 3/4ths. Each is a thesis chapter on its own.

### 2.1 Equiangular sampling for shadow rays
- **What:** Sample distance along shadow ray proportional to 1/d² from light. Drastic variance reduction near directional/point lights inside dense media.
- **Cost:** 2–3 days. Math is straightforward; integration into our NEE shadow ray is mechanical.
- **Payoff:** 5–10× variance reduction near light sources for scattering scenes.
- **Realistic:** yes, but only useful with Tier 1.2 (MIS) and a non-isotropic light setup.
- **Honest take:** for a uniform white env, the win is small. Worth it only if you add a directional sun.

### 2.2 Ratio tracking for transmittance
- **What:** Replace analytic `exp(-τ)` shadow-ray integration with stochastic ratio tracking (unbiased delta tracking variant).
- **Cost:** 2 days.
- **Payoff:** robust to extreme densities where erf integration loses precision; standard for production. Conceptually clean.
- **Realistic:** yes, but you lose some speed (it's stochastic). For Gaussian primitives where the analytic form is closed, it's arguably *worse* unless you add majorant tracking. **Skip unless reviewers ask.**

### 2.3 Differentiable rendering (adjoint pass)
- **What:** Backward pass over the forward path tracer for gradient w.r.t. Gaussian parameters.
- **Cost:** 15–25 days. This is a thesis chapter, not a feature.
- **Payoff:** huge. DSYG is fundamentally about *optimization* — having your own differentiable renderer is the obvious extension Jorge would care about.
- **Realistic:** **only if you have ≥4 weeks left and prioritize it over everything else.** Requires checkpointing, careful memory management, and probably a wavefront refactor (see `hybrid-wavefront-plan.md`).
- **Honest take:** highest impact, highest risk. If you commit, drop everything else in Tier 2.

### 2.4 Spectral rendering (3–8 wavelengths)
- **What:** Replace RGB with sampled wavelengths; albedo and σ_t become spectra.
- **Cost:** 4–6 days for hero-wavelength sampling.
- **Payoff:** physically meaningful Mie scattering, opens chromatic aberration phenomena.
- **Realistic:** yes, but it forces you to source spectral data — annoying if your asset is RGB.

### 2.5 Joint phase × distance sampling
- **What:** Sample scatter direction and distance jointly under HG phase.
- **Cost:** 3–4 days.
- **Payoff:** modest. Mostly relevant at high `|g|`.
- **Honest take:** **skip.** Diminishing returns for thesis effort.

---

## What I'd skip

- **Custom BVH or LBVH refit.** OptiX's BVH is excellent; you won't beat it.
- **Hand-rolled denoiser.** OptiX denoiser + AOVs is already SOTA-adjacent.
- **Wavefront refactor for its own sake.** See `hybrid-wavefront-plan.md`. Only do this if Tier 2.3 demands it.
- **Photon mapping / VPLs.** Powerful but a 2-week build. Out of scope for a thesis on Gaussian primitives.
- **Multi-GPU.** Engineering, not research.

---

## Recommended path

Assuming ~3 weeks of work left:

| Week | Plan | Tier |
|------|------|------|
| 1 | Jorge clarification, NEE, scattering scene, denoiser AOVs, HG phase | 0 + 1.1 + 1.3 |
| 2 | MIS, equiangular sampling, validation suite | 1.2 + 2.1 |
| 3 | Buffer / writing / one of {spectral, ratio tracking} for differentiation | 2.2 or 2.4 |

This realistically lands you in the **top quartile** for a thesis renderer: physically correct Beer-Lambert + HG, denoised, NEE+MIS, with one differentiator.

If you have 6+ weeks, replace week 3 with **2.3 differentiable rendering** — that's where "top quartile" becomes "publishable."

---

## What I would *not* promise

- Bit-matching Mitsuba reference. Convention mismatches and floating-point order make this unrealistic without Jorge's pipeline.
- Sub-0.01 RMSE without resolved sigma calibration. The math is right; the inputs aren't.
- Real-time. We're at 60M samples/sec — solid for offline, far from real-time.
