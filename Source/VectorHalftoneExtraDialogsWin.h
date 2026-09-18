#pragma once

#include "VectorHalftoneExtraParams.h"
#include <functional>

#ifdef WIN_ENV
#include <windows.h>

using GradientPreviewCallback = std::function<bool(const VectorHalftoneGradientParams&)>;
using SimpleColourHalftonePreviewCallback = std::function<bool(const VectorSimpleColourHalftoneParams&)>;
using PhotoshopHalftonePreviewCallback = std::function<bool(const VectorPhotoshopHalftoneParams&)>;

// Kept for legacy artwork; no longer surfaced as a new effect from v0.5.
int ShowVectorHalftoneGradientDialog(HWND parent, VectorHalftoneGradientParams& params,
                                     const GradientPreviewCallback& previewCallback);

int ShowVectorSimpleColourHalftoneDialog(HWND parent, VectorSimpleColourHalftoneParams& params,
                                         const SimpleColourHalftonePreviewCallback& previewCallback);

int ShowVectorPhotoshopHalftoneDialog(HWND parent, VectorPhotoshopHalftoneParams& params,
                                      const PhotoshopHalftonePreviewCallback& previewCallback);
#endif
