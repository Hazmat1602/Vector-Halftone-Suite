#pragma once

// Parameters persisted in Illustrator's Live Effect dictionary.
// Keep existing fields/semantics compatible with v0.1.x so artwork made with the
// early builds continues to render the same way.
struct VectorHalftoneParams {
    double glowWidth = 18.0;
    double glowStrength = 1.0;
    double gamma = 1.35;
    int invert = 0;

    // 0 circle, 1 square, 2 diamond, 3 triangle, 4 hexagon, 5 star, 6 line
    int shape = 0;
    double spacing = 9.0;
    double minSize = 0.0;
    double maxSize = 8.2;
    double cullSize = 0.0;             // skip generated marks below this size in points
    double gridAngle = 45.0;
    int stagger = 1;
    int curveSamples = 8;

    // v0.1 compatibility. Old effects used this to shrink edge marks so their
    // entire geometry stayed inside the source shape.
    int containDots = 1;

    // v0.2 additions.
    int glowMode = 0;                 // 0 inward, 1 outward
    double edgeSoftness = 0.65;       // 0..1, blends linear -> smooth fade
    int colorMode = 1;                // legacy field; v0.3+ always follows source fill colour
    int clipToSource = 0;             // exact compound-path clipping for inward mode
    int preserveSourceAppearance = 0; // keep the effect input art below the marks
    int solidCenter = 0;              // inward mode: overlap core marks so the centre becomes solid
    int followCurve = 0;              // rotate marks perpendicular to nearest source edge
    int preset = 0;                   // 0 custom, 1 fine, 2 medium, 3 coarse, 4 screenprint
};

inline VectorHalftoneParams VectorHalftoneFactoryDefaults() {
    VectorHalftoneParams p;
    // New v0.2 effects default to exact clipping rather than the old edge-shrink
    // behaviour. Existing v0.1 effects keep containDots=1 and clipToSource=0.
    p.containDots = 0;
    p.clipToSource = 1;
    p.edgeSoftness = 0.65;
    p.colorMode = 1;
    p.solidCenter = 0;
    p.followCurve = 0;
    p.cullSize = 0.25;
    return p;
}

inline void VectorHalftoneApplyPreset(VectorHalftoneParams& p, int preset) {
    p.preset = preset;
    switch (preset) {
    case 1: // Fine
        p.glowWidth = 14.0;
        p.glowStrength = 1.0;
        p.edgeSoftness = 0.72;
        p.gamma = 1.20;
        p.shape = 0;
        p.spacing = 5.0;
        p.minSize = 0.0;
        p.maxSize = 4.5;
        p.cullSize = 0.12;
        p.gridAngle = 45.0;
        p.stagger = 1;
        p.curveSamples = 8;
        break;
    case 2: // Medium
        p.glowWidth = 18.0;
        p.glowStrength = 1.0;
        p.edgeSoftness = 0.65;
        p.gamma = 1.35;
        p.shape = 0;
        p.spacing = 9.0;
        p.minSize = 0.0;
        p.maxSize = 8.2;
        p.cullSize = 0.25;
        p.gridAngle = 45.0;
        p.stagger = 1;
        p.curveSamples = 8;
        break;
    case 3: // Coarse
        p.glowWidth = 28.0;
        p.glowStrength = 1.0;
        p.edgeSoftness = 0.55;
        p.gamma = 1.25;
        p.shape = 0;
        p.spacing = 15.0;
        p.minSize = 0.0;
        p.maxSize = 13.5;
        p.cullSize = 0.40;
        p.gridAngle = 45.0;
        p.stagger = 1;
        p.curveSamples = 8;
        break;
    case 4: // Screenprint
        p.glowWidth = 22.0;
        p.glowStrength = 1.0;
        p.edgeSoftness = 0.80;
        p.gamma = 1.55;
        p.shape = 0;
        p.spacing = 12.0;
        p.minSize = 0.0;
        p.maxSize = 10.5;
        p.cullSize = 0.35;
        p.gridAngle = 45.0;
        p.stagger = 1;
        p.curveSamples = 12;
        break;
    default:
        p.preset = 0;
        break;
    }
}
