# v0.8.0 engineering notes

## Patterned Halftone

v0.8 removes **Simple Colour Halftone** from the visible Effect menu and introduces a dedicated **Patterned Halftone** command.

The old Simple Colour Halftone Live Effect ID remains registered without a menu item so existing v0.5-v0.7 documents keep rendering.

### Triangle renderer

Patterned Halftone currently fixes the mark family to triangles.

1. Raster-sample the rendered source appearance to an ARGB lookup surface.
2. Evaluate luminance/alpha at each pattern cell.
3. Convert darkness to radius with square-root/area behaviour.
4. Lay cells out on a staggered regular grid.
5. Alternate triangle orientation by 180 degrees from cell to cell.
6. Rotate the entire pattern with the Pattern Angle setting.
7. Emit clean Illustrator vector paths and clip to vector source silhouettes where possible.

The implementation reuses the v0.7 direct-vector luminance sampler internally rather than duplicating the raster lookup pipeline.

## Compact dialog

Patterned Halftone intentionally follows the Photoshop-style **Color Halftone** dialog:

- Max. Radius
- Pattern Angle
- Min. Radius
- Spacing (0 = auto)
- OK / Cancel

The same anti-aliased owner-drawn button renderer is used.

## Compatibility

Hidden registrations retained:

- legacy Vector Halftone
- legacy Halftone Gradient
- Simple Colour Halftone

No existing internal effect ID was renamed.

## Build validation

Run `.\build.ps1` from the project folder inside the Illustrator 2026 SDK `samplecode` directory. Any SDK/compiler diagnostics should be treated as the next validation step.
