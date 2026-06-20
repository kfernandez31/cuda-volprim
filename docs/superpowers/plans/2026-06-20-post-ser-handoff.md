# Handoff — post-SER session (2026-06-20)

Continuity note for the next Claude. The **SER/Ada work is DONE and banked** — do not redo it. This
lists only what's left. Build via `latexmk` in `thesis/latex/` (must be clean before any thesis commit).
Nothing is committed; the user approves commit contents and owns push/merge/branch-delete.

## Durable records (read these first)
- `results/campaign/ser_ab.md` — all SER numbers + thesis framing.
- `docs/superpowers/plans/2026-06-18-ser-remote-experiment.md` — the SER runbook.
- `thesis/reviews/2026-06-17-plot-audit-open-items.md` — figure-audit open items.
- `~/.claude/plans/cuddly-gathering-sun.md` — the figure-audit change plan.
- `~/ser_experiment.sh` (local; also on box at `/root/ser_experiment.sh`) — the SER sweep script.

## DONE this session (do NOT redo)
- Figure audit: cut fig:bvh-dragon + fig:flicker; redrew fig:mc-integ (2-panel); cited gmms; fixed
  fig:pipeline (×N→×H + "eliminated by argmin" + de-jargon); rewrote tab:complexity (reference-only);
  trimmed tab:four-modes (3 rows); pulled denoiser from tab:wins + reframed §Denoising; reworked
  fig:scattering-ladder (running-mean); roofline → 4 assets (ncu); scaling-memory → 7 points;
  tab:vram Mitsuba filled incl. bunny (806), honest mixed-result prose; Condor → "lead author".
- **SER fully documented**: §6.9 "Shader Execution Reordering" + Table 6.5; cross-refs in §6.1, autopsies,
  §5 setup (4090 as scoped exception), Ch8 limitations + future-work; **abstract + intro + conclusion**
  all carry the scoped result. Build clean, 73 pp, 0 undefined.
- **SER results** (RTX 4090, Ada, wall-clock, image-identical): cloud 1.42×, explosion 1.41×, tornado
  1.12×, **bunny 1.68×**; hint ablation (8-cell 1.42× vs no-hint 1.25× vs bounce-0 1.10×); RIS 1.35×.

## Remote box (Vast.ai RTX 4090, Ada CC 8.9) — still rented unless destroyed
- Connect: `ssh -i ~/.ssh/vastai -o IdentitiesOnly=yes -o BatchMode=yes -p 58384 root@90.84.225.124`
- Bash needs `dangerouslyDisableSandbox: true` for network. **Bulk repo egress (rsync/scp) is
  classifier-BLOCKED for the agent** → the USER runs transfers via `!` (or grants a Bash permission rule).
  Reading remote state / running builds+renders over ssh works for the agent.
- Built on box: `build/` (SER-off), `build-ser/` (SER-on), per-asset `build-*` dirs; `~/optix` SDK;
  assets (cloud/bunny/tornado/explosion PLYs + meadow HDR); `oiiotool`, libglm/libtbb installed.
- **ncu is host-blocked** (`RmProfilingAdminOnly=1`, ERR_NVGPUCTRPERM) and **clock-lock is blocked** →
  wall-clock only (warmup + interleaved off/on, min+median). Mechanism counters unavailable here.

## OPEN ACTION ITEMS

### A. GPU, time-sensitive (box rented, billing) — DECIDE keep vs destroy first
1. If keep: **"ours+SER vs Mitsuba on the 4090, equal-quality"** — the biggest *defensible* number.
   - **Scene choice (revised):** cloud is lowest-risk (g10-style parity already validated). **Bunny is now
     eligible too** — committing to the single Gaussian fit dissolves the old "3 fits ambiguous" reason —
     and bunny is the biggest-SER asset (1.68×), so potentially the biggest number. BUT bunny has **no
     parity gate yet**: first verify ours ≈ Mitsuba in the converged mean on the Gaussian fit
     (`assets/models/unpacked/bunny_gauss_1024x24k/.../root.primitives_pyr0_sigmat.ply`, already generated),
     then do equal-quality timing. Cloud needs no such gate.
   - Method: render ours+SER and Mitsuba-analog to matched noise (or fixed spp + variance vs a converged
     reference), compare time → k·t. Mitsuba can't use SER (Dr.Jit emits no reorder points) — that's the point.

### B. Figure-audit leftover (LOCAL 3090, no rental)
2. **fig:rr-depth redo** — needs the user to run `sudo bash scripts/campaign/lock_clocks.sh`; then
   re-render the RR-depth sweep **incl. depth 14**, recompute k·t, replot, update §sec:wins basin numbers
   (currently min@12, basin ≤1.2%). `k` can reuse `results/campaign/rr_seeds/` EXRs; only timing is fresh.

### C. No-GPU polish
3. **SER per-asset bar figure** — new `scripts/plots/ser_speedup.py` from `ser_ab.md` (cloud 1.42,
   explosion 1.41, tornado 1.12, bunny 1.68; 1.0 baseline line) → `figures/ser_speedup.pdf`, add to §6.9.

### D. Pending decisions (user)
4. §7.6 scope — keep cross-renderer parity at 3 assets (cloud/tornado/explosion) or broaden.
5. Optional trims — fig:showcase ↔ fig:g1-bias visual overlap; fig:ris-noise (both lean keep).
6. tab:icosphere — also bold ℓ=2, or leave just the analytic row bolded.

### E. Thesis-text follow-up from this session
7. **Soften the bunny "excluded due to ambiguous fits" wording** (07-results.tex ~§7.1 "Beyond the
   cloud", and reconcile the tab:vram caption) → "not yet parity-checked against Mitsuba." We now keep one
   Gaussian fit, so fit-count is no longer the real reason. (If item A1 runs the bunny parity, drop the
   exclusion entirely.)

### F. Git / housekeeping (USER actions)
8. **Commit** the ~54 uncommitted files on `main`. Suggested split: SER device code
   (`device/core/constants.cuh`, `device/entry/raygen.cuh`, `cmake/OptiX-IR.cmake`) separate from the
   figure/thesis-writing edits. NO AI mentions / no Co-Authored-By in messages.
9. **Branch cleanup** — safe (no loss): `git worktree remove .claude/worktrees/cap-free-middle &&
   git branch -d feature/cap-free-middle`; `git branch -D feature/{env-is-alias-table,path-guiding,wavefront-phase1}`
   (duplicates of deprecated-* twins). KEEP the `deprecated-*` and `feature/cap-free-streaming` archives.
   Stray worktree: `.claude/worktrees/capfree-baseline` (detached) — remove if unneeded.

## Standing constraints (preserve)
- NO AI mentions in commit messages; no Co-Authored-By. git push / branch deletion / merge-to-main are
  the USER's actions. Do not run git gc/prune/reflog. latexmk clean before thesis commits.
- SER is a **cross-architecture probe**: keep it scoped ("on Ada"), never multiply across GPUs, never fold
  into the 3090 headline. Honest, non-overclaiming framing throughout. Thesis is pinned to the RTX 3090.
