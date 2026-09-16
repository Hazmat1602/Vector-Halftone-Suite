#pragma once

#include "VectorHalftoneParams.h"
#include <functional>

#ifdef WIN_ENV
#include <windows.h>

using VectorHalftonePreviewCallback = std::function<bool(const VectorHalftoneParams&)>;

enum VectorHalftoneDialogResult {
    kVectorHalftoneDialogClosed = 0,
    kVectorHalftoneDialogCancel = 1,
    kVectorHalftoneDialogOK = 2
};

int ShowVectorHalftoneDialog(HWND parent, VectorHalftoneParams& params,
                             const VectorHalftonePreviewCallback& previewCallback);

bool LoadVectorHalftonePreferences(VectorHalftoneParams& params);
void SaveVectorHalftonePreferences(const VectorHalftoneParams& params);
#endif
