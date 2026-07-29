r"""Monte-Carlo integration figure (Ch 2 background), two panels.

Estimates I = \int e^{-x^2/2} dx (true value sqrt(2*pi)) by the sample mean of f/p with p uniform
on [-L, L]. LEFT: one running estimate homing in on the true value, inside the +-1.96 sigma/sqrt(n)
band. RIGHT: the RMS error over many runs vs n on log-log, hugging a slope -1/2 reference -- the
"halve the error costs 4x the samples" fact made literal.

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/mc_integ.py --out latex/figures/mc_integ.pdf
"""
import argparse
import os

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")

L = 5.0                      # uniform sampling half-range; captures ~all of the mass
TRUE = np.sqrt(2.0 * np.pi)  # \int_{-inf}^{inf} e^{-x^2/2} dx


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--n", type=int, default=20_000, help="samples per run")
    ap.add_argument("--runs", type=int, default=256, help="runs for the RMS-error panel")
    ap.add_argument("--seed", type=int, default=20260618)
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    n = args.n
    idx = np.arange(1, n + 1)

    def integrand(x):  # f/p for p = U[-L, L]
        return (2.0 * L) * np.exp(-0.5 * x * x)

    # Population standard deviation of the integrand under p -> standard error sigma/sqrt(n).
    xs = np.linspace(-L, L, 200_001)
    g = integrand(xs)
    mean_g = np.trapezoid(g, xs) / (2.0 * L)
    sigma = np.sqrt(np.trapezoid((g - mean_g) ** 2, xs) / (2.0 * L))

    # Left: one running estimate. Right: RMS error across many independent runs.
    one = np.cumsum(integrand(rng.uniform(-L, L, n))) / idx
    runs = np.cumsum(integrand(rng.uniform(-L, L, (args.runs, n))), axis=1) / idx
    rms = np.sqrt(np.mean((runs - TRUE) ** 2, axis=0))

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, (axL, axR) = plt.subplots(1, 2, figsize=(8.0, 3.2))

    # --- Left: convergence to the value ---
    se = 1.96 * sigma / np.sqrt(idx)
    axL.fill_between(idx, TRUE - se, TRUE + se, color="C3", alpha=0.18,
                     label=r"$\pm1.96\,\sigma/\sqrt{n}$")
    axL.plot(idx, one, color="C0", lw=1.2, label="one running estimate")
    axL.axhline(TRUE, color="0.2", lw=1.3, label=r"true $\sqrt{2\pi}$")
    axL.set_xscale("log")
    axL.set_xlim(1, n)
    axL.set_ylim(TRUE - 1.15 * 1.96 * sigma, TRUE + 1.15 * 1.96 * sigma)
    axL.set_xlabel("number of samples $n$")
    axL.set_ylabel(r"estimate of $\int e^{-x^2/2}\,dx$")
    axL.set_title("Estimate converges to the value")
    axL.legend(loc="upper right", fontsize=8)

    # --- Right: the 1/sqrt(n) rate ---
    axR.loglog(idx, rms, color="C0", lw=1.8, label=f"RMS error ({args.runs} runs)")
    axR.loglog(idx, sigma / np.sqrt(idx), color="0.35", lw=1.1, ls="--",
               label=r"slope $-1/2$  ($\propto 1/\sqrt{n}$)")
    axR.set_xlim(1, n)
    axR.set_xlabel("number of samples $n$")
    axR.set_ylabel("RMS error")
    axR.set_title(r"Error falls as $1/\sqrt{n}$")
    axR.legend(loc="lower left", fontsize=8)

    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote {args.out}  (true={TRUE:.4f}, sigma={sigma:.3f}, "
          f"rms[n]/sigma*sqrt(n) end={rms[-1]/(sigma/np.sqrt(n)):.3f})")


if __name__ == "__main__":
    main()
