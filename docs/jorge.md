# Status & Questions for Jorge

## Where we stand

- Cloud renders correctly as a dark volumetric shape on white background.
- Orientation fixed, smooth continuous cloud (not individual visible blobs).
- **Best RMSE vs `ref-better.exr`:** 0.0505 — 1024 SPP, OptiX denoiser, `sigma_multiplier=2.2`, single ortho camera.
- Performance: ~9s for 1024 SPP @ 900×600, ≈60M samples/sec on RTX 3090.
- ADT (argmin per SDTracking Theorem 1) is in place; escape path uses segment-by-segment integration.

## Bugs we found and fixed

1. **`optical_depth(t0, t1)` sign error.** The erf formula in our `primitive.h` (and the analogous `density_integral` in your Mitsuba) used
   `(erf(B/√2) + erf((t_limit-B)/√2)) * 0.5`,
   but the correct bounded integral is
   `(erf((B+t_limit)/√2) - erf(B/√2)) * 0.5`.
   The two only agree when `B = 0`. Verified against numerical quadrature to machine precision.
2. **ADT scatter sampling for `hit_buffer` entries.** Was rejecting all samples for dense media because the inv_cdf returned values before the entry t_hit. Restructured the argmin loop to handle this correctly (still ADT, no sorting).
3. **Albedo override.** PLY ships `albedo ≈ 0` (because `init_albedo=0, albedo_lr=0`); your `GaussianKernel.add()` default is `1.0`. We render with PLY values now → produces correct Beer-Lambert attenuation against the white env.

## Open questions

1. **Sigma convention.** `sigmat_scale = 7.5` from `args.json` gives a way-too-dark cloud. `sigma_multiplier = 2.2` (applied as `expf(sigma_t[i]) * 2.2 * (2π)^{3/2} * ∏s`) matches `ref-better` visually. What's the correct mapping from `sigma_t` in the PLY and `sigmat_scale` in `args.json` to a physical extinction coefficient? Is there an extra factor in your Mitsuba pipeline we're missing?
2. **What produced `ref-better.exr`?** Voxel grid or Gaussian primitives? What sigma/albedo were used? We'd like to bit-match the render setup, not just visual-match.
3. **Albedo for rendering.** PLY = 0 vs your default = 1.0. For *this asset* specifically, which is correct? (We assumed PLY since the asset was optimized with `albedo_lr=0`, locking it to init.)
4. **`density_integral` bounded form.** Does your Mitsuba pipeline ever call `density_integral` with `full_range=False`? If so, the erf bug above affects it — and would manifest only when the ray origin sits inside a primitive.

## Next step

Pending your answers, our plan is:
- Cherry-pick / reimplement NEE (already exists on `feature/test-suite`, 2 commits) once we have a scattering scene to test against.
- For the pure-absorber cloud, NEE is a no-op; we'll need a higher-albedo scene to validate it.
