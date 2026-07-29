# 16 — Reference corrections and certification (thesis Fig 7.2, §7.2, Appendix A)

**Claim.** The shipped reference's fast estimator over-counts (reference-free furnace
evidence); the current upstream revision carried five issues, each certified by a
deterministic, parameter-free probe; with the fixes the two renderers agree at noise
level. Fixes submitted upstream (PR; thesis §7.2).

**Run.** Every probe is parameter-free and lives in `../mitsuba-reference/`:
```
python3 ../mitsuba-reference/probe_mirror.py           # B-sign mirrored integral
python3 ../mitsuba-reference/probe_shipped_additivity.py   # release kernel additivity
# chain furnace, exact predictor, quadrature checks: see mitsuba-reference/README.md
bash scripts/campaign/run_nee_fix_acceptance.sh        # post-fix acceptance ladder
```

**Expected.** Additivity factor 1.0000 (release kernel); mirror probe reproduces the
B-sign defect; chain-furnace MRE +4.2pp -> +0.015pp with the 4-line fix; post-fix cloud
NEE 0.3225 vs truth 0.321. All deterministic — no seeds, no tolerance judgment calls.
