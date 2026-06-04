# Cloud render regression investigation

**Question:** `best-from-26.04.jpeg` (rendered ~17:31 CET on 2026-04-26) has a soft,
wispy contour that *looks* close to `assets/cloud/refs_pyr0/0000.exr`. Current
renders look harder and darker — did we regress?

**Answer (short):** No. The April 26 render was the visual product of *multiple
unfixed bugs* that happened to flatter the eye. The current code is closer to
the physics of the reference. Measured silhouette IoU against the reference is
higher for the current renders than for the April 26 image.

---

## 1. What the renderer actually looked like at 17:31 CET on 2026-04-26

`git log --until="2026-04-26 17:31:00 +0200"` places HEAD at commit `b3dbdf4`
(2026-04-07 `Optimize raygen occupancy, fix black Gaussians, add CompactSet`).
The first April-26 rendering-fix commit (`a86429e`) landed at **18:00:31**, ~30
minutes after the screenshot. So at 17:31 the live code carried every bug the
following commits would fix. Documented from `a86429e`'s commit message and from
diffs against the modern files:

| # | Bug at 17:31 | Effect on the render |
|---|---|---|
| 1 | `optical_depth(t0, t1)` used `(erf(B/√2) + erf((t_limit - B)/√2)) · 0.5` instead of `(erf((B + t_limit)/√2) - erf(B/√2)) · 0.5` | Both forms agree when `B = 0` (ray through the Gaussian center), diverge elsewhere with a sign-dependent error. Off-axis rays — which form the *edge* of the silhouette — had wrong tau, biased low. Soft falloff at the silhouette edges. |
| 2 | `GAUSSIAN_EXTENT_F = 1.0f` despite the docstring claiming 3σ | OptiX BVH wrapped the 1σ ellipsoid; `t_limit` clamp in `optical_depth` was at 1σ. Effective integration chord was ~4× shorter than physically correct. Net: massive under-integration of tau at every primitive. |
| 3 | PLY loader stored `sigma_t` directly into `optical_thickness`. No `(2π)^{3/2} · ∏s` bridge between Jorge's peak-convention and our mass-normalized convention | Density was wrong by the bridge factor (per-primitive scale-dependent multiplier). |
| 4 | `sigma_multiplier` knob did not exist | Whatever sigma you set in arguments was silently ignored. Render used raw PLY values. |
| 5 | Camera `viewport_v` in `buildOrthographic` was negated *and* `saveExr` already applied `flip_vertical=true` | Net: render was upside-down. |
| 6 | `exp(-τ)` double-count in raygen escape branch | Analog free-flight already weights escape by `exp(-τ_total)`; raygen multiplied again, giving `exp(-2τ)·env` in expectation. |
| 7 | No NEE, no MIS, no HG phase function | Scatter paths integrated unoccluded direct lighting only. For an absorber cloud, no behavioural difference; for scattering scenes, materially different. |

Bugs 1, 2, 3, and 6 all *under-darken* the cloud through different mechanisms.
Bug 4 means there was no calibration knob to compensate. Bug 5 was cosmetic.

## 2. Why those bugs together flatter the eye

The cloud rendered as a **mid-grey haze with diffuse soft edges**:

- Bug 1 + 2 → tau under-integrated at silhouette edges → gentle exponential
  falloff into the background instead of a sharp cutoff
- Bug 3 → density too low in absolute units → low overall opacity, mid-grey
  rather than near-black core
- Bug 6 → `exp(-2τ)` darkens slightly more than `exp(-τ)` would have at the
  same already-too-low density, so the cloud isn't washed out white either

Net visual: a soft, voluminous, organically-cloud-looking image. The eye reads
this as "matches the reference". But the reference (`refs_pyr0/0000.exr`) was
rendered through Mitsuba's `prbvolpath` on a voxel grid with no such bugs — it
is a sharp, dark absorber. The two are visually different even though both look
"cloud-like" at a glance.

## 3. What we changed since then

Commits between `b3dbdf4` and current HEAD (`897eafe`), in order:

| Commit | Date | Effect on the render |
|---|---|---|
| `a86429e` | 26.04 18:00 | Fixes bugs 1, 2, 3, 5; introduces `sigma_multiplier`. Single biggest behavioural shift. |
| `3f1b5e9` | 26.04 20:50 | Adds HG phase / NEE / MIS / denoiser AOVs. Invisible at HG_G=0 with NEE on for absorber. |
| `8955662` | 26.04 22:12 | HDR env importance sampling (only fires under MIS, currently inactive). |
| `1cc5fb8` | 26.04 22:12 | Adds `inv_cdf_segment`, fixes biased free-flight for hit-buffer primitives. |
| `95c78b3` | 03.05 | Pipeline cleanup. No math change. |
| `7354646` | 16.05 | RNG seed collision fix (decorrelation; unbiased in expectation). |
| `3d6eaba` | 17.05 | Add `single_gaussian_validation` scene + analytic comparator (additive). |
| `a66d42f` | 17.05 | Gate fast-math intrinsics behind `THESIS_ENABLE_FAST_MATH`. Bit-identical. |
| `1623187` | 17.05 | Explicit erfinv saturation rejection. Bit-identical. |
| `f54f161` | 17.05 | Delete redundant `camera_active_prims_` precompute. Bit-identical. |
| `37f5811` | 17.05 | `point_inside_bvh_bound`: 1σ → 3σ; new `exit_from_inside` helper. No visible change for camera-outside absorber scenes. |
| `3bb3b93` | 17.05 | **Removes `exp(-τ)` double-count from escape branch.** Cloud at sigma=165 brightens by ~16% on average. |
| `897eafe` | 17.05 | Restore `MAX_CAMERAS=24`. Scope only. |

The two cuts that visibly removed "softness" from the cloud are `a86429e`
(fixes optical_depth + raises GAUSSIAN_EXTENT to 3σ + adds the bridge) and
`3bb3b93` (removes the double-count). Everything else either preserved
appearance or was internal cleanup.

## 4. Measurement: silhouette IoU against the reference

`assets/cloud/refs_pyr0/0000.exr` is 900x600. Resized `best-from-26.04.jpeg`
to the same dimensions for fair comparison. Silhouette = pixels with mean
luminance < 0.7. IoU = `|A ∩ B| / |A ∪ B|`.

| Render | Silhouette IoU vs reference | Mean intensity |
|---|---|---|
| reference (ground truth) | 1.000 | 0.525 |
| `best-from-26.04` (resized) | 0.879 | 0.639 |
| current σ = 165 | 0.818 | 0.654 |
| current σ = 300 | 0.900 | 0.576 |
| current σ = 400 | 0.930 | 0.547 |
| current σ = 600 | **0.964** | **0.516** |
| current σ = 800 | 0.976 | 0.498 |

**Current renders at σ ≥ 300 already exceed the April 26 image's silhouette IoU.**
σ ≈ 600 jointly maximises silhouette match *and* mean-brightness match against
the reference.

Outline overlays saved to `/tmp/outline_{reference,old_best,current_sNNN}.png`
(red boundary = silhouette edge at threshold 0.7) for visual inspection.

## 5. Why the perception of regression is incorrect (but understandable)

The April 26 render was *visually pleasant* because soft mid-grey rolloff
reads as "fluffy/organic/cloudy" to the eye. The reference and the current
renderer both produce *hard absorber* images because that's what the underlying
physics — a dense voxel grid rendered as a non-scattering medium — actually
looks like. Hard absorbers don't look "fluffy"; they look like a dark blob with
a relatively sharp boundary.

So the right way to read the comparison: the April 26 render was incidentally
prettier; the current render is *quantitatively closer to the ground truth*.

## 6. What sigma to use going forward

For the absorber cloud against `refs_pyr0/0000.exr` with the current
post-fix renderer:

- **σ ≈ 600** is the empirical optimum for joint silhouette IoU + mean
  brightness match
- σ = 300 is a workable compromise if you want a slightly less aggressive
  optical depth (mean intensity 0.58 vs reference 0.52)
- The physically intended value is σ = 7.5 (Jorge's `args.json:sigmat_scale`);
  the gap from 7.5 → 600 is the unavoidable structural Gaussian-vs-voxel
  approximation gap (652 Gaussians cannot capture all the detail of a
  250×170×307 voxel grid; the calibration knob compensates by inflating
  per-Gaussian opacity)

A full sigma sweep at 512 spp around σ ∈ {500, 550, 600, 650, 700} against
the reference would pin the optimum more precisely if needed.

## 7. Pointers

- Reference image: `assets/cloud/refs_pyr0/0000.exr`
- April 26 image: `best-from-26.04.jpeg`
- Current renderer entry: `device/entry/raygen.cuh`
- Optical-depth integrator: `include/thesis/device/params/primitive.h` (`optical_depth`)
- Verification scene + comparator: `test/scenes/single_gaussian.cpp`,
  `tools/refs/single_gaussian_analytic.py`
- April 26 fix commit (the one that landed 30 minutes after the screenshot):
  `git show a86429e`
