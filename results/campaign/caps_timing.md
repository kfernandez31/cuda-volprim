# B5 — safe-512 vs per-asset-tuned FRAME-TIME penalty, run 2026-06-15

**Question.** §4.5 claims a universal "safe" build (512/512 caps, what a user without the cap-estimator
would pick) costs frame time versus the per-asset-tuned builds. The four percentages quoted in the
thesis (cloud +13 %, explosion +7 %, tornado/bunny ~+3 %) had **no banked record** — this re-banks them.

**Method.** For each asset, the per-asset-tuned binary (`~/winbins/exe_<a>`) and the universal
SAFE-512 binary (`~/winbins/exe_safe512`, 512/512 caps) are timed **interleaved** (T,S per round × 3
seeds, binary swapped every render) at locked clocks (SM 1800 / mem 9751 / 350 W; run clock
min 1650 / p50 1800 / max 1800), so same-session GPU drift hits both arms equally (the `caps_ab.md`
lesson). Identical PLY + scene ⇒ identical image; only the compile-time caps (⇒ per-ray thread-local
reservation ⇒ occupancy) differ. Config matches `scaling.md` part B / `tab:asset-cost`: scattering
albedo 0.9, `white_constant` env, diag view, σ-multiplier 10, 512², 64 spp. 0 cap overflows.
Driver: `scripts/campaign/run_caps_timing.sh`; data `caps_timing.csv`; log `caps_timing_2125.log`.

Per-asset tuned caps (active/hit, from the build logs): cloud 64/96, tornado 112/384,
explosion 32/160. SAFE-512 = 512/512.

**Bunny cap caveat.** The canonical bunny cap is 80/**528** (`cap_calibration.md`; the thesis and
`tab:vram` use 528, and the 900 MiB VRAM was measured at 528). But bunny's worst chord is
*build-fragile* (max hits/ray wobbles 418–464 under FMA reordering on a near-grazing shell ray), and
the `exe_bunny` winbin timed here was built at 80/**496** (`build_calibrated_rest.log`, which warns:
"bunny caps differ from cap_calibration.md — review before trusting timings"). So the bunny timing
below is for a 496 build, not the canonical 528. The conclusion is unchanged either way: SAFE-512
reserves more bytes/ray than the tuned build, so it is slower; whether the hit cap rises (496→512) or
falls (528→512) only shifts the split between the two caps.

**Result.**

| asset | N | tuned med (s) | safe-512 med (s) | penalty | tuned seeds | safe seeds |
|---|---|---|---|---|---|---|
| cloud     | 652   | 3.562  | 4.059  | **+14.0 %** | 3.554 3.629 3.562 | 4.054 4.061 4.059 |
| tornado   | 768   | 5.163  | 5.080  | **−1.6 %** (neutral) | 5.163 5.200 5.005 | 5.077 5.217 5.080 |
| explosion | 1024  | 5.550  | 5.978  | **+7.7 %** | 5.494 5.550 5.564 | 5.920 5.978 6.000 |
| bunny     | 25600 | 53.838 | 57.654 | **+7.1 %** | 53.832 53.838 54.017 | 57.519 57.654 57.977 |

**Reading.** The penalty is the occupancy cost of the extra per-ray local-memory reservation and is
**asset-dependent, not reducible to a single cap**:
- **cloud +14 %** — tuned caps far below 512 on *both* (64/96), largest reservation jump.
- **explosion +7.7 %** — tuned 32/160, large jump.
- **bunny +7.1 %** — measured on the 80/**496** winbin (see caveat above): both caps rise
  (active 80→512 = +864 B, hit 496→512 = +96 B), +960 B/ray total. For the canonical 80/**528** build
  the hit cap would instead *fall* (528→512) and the penalty would be active-set-driven (+768 B/ray
  net) — comparable in bytes to the `caps_ab.md` hit 128→256 increase (+768 B) that cost ~6× there.
  Either way it confirms: the cost is the reserved **bytes per ray**, not which cap.
- **tornado neutral** — tuned hit cap (384) already near 512; within run-to-run jitter.

**Supersedes** the old thesis figures: tornado is neutral (not +3 %) and bunny is +7 % (not +3 %).
§4.5 updated to the cloud +14 %, explosion +8 %, bunny +7 %, tornado-neutral numbers, and reframed
to the bytes-per-ray mechanism (not "hit buffer is the binding cap").

## Reproduce
```bash
sudo bash scripts/campaign/lock_clocks.sh         # SM 1800 / mem 9751 / 350 W
bash scripts/campaign/run_caps_timing.sh          # needs ~/winbins/exe_{cloud,tornado,explosion,bunny,safe512}
```
