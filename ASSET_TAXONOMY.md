# Asset Taxonomy & Thesis-Usefulness Assessment

Generated 2026-06-06. Source: `assets/*.zip` (11.8 GB) → extracted to `assets/unpacked/` (30 GB, 22 dirs).
All assets are outputs of Jorge's **hierarchical-Gabor** optimizer (a DSYG successor that fits a
coarse **Gaussian** base + a high-frequency **Gabor-noise** layer to a reference volume).

---

## TL;DR — what's actually usable by our renderer

Our renderer ingests **Gaussian** primitives (DSYG). The optimizer produces two fit modes, and the
mode is what decides usefulness:

| Mode | `skip_gabor_optim` | What the PLY contains | Usable by us? |
|------|--------------------|------------------------|---------------|
| **`_gauss`** (pure Gaussian) | `true` | the full 16k–25k Gaussian fit | **YES — directly** |
| **base / `_gabor`** | `false` | only a coarse 512–4k Gaussian *base* (detail lives in Gabor kernels) | Partially — coarse blob only |

**Four directly-renderable, high-quality Gaussian assets** (the `_gauss` fits):

| Asset | Gaussians | Phenomenon | Mitsuba ref? |
|-------|-----------|-----------|--------------|
| `wdas8_gauss` | 24,576 | WDAS Disney cloud (⅛ res) — famous benchmark | ✓ `reference_pyr0.exr` |
| `bunny_gauss_1024x24k` | 25,600 | Stanford bunny as a volumetric cloud | ✓ |
| `smoke_gauss` | 16,384 | wispy smoke plume | ✓ |
| `embergen_gauss` | 24,576 | dense combustion plume (EmberGen) | ✓ |

Everything else (18 dirs) is a **Gabor fit** → its PLY is just a coarse Gaussian base; rendering it
gives a blobby low-detail version. Not useful as-is unless we implement Gabor kernels (out of scope).

**Two cross-cutting caveats:**
- **All fits are `init_albedo=0` (absorption).** Scattering is obtained at render time via `SG_ALBEDO`
  override (as `cloud_asset_scattering` already does). The bundled `reference_*.exr` are therefore
  **absorption** ground truth — good for per-asset *absorption* validation; scattering still needs
  controlled volprim runs.
- **No emission data anywhere.** `npy_data` has only `centers/scales/quaternions/omega/opacities/
  albedo/extent`. So fire/explosion/tornado render as **dark smoke**, not glowing fire, until both
  (a) Phase 5 emission is implemented AND (b) emission values are fit/added (they aren't in these).

---

## Anatomy of one asset (common layout)

```
<name>/
  args.json                                  # optimizer config (kernel mode, counts, volume src)
  grid_pyr{0,1,2}.npy                         # source volume grid, per pyramid level (0 = finest)
  optimized_asset_pyr0/
    data/root.primitives_pyr0.ply            # ← THE DSYG GAUSSIAN PLY (directly loadable)
    npy_data/{centers,scales,quaternions,    # ← raw params (our npy→PLY converter input)
              omega,opacities,albedo,extent}_pyr0.npy
  reference_pyr{0,1}.exr                      # Mitsuba ground-truth render(s) → VALIDATION REF
  optimized_pyr{0,1}.exr                      # the fit's own render (quality check)
  *.png                                       # preview thumbnails
  loss/l1/ssim_pyr*.npy                        # training curves (ignore)
  pyramid_cache_metadata.json                 # LOD metadata
```

**PLY format** = exactly our DSYG layout: `x y z` (center), `nx ny nz` (unused), `omega_0`,
`opacities_0`, `albedo_0..2` (RGB), `extent_0`, `scale_0..2`, `rot_0..3` (quat). Loadable directly,
or rebuilt from `npy_data` via the existing converter (scale=log, quat xyzw→wxyz, opacity→σ_t).

---

## Taxonomy by source phenomenon

### 1. WDAS Disney Cloud — the famous offline-rendering benchmark
| Dir | Gaussians (PLY) | Notes |
|-----|------|-------|
| `wdas_export` | 4,096 base + 100k gabor | full-res "hero" fit; **gabor** (coarse base only in PLY) |
| `wdas4_12_1`, `wdas4_12_2` | 4,096 base | quarter-res, two crops/views; gabor. `_small/_big/_mid` = gabor-count tiers (8k/65k/12k) |
| `wdas8`, `wdas8_dense` | 768 base | eighth-res; gabor |
| **`wdas8_gauss`** | **24,576** | eighth-res, **pure Gaussian** — the usable one |

### 2. Bunny cloud — Stanford bunny voxelized as participating media
| Dir | Gaussians | Notes |
|-----|------|-------|
| **`bunny_gauss_1024x24k`** | **25,600** | **pure Gaussian** — usable; recognizable shape = great "generalizes" figure |
| `bunny_gabor_1024x24k` (+`_2`) | 1,024 base | gabor; `_2` is a 2.4 MB re-export (params only, no big EXRs) |

### 3. Smoke
| Dir | Gaussians | Notes |
|-----|------|-------|
| **`smoke_gauss`** | **16,384** | **pure Gaussian** — usable; wispy morphology adds diversity |
| `smoke` | 512 base | gabor |

### 4. Combustion / emissive-intent (fire, explosion, tornado, embergen)
| Dir | Gaussians | Notes |
|-----|------|-------|
| **`embergen_gauss`** | **24,576** | **pure Gaussian** — usable, but no emission → renders as dark plume |
| `embergen` | 512 base | gabor |
| `fire` | 512 base | gabor |
| `explosion` | 1,024 base | gabor |
| `tornado` | 768 base | gabor (single frame `tornado_0`) |
| `torni` | per-frame | tornado **animation** (`tornado_0..N`, 410 files) — gabor; out of scope (no anim support) |

---

## Thesis-usefulness tiers

**Tier A — use now (Phase 4 asset generalization + per-asset validation):**
- **`wdas8_gauss`** — *the* first asset to add. Famous WDAS cloud, pure Gaussian, ships a Mitsuba ref.
  This is the "method, not just our 652-Gaussian cloud" headline.
- **`bunny_gauss`** — recognizable shape; strong figure that the renderer generalizes to arbitrary
  volumes. (⚠️ verify it doesn't hit the known bunny NaN-at-σ=7.5 bug — Phase 2; this is a different
  fit than the one that NaN'd, so re-test.)
- **`smoke_gauss`** — different (wispy) morphology → visual diversity in the results chapter.

**Tier B — usable but caveated:**
- **`embergen_gauss`** — pure Gaussian and dense, but it's a *fire* sim with no emission, so it only
  reads as a grey plume. Use only if you want a 4th scattering shape; the real payoff needs Phase 5.

**Tier C — needs features we don't have (likely out of scope):**
- All **gabor** fits (wdas_export, wdas4_*, wdas8, fire, explosion, tornado, smoke, embergen, bunny_gabor):
  rendering their PLY gives only a coarse blob. Worth it *only* as a deliberate "coarse-base vs
  fine-fit" ablation, or if Gabor-kernel support is ever added.
- **Emissive** assets (fire/explosion/tornado/torni/embergen): gated on Jorge's emission decision
  AND on emission data that these fits don't contain.

**Tier D — redundant / dev variants (keep zipped, don't clutter):**
- `wdas4_1_small/_big/_mid`, `wdas4_2*`, `wdas8_dense`, `bunny_gabor_..._2` — resolution / gabor-count
  ablation tiers of assets already covered above. Useful only for an LOD/perf-scaling experiment.

---

## Free validation references

Every `_gauss` asset ships `reference_pyr0.exr` (Mitsuba ground truth, 32-cam) — these are the
per-asset Mitsuba references Phase 4's ≤1e-4 validation needs, **for the absorption path**. For the
scattering showcase, generate controlled volprim references with matched `--hg-g`/`--max-depth`
(now possible without a rebuild after Phase 1).

## Disk note
`assets/unpacked/` is 30 GB; the source `*.zip` are another 11.8 GB. Once the 4 Tier-A assets are
copied into the renderer's asset path, the Tier-C/D unpacked dirs (and possibly the zips) can be
deleted to reclaim ~35 GB. `assets/unpacked/` is not gitignored — add it.
