"""
Closed-form analytic comparator for the single_gaussian_validation scene.

For one isotropic axis-aligned Gaussian at origin with scale = (1, 1, 1) and
mass-normalised optical_thickness M, an orthographic view along +Z produces a
ray at perpendicular distance d = sqrt(px^2 + py^2) from the Gaussian center.
The line-integrated optical depth is closed-form:

    tau(d) = M / (2 * pi) * exp(-d^2 / 2)

Per-pixel rendered intensity has two competing hypotheses:

    H_analog : exp(-tau(d)) * env       # analog free-flight (Jorge's volprim_prb)
    H_double : exp(-2 * tau(d)) * env   # current code may double-count exp(-tau)
                                          on escape (Finding 1 of the systematic review)

This script reads the rendered EXR plus the sigma_multiplier it was rendered at
(which equals the primitive's peak extinction; M is then
sigma_multiplier * (2*pi)^(3/2) for scale=(1,1,1)), reconstructs both hypothesis
images, and reports RMSE / MAE / max-error against each. Whichever hypothesis
the render matches more closely tells us whether Finding 1 is real.

Run from the project root:

    tools/refs/.venv/bin/python tools/refs/single_gaussian_analytic.py \\
        single_gaussian_validation/0000.exr --sigma 1.0
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
import OpenEXR
import Imath


# Must match scenes/single_gaussian.cpp.
ORTHO_HEIGHT = 6.0
WIDTH = 256
HEIGHT = 256
ENV_RADIANCE = 1.0  # assets/white_constant.hdr is unit white


def load_exr(path: Path) -> np.ndarray:
    f = OpenEXR.InputFile(str(path))
    dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    chans = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack(
        [np.frombuffer(c, dtype=np.float32).reshape(sz) for c in chans], axis=-1
    )


def perpendicular_distance_grid(width: int, height: int, extent: float) -> np.ndarray:
    """
    For each pixel center, return d = sqrt(px^2 + py^2), where (px, py) is the
    world-space position of the pixel on the view plane z=0.

    The orthographic camera spans [-extent/2, extent/2] in y and (aspect-scaled)
    in x. With width == height the viewport is square, so x and y span the same.
    """
    aspect = width / height
    view_w = extent * aspect
    view_h = extent

    # Pixel-center positions; sign / orientation does not matter — d is symmetric.
    js = np.arange(width, dtype=np.float64)
    is_ = np.arange(height, dtype=np.float64)
    px = (js + 0.5) / width * view_w - 0.5 * view_w
    py = (is_ + 0.5) / height * view_h - 0.5 * view_h
    PX, PY = np.meshgrid(px, py)
    return np.sqrt(PX * PX + PY * PY)


def analytic_tau(d: np.ndarray, M: float) -> np.ndarray:
    """tau(d) = M / (2*pi) * exp(-d^2 / 2) for unit isotropic Gaussian, scale=(1,1,1)."""
    return (M / (2.0 * math.pi)) * np.exp(-0.5 * d * d)


def report(label: str, residual: np.ndarray) -> None:
    rmse = float(np.sqrt(np.mean(residual * residual)))
    mae = float(np.mean(np.abs(residual)))
    mx = float(np.max(np.abs(residual)))
    print(f"  {label}: RMSE={rmse:.6e}  MAE={mae:.6e}  max|err|={mx:.6e}")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("exr", type=Path, help="Path to rendered EXR")
    p.add_argument("--sigma", type=float, required=True,
                   help="sigma_multiplier the render was produced with "
                        "(= primitive peak extinction)")
    p.add_argument("--env", type=float, default=ENV_RADIANCE,
                   help="Constant env radiance (default 1.0)")
    args = p.parse_args()

    img = load_exr(args.exr)
    h, w, _ = img.shape
    if (h, w) != (HEIGHT, WIDTH):
        print(f"WARN: render is {w}x{h}, expected {WIDTH}x{HEIGHT}. "
              "Analytic mapping still uses pixel-centred viewport so RMSE is meaningful.")

    # In the Mitsuba volprim_tomography convention used everywhere now, the
    # per-primitive `sigma_t` IS the total integrated mass M. No (2π)^{3/2} bridge.
    # See src/thesis/host/utils/io/ply.cpp and test/scenes/single_gaussian.cpp.
    M = args.sigma

    d = perpendicular_distance_grid(w, h, ORTHO_HEIGHT)
    tau = analytic_tau(d, M)

    H_analog = np.exp(-tau) * args.env
    H_double = np.exp(-2.0 * tau) * args.env

    # Average across RGB channels — env is grey and primitive is grey.
    rendered_grey = img.mean(axis=-1)

    print(f"\nSingle-Gaussian verification — sigma_multiplier={args.sigma}, M={M:.4f}")
    print(f"  Render: {args.exr}  ({w}x{h})")
    print(f"  Mean rendered intensity: {rendered_grey.mean():.6e}")
    print(f"  Max tau(d): {tau.max():.6f}")
    print()
    print("Residuals (rendered - hypothesis):")
    report("H_analog (exp(-tau)*env)    ", rendered_grey - H_analog)
    report("H_double (exp(-2*tau)*env)  ", rendered_grey - H_double)

    rmse_a = float(np.sqrt(np.mean((rendered_grey - H_analog) ** 2)))
    rmse_d = float(np.sqrt(np.mean((rendered_grey - H_double) ** 2)))
    verdict = "H_analog (escape factor is correct)" if rmse_a < rmse_d \
              else "H_double (Finding 1 confirmed — escape double-counts exp(-tau))"
    print(f"\nNearer hypothesis: {verdict}")
    print(f"  ratio rmse_double / rmse_analog = {rmse_d / max(rmse_a, 1e-30):.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
