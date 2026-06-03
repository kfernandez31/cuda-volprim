# Ship Plan — code + master thesis by end of month (+ε)

**Created:** 2026-06-03  **Hard deadline:** ~2026-06-30 (+ a few days, target 2026-07-03)
**Two deliverables:** (1) shipped renderer code, (2) the written master thesis.
**Operating mode:** Claude in auto/24-7 on branches; user reviews + approves merges
(per CLAUDE.md) and owns thesis intellectual content + go/no-go calls.

## Guiding principles (read first)
1. **The thesis is the gating deliverable.** Code is ALREADY shippable: validated
   end-to-end, provably correct (FINDINGS §8, agreement to ~1e-5). Correctness is done.
2. **`main` stays always-shippable** = validated renderer + only low-risk, ladder-passed
   improvements. Never bet the validated renderer on a speculative rewrite.
3. **The validation ladder is the regression gate.** ANY perf change must reproduce:
   furnace flat (≤2e-3 @ modest spp), single/cluster/cloud systematic ≤ ~1e-4 vs Mitsuba
   analog. This auto-catches bias — run it after every perf change before merge.
4. **Perf is upside, not a requirement.** A rigorous validation + honest perf analysis is
   already a complete, strong thesis. Beating Mitsuba is NOT required to pass.
5. Order perf by certainty, not glamour: contained+certain first, big-bet rewrite last.

## Regression gate (the safety net) — automate this first
Script `tools/refs/validate_ladder.sh` (TO WRITE): renders CUDA + compares vs stored
Mitsuba-analog refs for furnace(α=1), single(α=0.9), n5, traits, cloud cam0; prints
systematic per rung; FAILS if any exceeds threshold. This is the gate for every merge.

---

## Schedule

### Week 1 (Jun 3–9): certain wins + thesis spine + gate
- [ ] **Profile as-is** (ncu): occupancy, regs/thread, LMEM spill+traffic, per-kernel
      time; instrument max-hits-per-ray histogram for the cloud. → grounds everything.
- [ ] **Write `validate_ladder.sh`** (the regression gate). Store reference numbers.
- [ ] **B1 — right-size per-ray buffers** (HIT_BUFFER_CAPACITY / MAX_ACTIVE_PRIMS) to the
      measured max-hits (+margin). Re-time, run gate. Low risk, proven bottleneck.
- [ ] **maxRegisterCount sweep** (module.h=96, renderer.cpp=DEFAULT) after buffers shrink.
- [ ] **Thesis: methods + validation chapters** — draft from existing figures (furnace,
      ladder, systematic decomposition, Mitsuba-NEE finding). All material exists.
- DELIVERABLE: faster renderer (low-risk) + thesis spine drafted + automated gate.

### Week 2 (Jun 10–16): the equal-quality lever + thesis body + wavefront Phase 1
- [ ] **A1 — per-step Rao-Blackwellization** (PRIMARY perf bet, more contained than
      wavefront): extend analytic transmittance-folding from bounce-0 to all bounces.
      Attacks the BIGGER factor (2.85× variance vs wavefront's ~1.5× throughput) and is a
      focused change to scatter accumulation (machinery exists in compute_transmittance_to_env).
      MEASURE net equal-quality (may cost per-sample throughput). Re-validate via gate.
- [ ] **Wavefront Phase 1** on `feature/wavefront`: global ray state + host bounce loop
      (NO kernel split yet). This is the make-or-break MEASUREMENT (plan risk #1: global-mem
      traffic may eat the gain). Gate must still pass.
- [ ] **★ GO/NO-GO on wavefront (by Jun 16):** continue ONLY if Phase 1 overhead is
      acceptable AND thesis draft is on track. Else STOP and write it up as a negative
      result (a legit thesis finding: "wavefront on the OptiX megakernel model is bottlenecked
      by global ray-state traffic").
- [ ] **Thesis: results + performance chapters** (incl. the speed analysis, RB results).

### Week 3 (Jun 17–23): finish perf (gated) + thesis writing
- [ ] **If GO:** wavefront Phases 2–3 (extract escape CUDA kernel, tune launch_bounds,
      profile, measure wall-clock). Gate after each step.
- [ ] **If NO-GO:** polish — more cloud cameras, the §8.3 overlap-residual investigation
      writeup, extra ablations for the thesis.
- [ ] **Thesis: discussion, related work, intro, conclusion.**

### Week 4 (Jun 24–30+): freeze, finalize, ship
- [ ] **Code freeze ~Jun 27.** Merge to main ONLY what passed the gate + showed real
      speedup. Wavefront merges only if clean + faster; else stays on branch, documented.
- [ ] **Thesis: finalize**, figures, proofread, references.
- [ ] **Ship** code + thesis.

---

## What runs in auto vs needs the user
- AUTO (Claude 24/7): profiling, renders, ablations, the regression gate, code iteration
  on branches, drafting prose/figures/tables, the wavefront implementation attempts.
- USER (gating): architecture decisions, commit/merge approval (CLAUDE.md), thesis
  intellectual content + final wording, the Jun 16 wavefront go/no-go call.

## Risk register
- Wavefront may not speed up at all (global-mem traffic) → Phase-1 measurement + go/no-go.
- Perf change silently biases → caught by the regression gate (the ladder).
- Thesis time squeezed by perf work → perf is upside; thesis time is protected, perf yields.
- Context/SSH loss during long runs → detached (setsid/nohup) + idempotent resumable scripts.

## Definition of done
- main = validated renderer (gate green) + low-risk speedups, tagged + shippable.
- Thesis complete: method, validation (the spine), honest performance analysis (+ wavefront
  result OR negative-result writeup).
- Open items documented (FINDINGS §8.3 overlap residual; TODO.md leftover knobs).

See TODO.md for the per-lever technical detail (B1/B2/A1/A2/wavefront) and FINDINGS.md §8 for
the validation evidence this plan rests on.
