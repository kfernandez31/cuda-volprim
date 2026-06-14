# G2 merge-ladder A/B — INCONCLUSIVE (old-commit re-measurement), 2026-06-14

**Outcome: the contemporaneous dev §-numbers in `tab:wins` stand; a post-hoc isolated re-measurement
via the merge commits is not reliable.** 350 W, clocks held at 1800 MHz (min 1770, p50/max 1800).
Driver: `scripts/campaign/run_g2_ladder.sh`; data `g2_ladder.csv`.

## Method
Each optimisation's win = equal-spp time ratio (these changes are image-preserving) between its merge
commit (after) and the commit before it (before), built in git worktrees and rendered against pre-reorg
asset symlinks (`assets/cloud`, `assets/<env>.hdr`). cloud-meadow, 64 spp, 5 interleaved rounds.

| pair | before | after | ratio | verdict |
|---|---|---|---|---|
| shadow-transmittance (9dfa6de~1 ↔ 9dfa6de) | 0.014 s | 0.014 s | 1.00 | **invalid** |
| anyhit-fusion (9dfa6de ↔ 71ced87) | 0.013 s | 0.013 s | 1.00 | **invalid** |
| dedup-bounce0 (3b08b0c ↔ f54deaa) | 19.41 s | 19.48 s | 0.997 | ~0 % full-frame |
| skip-scan (f54deaa ↔ 174777d) | 19.33 s | 19.30 s | 1.002 | ~0 % full-frame |

## Why it doesn't reproduce the dev numbers
1. **Early commits render the production cloud trivially.** 9dfa6de/71ced87 load all 652 primitives +
   the meadow env but "render" 8 spp in ~17 ms — the full multi-bounce scattering path is not
   functioning at that vintage (rays escape immediately). Both arms of the shadow/fusion pairs are
   therefore ~14 ms and the ratio is a meaningless 1.00. (Builds fine; the *scene* is incompatible —
   like the wavefront branch's runtime fault, an old-commit-vs-current-assets mismatch.)
2. **The measurable commits run at pre-calibration caps/config.** dedup/skip-scan render at ~19.3 s for
   64 spp (vs ~9 s calibrated), and at that operating point the per-optimisation win is diluted to
   within run-to-run noise. This matches the standing caveat that the skip-scan win is cap-dependent
   (`incremental-active-prims ~16 %`, §8.23, "may shrink at the smaller active cap"), and that
   bounce-0/per-bounce-scan savings are a tiny full-frame fraction at 64 spp.

## Conclusion
A clean isolated A/B is not recoverable from the merge commits: the optimisations compound, the config
drifted between each commit and the production point, and the early commits cannot drive the current
scene. The honest record is the **contemporaneous measurements** (`tab:wins`: shadow-ray
\~12–15× shadow-kernel §8.16, skip-scan \~16 % frame §8.23, dedup \~8 % §8.19, fusion \~3 % §8.18),
each taken at the time in its own context. `tab:wins` is left as-is; this re-measurement is documented
as a negative (cf. the wavefront re-run, also blocked by an old-branch/current-toolchain mismatch).
Worktrees removed after the run; commits preserved.
