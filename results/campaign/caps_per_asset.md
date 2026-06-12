# Per-asset cap verification — tornado / explosion / bunny (§0.2 + §0.3), measured 2026-06-11

**Question.** Jorge's final lineup adds three assets to the cloud. `estimate_caps.py` predicts each
needs caps different from the cloud's stock 128/128 (`scripts/tools/caps_table.csv`). Are those
predictions (a) *necessary* — does stock actually overflow — and (b) *sufficient* — do the predicted
caps render with zero drops? And is each recompiled binary still correct (§0.3)?

**Why this is runnable now (clock-independent).** Overflow counts are deterministic functions of ray
geometry, not timing, so the 150 W cap is irrelevant. And **"Cap check: 0 overflows" is itself the
sufficiency proof**: zero drops means nothing was truncated, so the image is bit-identical to what
unbounded caps would produce — no reference diff needed.

**Assets (converted from Jorge's native npy via `tools/refs/npy_asset_to_ply.py`, `QUAT_ORDER=xyzw`):**
tornado 768 prims, explosion 1024, bunny 25 600. Cloud unchanged (652, stays 128/128).

## Method

1. **Necessary** — render at stock 128/128, confirm the overflow counter fires where the estimator
   said it would. Tested under both absorption (primary + shadow rays) and **scattering** (albedo 0.9,
   meadow — secondary rays bounce in arbitrary directions, sampling far more chords through the volume,
   so this is the *binding* stress and the closest match to the estimator's whole-bbox line sampling).
2. **Sufficient** — rebuild at the estimator's caps, confirm `Cap check: 0 overflows` on the same
   scattering stress views.
3. **Correct (§0.3 re-gate)** — furnace (single Gaussian, albedo 1.0, constant env) @ 1024 spp;
   `furnace_check.py` must report bias OK + structure OK. Cap-independent (overlap = 1), so it isolates
   "did the recompile break the renderer" from "are the caps big enough." The cap A/B (`caps_ab.md`)
   already established that buffer-size-only edits are numerically equivalent, so this is confirmation,
   not discovery.

## Results

| Asset | Estimator caps (active/hit) | Stock 128/128 (scattering, diag) | At estimator caps | Furnace @1024 |
|---|---|---|---|---|
| tornado | **112 / 432** | hit overflow: 1 769 787 drops | 0 overflows (negz/diag/posy) | PASS (bias+struct) |
| explosion | **32 / 176** | hit overflow: 4 drops (marginal) | 0 overflows (diag/posy/negz) | PASS (bias+struct) |
| bunny | **320 / 496** | hit overflow: 59 019 672 drops | 0 overflows (negz/diag/posy) | PASS (bias+struct) |

The binding cap is the **hit buffer** in every case (consistent with `caps_ab.md` / Ch 4). Note
explosion's active cap *drops* 128→32 (a memory saving) while its hit cap rises 128→176.

## Key finding — the estimator is a conservative whole-bbox bound; scattering is the binding stress

The estimator Monte-Carlo-samples points and **lines through the entire bounding box**, so its maxima
bound *any* camera and *any* path. A single camera under **absorption** can therefore stay under stock
even when the estimator flags the asset:

- **explosion** rendered **clean at stock 128 from all seven axis/diagonal views at 1024² (absorption)**
  — yet the estimator's hit_max is 136 (> 128). Only under **scattering** did the predicted marginal
  overflow appear (4 drops, diag/posy). This is the estimator working exactly as designed: it called
  explosion *marginal* (136, just over 128), and scattering revealed exactly that — 4 drops, versus
  tornado's millions.
- **bunny** trips the hit buffer enormously (137M–210M drops across all seven axis + diagonal views,
  scattering @ 768²/24 spp) but its **active-set** (estimator active_max 245 > 128) did **not** fire
  from *any* of those views — the densest 245-overlap point the estimator found is not on a
  camera/scatter path. The shell of 25 600 tiny Gaussians means a tangential ray *enters* hundreds of
  primitives (hit buffer overflows hugely) yet few overlap at any single *point* (active set stays
  under 128). So for the bunny the binding constraint is unambiguously the hit buffer; the 320 active
  cap is sized to the estimator's whole-bbox bound for camera-independent safety, not to any observed
  render.

**Takeaway for the thesis (§0/Ch 4):** size to the estimator (conservative, camera-independent), and
verify with the **scattering** stress, where the marginal cases manifest. The per-asset recompile is
*necessary* (stock overflows under the binding stress) and the predicted caps are *sufficient* (zero
drops). Cloud stays 128/128.

## Reproduce

```bash
# convert (native npy -> renderer PLY)
tools/refs/.venv/bin/python tools/refs/npy_asset_to_ply.py \
  assets/models/unpacked/tornado/optimized_asset_pyr0/npy_data assets/models/tornado/tornado_pyr0.ply
# (same for explosion)

# per-asset build (edit the two constants, rebuild ~9 s)
sed -i 's/MAX_ACTIVE_PRIMS = 128;/MAX_ACTIVE_PRIMS = 112;/;s/HIT_BUFFER_CAPACITY = 128;/HIT_BUFFER_CAPACITY = 432;/' \
  device/core/constants.cuh
cmake --build build --target test_runner -j

# binding stress (scattering) — expect "Cap check: 0 overflows"
SG_PLY=assets/models/tornado/tornado_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=512 SG_VIEW=diag \
  build/bin/Release/test_runner --scene asset_validation --spp 16

# furnace (§0.3 correctness) — expect PASS at 1024 spp
SG_ALBEDO=1.0 SG_ENV=white_constant \
  build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024
tools/refs/.venv/bin/python tools/refs/furnace_check.py test_results/single_gaussian_validation/0000.exr

git checkout device/core/constants.cuh   # restore canonical 128/128
```

Canonical repo state stays **128/128/1024** (cloud). Per-asset builds are transient; the recipe above
reproduces each for the window. The estimator predictions are now **verified**, not assumed.

---

## SUPERSEDED for sizing (2026-06-12) — measurement replaced estimation

The cap-domain session on `main` added `--measure-caps` (in-render, cap-independent counters) +
`scripts/tools/calibrate_caps.sh` (measure → write constants → rebuild → verify on an unmeasured
seed). **That workflow supersedes `estimate_caps.py` as the sizing authority**; the calibrated caps
are cloud **64/96**, tornado **112/384**, explosion **32/160**, bunny **80/528**
(`results/campaign/cap_calibration.md`). This record's verification logic stands (necessity at stock,
sufficiency via 0 overflows, scattering as the binding stress — calibration adopted that stress), but
its estimator-based cap VALUES are obsolete; the estimator survives as a camera-independent ceiling
with a measured caveat (bunny: active bound 4× over, worst chord ~20 % under — margin-saved).
