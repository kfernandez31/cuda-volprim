"""
Diff two directories of EXR renders. Outputs per-frame and aggregate metrics.

Pairs frames by basename (0000.exr in dir A vs 0000.exr in dir B). Skips frames
not present in both sides.

Run from project root:
    tools/refs/.venv/bin/python tools/refs/compare_renders.py \\
        assets/cloud/refs_voxel_self/ \\
        test_results/cloud_asset_validation/
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import OpenEXR
import Imath


def load_exr(path: Path) -> np.ndarray:
    """Return HxWx3 float32 image (R, G, B)."""
    f = OpenEXR.InputFile(str(path))
    dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    chans = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    arr = np.stack([np.frombuffer(c, dtype=np.float32).reshape(sz) for c in chans], axis=-1)
    return arr


def save_exr(path: Path, arr: np.ndarray) -> None:
    """Write HxWx3 float32 as an RGB EXR."""
    h, w, _ = arr.shape
    arr = arr.astype(np.float32)
    header = OpenEXR.Header(w, h)
    header["channels"] = {c: Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT))
                          for c in ("R", "G", "B")}
    out = OpenEXR.OutputFile(str(path), header)
    out.writePixels({c: arr[..., i].tobytes() for i, c in enumerate("RGB")})
    out.close()


def phase_shift(a: np.ndarray, b: np.ndarray) -> tuple[int, int]:
    """Integer-pixel shift maximizing phase correlation of a vs b."""
    G = np.fft.fft2(a); B = np.fft.fft2(b)
    R = G * np.conj(B)
    R /= np.abs(R) + 1e-10
    ifft = np.real(np.fft.ifft2(R))
    py, px = np.unravel_index(np.argmax(ifft), ifft.shape)
    h, w = ifft.shape
    dy = py - h if py > h // 2 else py
    dx = px - w if px > w // 2 else px
    return int(dy), int(dx)


def metrics(ref: np.ndarray, test: np.ndarray, silhouette_thresh: float) -> dict:
    diff = ref - test
    abs_diff = np.abs(diff)
    rmse = float(np.sqrt(np.mean(diff ** 2)))
    mae = float(np.mean(abs_diff))
    ref_lum = ref.mean(-1)
    test_lum = test.mean(-1)
    ref_sil = ref_lum < silhouette_thresh
    test_sil = test_lum < silhouette_thresh
    inter = (ref_sil & test_sil).sum()
    union = (ref_sil | test_sil).sum()
    iou = float(inter / union) if union > 0 else 1.0
    dy, dx = phase_shift(ref_lum, test_lum)
    return {
        "rmse": rmse,
        "mae": mae,
        "iou": iou,
        "phase_shift": (dy, dx),
        "ref_dark_pct": float(100 * ref_sil.mean()),
        "test_dark_pct": float(100 * test_sil.mean()),
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("ref_dir", type=Path, help="Reference EXR directory")
    p.add_argument("test_dir", type=Path, help="Test EXR directory")
    p.add_argument("--silhouette-thresh", type=float, default=0.95,
                   help="Luminance threshold for the 'dark / cloud' silhouette mask")
    p.add_argument("--out", type=Path, default=None,
                   help="If set, write per-frame |ref - test| EXRs into this dir.")
    args = p.parse_args()

    ref_files = {f.name: f for f in sorted(args.ref_dir.glob("*.exr"))}
    test_files = {f.name: f for f in sorted(args.test_dir.glob("*.exr"))}
    common = sorted(set(ref_files) & set(test_files))
    if not common:
        raise SystemExit(f"No matching EXR pairs between {args.ref_dir} and {args.test_dir}")

    only_ref = sorted(set(ref_files) - set(test_files))
    only_test = sorted(set(test_files) - set(ref_files))
    if only_ref:
        print(f"Skipping {len(only_ref)} frames only in ref: {only_ref[:3]}{'...' if len(only_ref)>3 else ''}")
    if only_test:
        print(f"Skipping {len(only_test)} frames only in test: {only_test[:3]}{'...' if len(only_test)>3 else ''}")

    if args.out is not None:
        args.out.mkdir(parents=True, exist_ok=True)

    print(f"\n{'frame':>10} {'RMSE':>8} {'MAE':>8} {'IoU':>6} {'shift':>10} {'ref%':>6} {'test%':>6}")
    print("-" * 64)

    agg_rmse = []
    agg_mae = []
    agg_iou = []
    for name in common:
        ref = load_exr(ref_files[name])
        test = load_exr(test_files[name])
        if ref.shape != test.shape:
            print(f"{name:>10} SHAPE MISMATCH ref={ref.shape} test={test.shape}")
            continue
        m = metrics(ref, test, args.silhouette_thresh)
        agg_rmse.append(m["rmse"])
        agg_mae.append(m["mae"])
        agg_iou.append(m["iou"])
        dy, dx = m["phase_shift"]
        print(f"{name:>10} {m['rmse']:>8.4f} {m['mae']:>8.4f} {m['iou']:>6.3f} "
              f"({dy:>+3},{dx:>+3})  {m['ref_dark_pct']:>5.1f}% {m['test_dark_pct']:>5.1f}%")

        if args.out is not None:
            save_exr(args.out / name, np.abs(ref - test))

    print("-" * 64)
    print(f"{'aggregate':>10} {np.mean(agg_rmse):>8.4f} {np.mean(agg_mae):>8.4f} {np.mean(agg_iou):>6.3f}")


if __name__ == "__main__":
    main()
