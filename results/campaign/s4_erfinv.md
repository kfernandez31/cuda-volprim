# S4 — heavy-overlap residual: erf^-1 precision ruled out (2026-06-21)

Discriminating test for Condor review item S4 (`scripts/campaign/run_s4_erfinv.sh`):
cluster_validation, albedo 0.9, sigma 2, 16384 spp, 2 seeds. The shipped single-precision `erfinvf`
(build/) vs a double-precision `::erfinv` build (build-hiprec, `THESIS_HIPREC_ERFINV`) — identical
seeds, differing ONLY in the inverse-CDF precision.

| metric | value |
|---|---|
| mean (float32) | 0.985371 |
| mean (double erfinv) | 0.985371 |
| mean difference | +0.0 |
| `|diff|` overlap centre (1/4 box) | 2.4e-8 |
| `|diff|` max pixel | 4.0e-5 |

**Decision:** erf^-1 precision contributes ~2e-8 at the overlap, **four orders of magnitude below** the
+2e-4 heavy-overlap cluster residual. So the residual is **NOT** a single-precision inversion artefact
(this rules out Condor's S4 hypothesis with a direct measurement). The cause is an overlap-regime effect:
the argmin free-flight independence assumption under heavy overlap, or next-event shadow-ray transmittance
from a vertex inside overlapping primitives. The shipped single-precision `erfinvf` is validated as
adequate (the fp64 inverse would cost ~1/64 rate on this hot line for no measurable accuracy gain).
