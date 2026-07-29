# 18 — Startup and iteration latency (thesis §7.3)

**Claim.** Ahead-of-time compilation starts the renderer in ~0.39 s — about half the
reference's warm start and a fifth of its cold start; the advantage is pipeline setup,
independent of the asset.

**Protocol** (self-contained in thesis §7.3): for each renderer, 3 fresh processes;
startup = first-render wall time minus steady-state render time; reference measured
twice — with a warm Dr.Jit kernel cache and with the cache cleared (cold).
```
# ours: time a 1-spp first render vs a steady-state repeat, 3 fresh processes
build/bin/Release/test_runner --scene cloud_asset_scattering --spp 1 ...
# reference: same protocol via ../mitsuba-reference/render_via_volprim.py;
# cold = rm -rf ~/.drjit (kernel cache) between processes
```

**Expected.** Ours 0.39 s (range 0.39–0.45 across processes); reference warm ≈ 2x,
cold ≈ 5x ours (exact values in §7.3). `[gpu][mitsuba]`
