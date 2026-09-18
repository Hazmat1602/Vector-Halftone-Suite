# Photoshop Color Halftone calibration

This build uses measurements from Photoshop 2026 output generated with the native Color Halftone filter.

## Recovered screen model

For Max Radius `R`:

- square-cell side: `S = R * sqrt(2)`
- screen origin: image centre `(width - 1)/2, (height - 1)/2`
- screen rotation in raster coordinates:
  - `u = dx cos(a) + dy sin(a)`
  - `v = -dx sin(a) + dy cos(a)`
- each source pixel belongs to its nearest screen-cell centre
- the average channel value in that cell sets the requested channel coverage

For RGB channel value `V` in `[0,1]`, dark coverage is `D = 1 - V`.

When `D <= pi/4`, the dot fits entirely inside the cell:

`r = S * sqrt(D / pi)`

For darker coverage, `r` is found by solving the area of a circle intersected with the square cell. At `D = 1`, `r = R` and adjacent dots meet at cell corners.

## RGB vector equivalent

- R-channel dark dots → Cyan
- G-channel dark dots → Magenta
- B-channel dark dots → Yellow
- blend mode → Multiply
- backdrop → White

This reproduces the channel combinations seen in Photoshop's RGB Color Halftone output while keeping the final Illustrator art vector.
