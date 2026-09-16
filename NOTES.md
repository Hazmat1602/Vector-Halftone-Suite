# Vector Halftone v0.3.1 engineering notes

## Solid centre

`solidCenter` is a new Live Effect dictionary parameter (`vigh.solidCenter`). In inward mode, the normal selected mark shape is used while `distance < glowWidth`. Once a candidate grid point is beyond the transition width, the renderer switches the core mark to an overlapping square tile sized to 1.60× the grid spacing. That is larger than the grid diagonal, so the core has no gaps even when the transition uses stars, triangles or line marks.

The solid core remains vector artwork and is generated inside the same exact clipping group when clipping is enabled.

## Source colour is now automatic

v0.3 always calls `VHGetSourceFillColor()` and uses the returned source fill for generated marks. The old `colorMode` dictionary key is retained only for backwards compatibility and is written as source-fill mode. The colour dropdown was removed from the UI.

## Illustrator-style UI

The Windows settings dialog now imports `AIUIThemeSuite`. It queries:

- `kAIUIComponentColorBackground`
- `kAIUIComponentColorText`
- `kAIUIComponentColorEditTextBackground`
- `IsUIThemeDark()`

This lets the dialog follow Illustrator's current UI brightness. The layout now uses flat sections/dividers instead of Windows group boxes, smaller Illustrator-like spacing, inline units, muted helper text, Preview at lower left, and Cancel/OK at lower right.

Native controls are additionally given the Explorer/DarkMode_Explorer Windows theme where available. The dark title-bar request is loaded dynamically from `dwmapi.dll`, so no extra linker dependency is required.

## Compatibility

The Live Effect internal identifier is unchanged. Existing v0.1/v0.2 art continues to resolve. The last-used Windows preference blob moved to `LastUsedParamsV3` because the parameter struct gained `solidCenter`; old preference blobs are intentionally ignored rather than being misread.

## Build behaviour

The project still uses the forced Rebuild target introduced in v0.1.4 to prevent stale `.obj` files under the SDK's shared `samplecode\output` directory.


## v0.3.1
- Tightened the Illustrator-style dialog layout so labels, status text and checkboxes no longer collide or clip.
- Removed the noisy helper text beside checkboxes; the form now uses cleaner Adobe-style rows and section dividers.
- Fixed the solid-centre renderer so it no longer switches to square tiles at the glow boundary. It now starts deeper inside the shape and preserves the selected mark family for a smoother transition.
- Keeps generated artwork locked to the source fill colour; the colour dropdown remains removed.


## v0.3.2
- Fixed fill/stroke inheritance. The effect now registers as a post-effect so Illustrator has already applied the object's Fill/Stroke appearance before the halftone renderer samples it.
- Generated marks now copy source fill and stroke colours. If the source has stroke-only artwork, the stroke colour becomes the mark fill so the halftone remains visible.
- Stroke colour is preserved on marks, with stroke width capped for tiny dots so heavy outlines do not swallow the halftone.
- Expanded preferred input support to paths, compound paths and groups so post-effect artwork can still be flattened back into usable halftone geometry.


## v0.3.3
- Improved **Make centre solid** so the interior uses more aggressive overlap and starts inside the transition instead of beyond the glow width. This removes small gaps in shallow/wavy shapes.
- Core marks suppress repeated source strokes, preventing stroke outlines from stacking/overlapping inside the solid centre.
- Source strokes are still inherited on transition marks, but stroke width is now capped much more tightly so outlines behave like mark hairlines rather than full artwork strokes.
- Windows-only build remains the supported output from this project; a native macOS build needs a macOS/Xcode project and a macOS dialog implementation.


## v0.3.4
- Added `vigh.cullSize` dictionary parameter and Windows UI field. Marks below the threshold are skipped after tone/solid-centre sizing.
- Added `vigh.followCurve` dictionary parameter and Windows UI checkbox. When enabled, marks are rotated to the normal angle of the nearest flattened source segment via `VHNearestEdgeNormalAngle()`.
- Directional mark geometry now actually honours the supplied mark angle for squares, diamonds, triangles, hexagons, stars and line marks. Circles remain visually unchanged.
- Solid-centre overlap was increased and starts earlier in the transition to remove residual pinholes in wavy shapes.
- Repeated per-mark source strokes are disabled to avoid visible self-overlap. Fill colour is inherited from source fill; stroke-only artwork falls back to stroke colour as mark fill.
- This remains a Windows project. A macOS build requires a macOS/Xcode target and a non-Win32 dialog implementation.
