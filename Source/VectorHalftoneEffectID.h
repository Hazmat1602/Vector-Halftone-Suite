#pragma once

#define kVectorHalftonePluginName "VectorHalftoneEffect"
#define kVectorHalftoneEffectCategory "Vector Halftone"

// Legacy effect IDs stay registered (without menu items) so existing artwork
// made in v0.1-v0.4 still resolves correctly after upgrading.
#define kVectorHalftoneEffectName "com.hazmat.vectorhalftone.innerglow"
#define kVectorHalftoneEffectTitle "Vector Halftone"
#define kVectorHalftoneEffectMenuTitle "Vector Halftone..."

#define kVectorHalftoneGradientEffectName "com.hazmat.vectorhalftone.gradient"
#define kVectorHalftoneGradientEffectTitle "Halftone Gradient"
#define kVectorHalftoneGradientEffectMenuTitle "Halftone Gradient..."

#define kVectorSimpleColourHalftoneEffectName "com.hazmat.vectorhalftone.simplecolour"
#define kVectorSimpleColourHalftoneEffectTitle "Simple Colour Halftone"
#define kVectorSimpleColourHalftoneEffectMenuTitle "Simple Colour Halftone..."

// v0.8+: visible vector pattern effect. Simple Colour Halftone remains
// registered without a menu item so existing documents keep rendering.
#define kVectorPatternedHalftoneEffectName "com.hazmat.vectorhalftone.patterned"
#define kVectorPatternedHalftoneEffectTitle "Patterned Halftone"
#define kVectorPatternedHalftoneEffectMenuTitle "Patterned Halftone..."

#define kVectorPhotoshopHalftoneEffectName "com.hazmat.vectorhalftone.photoshop"
#define kVectorPhotoshopHalftoneEffectTitle "Color Halftone"
#define kVectorPhotoshopHalftoneEffectMenuTitle "Color Halftone..."
