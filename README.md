# Vector Halftone Suite v0.7.0

Native Windows C++ Live Effects for Adobe Illustrator 2026.

The suite currently exposes two effects under **Effect > Vector Halftone**:

- **Simple Colour Halftone...** — direct editable vector halftone patterns driven by a gradient or the selected artwork/image.
- **Color Halftone...** — the Photoshop-style multi-channel Color Halftone recreation with vector output.

The older Vector Halftone / Halftone Gradient effects remain registered internally so existing documents created with earlier development builds can still resolve them, but they are not shown as new menu commands.

## Simple Colour Halftone

v0.7 replaces the previous opacity-mask interpretation with the workflow the Illustrator tutorial was actually using.

The tutorial uses a gradient, Color Halftone, Rasterize, Image Trace, Expand, Pathfinder Unite and `replaceItems.jsx` to recover clean circles. The plug-in skips those intermediary stages and creates the final clean vector marks directly.

### Source

**Size source**

- **Gradient** — use the procedural controls in the dialog.
- **Artwork / image** — raster-sample the rendered selected artwork only to determine tone/alpha; the generated output remains vector.

**Gradient controls**

- Linear / Radial
- Gradient angle
- Tone midpoint
- Tone scale
- Reverse

Darkness controls mark area, so mark diameter follows a square-root tone mapping similar to a traditional halftone screen rather than a purely linear diameter ramp.

### Pattern

- Shape: Circle, Square, Diamond, Triangle, Hexagon, Star or Line
- Grid spacing
- Grid angle
- Minimum size
- Maximum size
- Cull below
- Optional staggered rows

The defaults use a 45-degree grid and keep the largest marks separated, matching the intent of the tutorial after the dark end of its gradient is lightened.

### Output

- **Connecting stroke** with an explicit stroke width. The stroke uses the mark fill colour and can be increased until neighbouring shapes touch/connect.
- **Clip exactly to source** for vector path/compound-path inputs.
- **Preserve original appearance underneath** when the halftone should overlay the source rather than replace it.

Generated marks are real Illustrator vector paths. While the effect is live they are owned by the Live Effect result; use **Object > Expand Appearance** when you want to directly select/edit individual marks or export the expanded artwork as SVG.

### Colour behaviour

For procedural Gradient mode, solid source fills/strokes are inherited. Gradient/pattern/advanced source paints are treated as the size source rather than copied onto every dot, so generated marks fall back to a clean monochrome fill.

Artwork / image mode emits a monochrome vector screen driven by sampled luminance and alpha.

## Color Halftone

The Photoshop-style Color Halftone effect remains separate. It keeps the compact Photoshop-like dialog, document-unit display for Max Radius, calibrated channel-screen geometry and vector output.

## Backwards compatibility

Simple Colour Halftone render modes are retained internally:

- `mode=0` — v0.5 flat pattern
- `mode=1` — v0.6 opacity-mask shading experiment
- `mode=2` — v0.7 direct-vector gradient/artwork halftone

Existing artwork continues to render with its stored mode. Opening an older Simple Colour Halftone instance in the current dialog opts that instance into the v0.7 controls when the edit is committed.

## Requirements

- Windows 10/11 x64
- Adobe Illustrator 2026
- Adobe Illustrator 2026 C++ SDK
- Visual Studio C++ build tools with the v143 toolset
- Python 3.11 through the Windows `py` launcher

## Build

Place the project inside Illustrator SDK `samplecode` and run:

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

This source is designed for the Illustrator 2026 SDK. It has been source-level sanity checked here, but the authoritative compile/runtime test is the SDK build on a Windows machine with Illustrator installed.
