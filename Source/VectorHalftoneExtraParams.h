#pragma once

// Legacy v0.4 gradient parameters are intentionally retained so artwork created
// with older builds can still resolve the hidden legacy effect. The effect is
// no longer exposed in the Effect menu from v0.5 onward.
struct VectorHalftoneGradientParams {
    int gradientType = 0;      // 0 linear, 1 radial
    double gradientAngle = 0.0;
    double spacing = 10.0;
    double minSize = 0.0;
    double maxSize = 9.0;
    double cullSize = 0.25;
    double gridAngle = 45.0;
    int shape = 0;
    int reverse = 0;
    int stagger = 1;
    int clipToSource = 1;
    int preserveSourceAppearance = 0;
};

inline VectorHalftoneGradientParams VectorHalftoneGradientDefaults() {
    return VectorHalftoneGradientParams{};
}

// Simple Colour Halftone. v0.7 is a direct-vector shortcut for the common
// Illustrator workflow: a gradient or source artwork controls the size of a
// regular halftone grid, then the plug-in creates clean editable circles/shapes
// directly. No Rasterize -> Image Trace -> Expand -> replaceItems.jsx stage is
// required.
//
// Legacy v0.5/v0.6 fields are retained for document compatibility. New
// instances use mode=2.
struct VectorSimpleColourHalftoneParams {
    int mode = 2;                    // 0 v0.5 flat pattern, 1 v0.6 mask workflow, 2 direct vector

    // v0.7 source/tone controls.
    int sourceMode = 0;              // 0 procedural gradient, 1 source artwork/image luminance
    int gradientType = 0;            // 0 linear, 1 radial
    double gradientAngle = 0.0;      // degrees, linear gradient only
    double gradientOffset = 0.0;     // -100..100%, moves the tonal midpoint
    double gradientScale = 100.0;    // 10..400%, tone transition width
    int reverse = 0;

    // v0.7 regular-grid output controls.
    int shape = 0;                   // same mark family as Vector Halftone
    double spacing = 17.0;           // centre-to-centre grid spacing, document points
    double gridAngle = 45.0;         // grid rotation in degrees
    double minSize = 0.0;            // minimum mark diameter/extent, document points
    double maxSize = 14.0;           // maximum mark diameter/extent, document points
    double cullSize = 0.15;          // skip marks smaller than this size
    int stagger = 0;
    int connectStroke = 0;           // outline each mark using its fill colour
    double strokeWidth = 0.0;        // document points
    int clipToSource = 1;
    int preserveSourceAppearance = 0;

    // v0.6 compatibility fields.
    double maxRadius = 20.0;
    double screenAngle = 45.0;
    double shadeStrength = 28.0;

    // v0.5 compatibility fields.
    double markSize = 6.0;
    double opacity = 100.0;
};

inline VectorSimpleColourHalftoneParams VectorSimpleColourHalftoneDefaults() {
    return VectorSimpleColourHalftoneParams{};
}

struct VectorPhotoshopHalftoneParams {
    double maxRadius = 8.0;    // Photoshop-style pixel radius; converted to document points using sampleDpi
    double angle1 = 108.0;     // Channel 1: Red in RGB / Cyan in CMYK
    double angle2 = 162.0;     // Channel 2: Green in RGB / Magenta in CMYK
    double angle3 = 90.0;      // Channel 3: Blue in RGB / Yellow in CMYK
    double angle4 = 45.0;      // Channel 4: unused in RGB / Black in CMYK
    double cullSize = 0.0;
    double sampleDpi = 72.0;
    int preserveSourceAppearance = 0;
};

inline VectorPhotoshopHalftoneParams VectorPhotoshopHalftoneDefaults() {
    return VectorPhotoshopHalftoneParams{};
}
