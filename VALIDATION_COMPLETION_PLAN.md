# Validation completion / hardening plan (2026-06-04)

> The renderer is ALREADY thesis-grade validated (core physics + meadow + HG + MIS, 3 bugs
> found & fixed). This plan is about *closing the remaining rigor gaps*, *auto-gating* the
> result, and *committing* — not "it isn't validated yet." Evidence: FINDINGS §8. After this,
> pivot to optimization (PLAN.md / TODO.md) and thesis writing.

## Already validated (scope — do NOT redo)
- Absorption single→cluster→cloud: exact vs analytic & Mitsuba `volprim_prb`.
- Scattering (albedo>0): furnace energy; single→cluster→cloud vs Mitsuba-analog ≤~1e-4 (§8.1–8.5).
- Real HDR env (meadow): single + cloud; env path + `env_is` (§8.6–8.8).
- HG anisotropy g=0.85 (§8.9). MIS: furnace-exact, matches NEE estimator 0.7σ, ~159× variance (§8.10).
- 3 bugs fixed: env vertical flip (hdr.cpp), HG sign (phase::sample), MIS sign (phase::eval).
- Regression gate (furnace energy) green; match/beat benchmark on cloud+meadow.

---

## P1 — Path-control robustness sweeps (CHEAP, high rigor) — DO FIRST
Prove the termination knobs change only variance/speed, NOT the converged mean (invariance =
the validation). Furnace (albedo=1) already implies these are fine; explicit sweeps are cleaner
thesis evidence. Scenes: single-G (fast) + cloud cam0. Metric: mean diff vs the spp-converged
image should be within noise.
- [ ] **RR_DEPTH** sweep (off / 1 / 5 / 10): mean invariant → RR unbiased.
- [ ] **MAX_BOUNCES** sweep (32 / 64 / 128 / 256): cloud mean flat → depth converged.
- [ ] **MIN_THROUGHPUT** sweep (1e-3 / 1e-4 / 1e-5 / 0): bound the one genuine bias source.
Deliverable: a "mean invariant to X" table in FINDINGS. Each is a rebuild + a couple renders.

## P1.5 — Asset generalization (HIGH thesis value — gated on Jorge's assets)
"Match & beat on ONE cloud" invites cherry-pick doubts; "across N diverse DSYG assets" is a real
generalization claim + a richer beauty gallery. Pipeline is now turnkey (conventions solved).
- [ ] **Obtain assets** from Jorge's Outlook drive (user action). Per asset need: PLY primitives,
      scene config (cameras + extent + density scale), Jorge's density scale (cloud=7.5, tornado=60
      per [[reference_asset_density_scales]]); optional Jorge reference renders.
- [ ] **Target 3–4 validated total** (cloud + 2–3). Validate EVERY loadable asset (diversity =
      the test, incl. dense/ugly ones); SHOWCASE the cleanest 3–4. >5 only as free extra beauty.
- [ ] **Per-asset pipeline:** load PLY + density scale → render CUDA + Mitsuba-analog → furnace
      energy check (albedo=1) → systematic vs analog (≤~1e-4) → check overlap depth vs the 128
      caps (denser assets may trip MAX_ACTIVE_PRIMS/HIT_BUFFER silent overflow — a useful boundary
      test) → MIS beauty under meadow. Diversity goal: density / overlap-depth / shape.

### P1.5 status (2026-06-04) — conventions SOLVED, bunny ready, render plumbing next
**Assets** = DSYG/GaborVolumes Gaussian-pyramid. **pyr0 = Gaussian (we render it); pyr1+ = Gabor
(out of scope, no Gabor support).** Format: `optimized_asset_pyr0/npy_data/{centers,scales,
quaternions,opacities,albedo,omega,extent}_pyr0.npy` AND a ready PLY at `.../data/
root.primitives_pyr0.ply`. The asset PLY uses property name `opacities_0` (not our required
`sigma_t_0`), so our loader can't read it directly.
- **Conventions NAILED** (verified via the bunny's own PLY↔npy Rosetta): center=direct;
  **scale = log(npy_linear)** (our loader expf's it); **quaternion npy=(x,y,z,w) → our
  rot=(w,x,y,z)**; **opacity → sigma_t** (× a per-asset density scaler, à la cloud=7.5; Jorge to
  give exact scalers — calibrate if absent). albedo=0 in-asset (absorption; override per-render).
- **Converter DONE + validated:** `tools/refs/npy_asset_to_ply.py <pyr0/npy_data> <out.ply>`
  reproduces the asset PLY exactly into our format. Produced `assets/bunny/bunny_pyr0.ply` (25600
  prims). [Alternative, cleaner for the batch: add `opacities_0`/`omega_0`/`extent_0` aliases to
  the C++ PLY loader so asset PLYs load directly — one rebuild, then no per-asset conversion.]
- **bunny args.json:** cam_count=32, res 1024², max_depth=-1, ref_spp=2048, vol_size_scale=1.0,
  centers in ~[-1,1]³. opacities tiny (~1e-3) → density scaler will be sizable.
- **Second reference available:** `~/jorge/advol` (voxel-grid lib) + voxel `.npy` grids (other
  SharePoint link) — compare Gaussian render vs a dense voxel grid of the same content (strong
  independent cross-check). Set up: pip install mitsuba drjit, then `pip install -e advol`.
- **REMAINING render plumbing (the next chunk):**
  1. CUDA: a generic asset scene (load a PLY via env e.g. `SG_PLY`, framed perspective/ortho
     camera, `SG_ENV`) — or extend cloud scene. Needs the loader to accept the asset PLY (alias)
     OR use the converter output.
  2. Mitsuba reference: generalize `render_cloud_prb_absorption.py` → `render_asset_via_prb.py`
     (arbitrary PLY/npy as analytic `ellipsoids`, volprim_prb **analog**, matched camera+env+scaler).
     NB Jorge's `render_asset_new.py` uses `volprim_prb_hierarchical_v2` — for apples-to-apples with
     our validated cloud pipeline, use plain `volprim_prb` analog instead.
  3. **Camera matching** (Piotr's flag): replicate `create_cameras` geometry (cam_count/res/scale/
     fov/distance, `create_cameras` ~line 540-635 of render_asset_new.py) on both sides, OR define
     one shared camera both sides (like single_gaussian's SG_VIEW).
  4. Calibrate density scaler (match Mitsuba), furnace-style energy check, systematic, MIS beauty.
  5. Batch the remaining `_gauss` assets; keep the best for the showcase; add advol voxel cross-check.

## P2 — Coverage gaps (moderate)
- [ ] **More cloud cameras** under meadow+scattering (only cam 0 done; other 23 validated only
      under constant-env/absorption). A few representative views confirm view-independence.
- [ ] **Low-σ interior check** (σ≈2): at σ=7.5 the dense interior clamps near-black on both
      sides (match partly trivial). Lower σ → non-saturated interior comparison (noted in §9).

## P3 — Deferred / off features (OPTIONAL — only if you'll use them)
- [ ] **Colored per-channel RGB albedo**: colored-albedo single-G vs Mitsuba (closes the deferred
      item; meadow already covers RGB *lighting*, this covers RGB *albedo*).
- [ ] **Adaptive sampling** (`ENABLE_ADAPTIVE_SAMPLING=false`): validate ONLY if enabling
      (invariance of mean + convergence of the stopping rule). NB largely redundant with MIS now
      (MIS already removed the spatial variance heterogeneity it would exploit). Not a Mitsuba
      differentiator — generic technique.
- [ ] **OptiX denoiser** (`--denoise`): non-physical post-process; "validation" = visual only.
      Skip unless needed for beauty figures.

## P4 — Lock it in (DO with P1)
- [ ] **Extend `validate_ladder.sh`**: add furnace-HG and furnace-MIS rungs, a config-matched
      meadow single-G systematic rung, and a MIS-vs-NEE-estimator rung → one command gates the
      whole feature set before any merge.
- [ ] **Finalize FINDINGS** (mostly done): fold in the P1 sweep results + the match/beat benchmark.
- [ ] **COMMIT** (user-gated): the 3 source fixes (hdr.cpp, sampling.cuh ×2), config
      (constants.cuh: HG_G, ENABLE_MIS — user decides committed defaults), scene/script knobs,
      the new tool scripts, FINDINGS/TODO/memory. main → validated + shippable, tag it.

---

## After validation (project arc — PLAN.md / TODO.md)
- **Optimization** (the durable speed advantage): per-step Rao-Blackwellization (TODO A1 — the
  real throughput/variance lever; we're 2.9× slower per-spp), buffer sizing + NSight (B1/B2),
  wavefront (B3, big bet). NB MIS already gives the env-scene win; A1 attacks the flat-scene gap.
- **Thesis writing** (the gating deliverable): method, validation spine (§8), honest performance
  analysis (match/beat + scene-dependence), the 3-bugs-found narrative.

## Recommendation
**P1 + P4 are the must-do closers** (cheap; make validation airtight, auto-gated, committed).
P2 is nice-to-have. P3 only if those features matter. Then pivot to thesis + optimization.
Estimated: P1 ~½ day, P4 ~½ day, P2 ~½–1 day, P3 ~½ day each.
