# 13 — Per-asset cap calibration (thesis Tab 6.7, §6.7)

**Claim.** The offline estimator predicts per-asset worst-case 3-sigma overlap and hit
counts; --measure-caps measures them in-render; calibrated caps cut device memory
25–52 % vs the safe build (Tab 7.3).

**Run.**
```
build/bin/Release/test_runner ... --measure-caps       # in-render maxima [gpu]
bash scripts/tools/calibrate_caps.sh <asset>           # estimator + rebuild
```

**Expected.** Tab 6.7 estimates vs measured (estimates carry no guarantee — the bunny
worst chord was under-estimated ~20 %, stated in the thesis); measured caps per asset:
cloud 64/96, tornado 112/384, explosion 32/160, bunny 80/528.
