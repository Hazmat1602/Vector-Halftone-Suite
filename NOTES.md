# v0.7.0 engineering notes

## Simple Colour Halftone redesign

The v0.6 opacity-mask implementation did not match the referenced Illustrator tutorial. v0.7 introduces `mode=2`, a direct-vector renderer that models the useful end result instead of reproducing the tutorial's temporary raster/trace steps.

### Direct-vector pipeline

1. Establish a shared regular grid over the source bounds.
2. Rotate the grid by `gridAngle` and optionally stagger alternate rows.
3. Determine tone from either a procedural linear/radial gradient or a one-time ARGB sample of the rendered source artwork.
4. Convert tone to mark diameter with square-root/area behaviour.
5. Cull tiny marks.
6. Create the selected vector mark shape directly with `VHCreateMark`.
7. Optionally add a same-colour connecting stroke.
8. Optionally clip the marks to the source path/compound path.

No Image Trace, Pathfinder Unite or replacement-script stage is required.

## Source artwork/image sampling

Artwork mode rasterizes the input to temporary 72-PPI ARGB only as a tone/alpha lookup surface. The raster is disposed before the Live Effect returns. Output marks remain vector.

Tone is based on RGB luminance and alpha. Transparent pixels produce no marks.

## Connecting stroke

`VHCreateMark` now accepts an optional explicit stroke width. Normal halftone renderers still suppress repeated source strokes; Simple Colour Halftone can explicitly request a same-fill-colour stroke to make neighbouring shapes connect.

## Compatibility

- mode 0 = v0.5 flat pattern
- mode 1 = v0.6 opacity-mask experiment
- mode 2 = v0.7 direct vector

Legacy dictionary keys are still read/written. New keys include source mode, min/max/cull size, connecting-stroke toggle and stroke width.

## Build validation

The package cannot be linked against the user's Illustrator 2026 SDK in this environment. Run `./build.ps1` in the SDK samplecode project and report any compiler diagnostics for SDK-specific adjustments.
