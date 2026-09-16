# Vector Halftone — Illustrator Live Effect v0.3.4

A native Windows C++ Live Effect for Adobe Illustrator 2026. It converts the edge of a closed vector path into a live vector halftone while keeping the Illustrator Appearance effect non-destructive.

The internal Live Effect identifier remains unchanged, so artwork created with the earlier v0.1/v0.2 builds can continue to resolve the effect.

## What is new in v0.3.4

- **Illustrator-style settings dialog.** The dialog now queries `AIUIThemeSuite` for Illustrator's actual dialog background, text and edit-field colours and follows Illustrator's light/dark UI brightness.
- Replaced chunky Windows group boxes with compact Illustrator-style section headings, dividers, tighter spacing, units beside numeric fields and a conventional bottom action bar.
- **Make centre solid** checkbox for inward halftones. The chosen mark shape is kept through the fade, then the fully-dark core switches to overlapping source-colour vector tiles so there are no pinholes in the centre.
- **Generated colour always follows the source object's fill.** The Black / Source Fill colour selector has been removed. Existing documents with the old colour parameter remain compatible, but v0.3 ignores it and uses the source fill.
- Presets, live preview, last-used settings, inward/outward mode, mark shapes, exact clipping and compound-path support from v0.2 remain available.

## Workflow

1. Select a closed path or compound path.
2. Choose **Effect > Vector Halftone > Vector Halftone...**
3. Pick a preset or adjust the settings with **Preview** enabled.
4. For artwork like an Inner Glow, enable **Make centre solid** if you want the halftone transition only around the edge and a continuous fill in the middle.
5. Click **OK**.
6. Edit the original path. Illustrator reruns the Live Effect automatically.
7. Double-click **Vector Halftone** in the Appearance panel to edit it again.

## Settings

### Glow

- **Direction:** Inward from edge / Outward from edge
- **Width:** distance over which the edge transition occurs
- **Strength:** amount of edge transition mixed into mark size
- **Softness:** smoothness of the transition
- **Fade curve:** gamma-style shaping of the edge profile
- **Invert tone:** reverses large/small marks
- **Make centre solid:** for inward mode, converts the region beyond Glow Width into overlapping vector fill while preserving the halftone transition near the edge

### Halftone

- **Mark shape:** Circle, Square, Diamond, Triangle, Hexagon, Star, Line
- **Spacing:** grid spacing
- **Minimum / Maximum size:** generated mark-size range
- **Screen angle:** rotates the halftone grid
- **Stagger rows:** offsets alternate rows
- **Curve quality:** samples used when flattening Bezier segments
- **Colour:** always inherited from the source object's fill

### Output

- **Clip inward halftone exactly to source:** creates a real Illustrator clipping group and preserves compound-path holes
- **Preserve original appearance underneath:** includes the effect-input appearance below the generated halftone

Exact clipping is intentionally available only for **Inward** mode. Outward mode generates marks outside the source path up to Glow Width.

## Illustrator UI integration

The Windows dialog uses Illustrator's `AIUIThemeSuite` to query the current dialog background, text and edit-field colours. It also follows Illustrator's dark/light state for native Windows controls and the title bar. This keeps the plug-in visually much closer to built-in Illustrator effect dialogs while retaining the stable native Live Effect implementation.

## Backwards compatibility

The internal effect name remains:

```text
com.hazmat.vectorhalftone.innerglow
```

Old `containDots` and `colorMode` dictionary keys are still read/written for compatibility. `colorMode` is now forced to source fill colour. The new solid-centre setting is stored as `vigh.solidCenter` and defaults off for existing artwork.

## Requirements

- Windows 10/11 x64
- Adobe Illustrator 2026
- Adobe Illustrator 2026 C++ SDK
- Visual Studio C++ build tools with the v143 toolset
- Python 3.11 available through the Windows `py` launcher

## Build

Place the project inside the SDK `samplecode` folder and run:

```powershell
.\build.ps1
```

Expected output:

```text
<SDK>\samplecode\output\win\x64\Release\VectorHalftoneEffect.aip
```

`build.ps1` uses MSBuild's **Rebuild** target to avoid stale SDK object files.

## Install

Close Illustrator, replace your existing `VectorHalftoneEffect.aip` in Illustrator's Plug-ins folder, then restart Illustrator.

## Source colour note

For ordinary solid-filled paths and compound paths, generated artwork inherits the source fill directly. If Illustrator cannot expose a usable simple fill for the Live Effect input, the effect falls back to black rather than failing.

## Development status

v0.3.4 is based on the same project that successfully built and loaded as v0.1.4/v0.2.x on Illustrator 2026. The UI now additionally imports `AIUIThemeSuite`, which is part of Illustrator's SDK and is used only to query current UI theme colours/brightness.


## v0.3.4
- Tightened the Illustrator-style dialog layout so labels, status text and checkboxes no longer collide or clip.
- Removed the noisy helper text beside checkboxes; the form now uses cleaner Adobe-style rows and section dividers.
- Fixed the solid-centre renderer so it no longer switches to square tiles at the glow boundary. It now starts deeper inside the shape and preserves the selected mark family for a smoother transition.
- Keeps generated artwork locked to the source fill colour; the colour dropdown remains removed.


### v0.3.4 appearance fix
This build changes the Live Effect from a pre-effect to a post-effect so it can read the object after Illustrator applies its Fill and Stroke appearance. Marks now inherit fill and stroke colours from the source artwork instead of falling back to black.


### v0.3.4 polish pass
This update tightens the solid-centre behaviour and stroke handling. The centre now fills more reliably on thinner/wavy shapes, and repeated source strokes no longer pile up through the solid area. Transition dots still inherit the source fill/stroke colours, with stroke widths capped to small mark-safe outlines.

### macOS note
This ZIP is still a Windows Illustrator plug-in project. You can edit most shared C++ source on Windows, but a real Mac Illustrator plug-in must be built and tested on macOS with Xcode/the macOS SDK project. The current dialog implementation is Win32, so the Mac version also needs a macOS dialog implementation before it will feel complete.


### v0.3.4 curve/stroke/cull pass
- Added **Cull below** so tiny marks under a chosen point size are skipped instead of generating specks.
- Added **Marks perpendicular to edge**. This rotates directional marks from the nearest source path normal so line/diamond/square/star marks follow the curve more naturally.
- Reworked solid-centre overlap again to remove the small pinholes visible on wavy/thin shapes.
- Repeated source strokes are no longer applied to every mark, which prevents overlapping outlines. Stroke-only artwork still uses the source stroke colour as the mark fill.
- macOS builds still need macOS/Xcode; Windows can edit the shared C++ source but cannot produce or test the final macOS `.aip` for this Win32-dialog build.
