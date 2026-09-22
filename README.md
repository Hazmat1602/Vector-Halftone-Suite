# Vector Halftone Suite v0.8.0

Native Windows C++ Live Effects for Adobe Illustrator 2026.

The suite exposes two new-effect commands under **Effect > Vector Halftone**:

- **Patterned Halftone...** — source-luminance-driven editable vector triangle halftone.
- **Color Halftone...** — Photoshop-style multi-channel Color Halftone recreation with vector output.

**Simple Colour Halftone** is no longer shown as a new menu command. Its internal effect registration and renderer remain in the plug-in so documents created with v0.5-v0.7 continue to resolve and render correctly.

## Patterned Halftone

Patterned Halftone uses the same general idea as Color Halftone — sample the rendered source artwork and convert tone into mark size — but it creates one clean vector shape per screen cell instead of separate process-colour circles.

v0.8 starts with an alternating triangle pattern inspired by triangular tessellation halftones:

- darker source areas produce larger triangles;
- lighter source areas produce smaller triangles;
- alternate cells flip the triangle 180 degrees;
- alternate rows are staggered to create a triangular lattice;
- the whole lattice rotates with **Pattern Angle**;
- output marks remain Illustrator vector paths.

### Controls

The dialog intentionally mirrors the compact Color Halftone window:

- **Max. Radius** — largest triangle circumradius.
- **Pattern Angle** — rotates the grid/pattern.
- **Min. Radius** — smallest generated triangle.
- **Spacing** — centre-to-centre screen spacing. Enter **0** for automatic spacing derived from Max. Radius.

The radius fields follow the current Illustrator document ruler units.

### Output

Patterned Halftone temporarily raster-samples the selected artwork at 72 PPI only to read luminance/alpha. The generated result itself is vector.

For path and compound-path inputs the result is clipped to the source silhouette. Transparent areas produce no marks.

While the effect remains live, the generated paths belong to the Live Effect result. Use **Object > Expand Appearance** when you want to select/edit individual triangles or export the expanded result as SVG.

## Color Halftone

Color Halftone remains the Photoshop-oriented effect with its compact dialog, document-unit Max Radius display, calibrated channel screens and vector output.

## Backwards compatibility

The following older effect IDs remain registered without menu items:

- Vector Halftone
- Halftone Gradient
- Simple Colour Halftone

That allows documents made with development versions v0.1-v0.7 to continue resolving their stored Live Effects.

## Requirements

- Windows 10/11 x64
- Adobe Illustrator 2026
- Adobe Illustrator 2026 C++ SDK
- Visual Studio C++ build tools with the v143 toolset
- Python 3.11 through the Windows `py` launcher

## Build

Place the project directly inside the Illustrator SDK `samplecode` folder and run:

```powershell
.\build.ps1
```

Expected output:

```text
<SDK>\samplecode\output\win\x64\Release\VectorHalftoneEffect.aip
```

## Install

Close Illustrator, replace the existing plug-in in Illustrator's Plug-ins folder with the newly built `.aip`, then restart Illustrator.

## Development status

The source has been sanity checked at source level, but the authoritative compile/runtime test is the Illustrator 2026 SDK build on Windows.
