# Thesis writing conventions

Quick-reference so every chapter stays consistent (notation, LaTeX, voice). Kept deliberately
short. Adapted from patterns in academic-writing Claude setups — the "always-on rules" idea.

## Voice & style
- Third person, present tense, active, terse. Cut filler ("it is important to note", "very", "in order to").
- Define a term/symbol **once**, then use it.
- **Foreshadow the contribution:** where a Background/Related-Work concept sets up our method, end the
  paragraph with a pointer — e.g. "…the lever behind the sort-free scatter sampling in \Cref{ch:architecture}".
- **Past tense only** for reporting what was done/measured ("we measured…", "the build produced…").
  Everything else is present tense.

## Notation (use consistently)
- Vectors via `\bm{}`: `\bm{r}, \bm{o}, \bm{d}, \bm{x}, \bm{\omega}`.
- Extinction `\sigma_t`, scattering `\sigma_s`, absorption `\sigma_a`; single-scatter albedo `\alpha`.
- Optical depth `\tau`; transmittance `T`; HG anisotropy `g`; uniform sample `\xi`.
- Error function: `\operatorname{erf}` (the analytic optical depth).
- Counts: `N` total primitives · `A` active/overlapping · `H` entry hits · `D` path depth ·
  `K` RIS candidates · spp = samples per pixel.

## LaTeX
- **Cross-refs (cleveref):** use `\Cref{}` **everywhere** as house style (capitalised
  "Chapter/Section/Figure N", mid-sentence included; the loaded `capitalise` option keeps it
  consistent). Label prefixes: `ch:`, `sec:`, `fig:`, `tab:`, `eq:`, `app:`.
- **Tables:** `booktabs` (`\toprule \midrule \bottomrule`); no vertical rules.
- **Units & numbers (siunitx):** `\SI{150}{\watt}`, `\SI{2.1}{\milli\second}`, `\SI{30}{\giga\byte}`,
  `\num{24576}`; speedups as `\(1.4\times\)`.
- **Citations:** `\cite{}` (plain numbered style — current). Keep the bibliography small and defensible
  (only cite what can be defended at the viva).
- **Equations:** `\[ … \]` for display; use a numbered `equation` env + `\label{eq:…}` only when referenced.
- **Figures:** generated as PDF into `figures/` via `scripts/plots/figure_from_csv.py`; include with a
  `\label{fig:…}` and a caption that states the takeaway, not just "the cloud".

## Build
- `cd thesis/latex && latexmk -pdf thesis.tex` (runs the bibtex passes automatically). Zero `!` errors
  before any commit.
