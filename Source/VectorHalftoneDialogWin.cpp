#include "IllustratorSDK.h"
#include "VectorHalftoneDialogWin.h"
#include "VectorHalftoneEffectSuites.h"

#ifdef WIN_ENV

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"VectorHalftoneEffectSettingsWindowV34";
constexpr wchar_t kRegistryPath[] = L"Software\\Hazmat\\VectorHalftoneEffect";
constexpr wchar_t kRegistryValue[] = L"LastUsedParamsV4";
constexpr UINT_PTR kPreviewTimer = 1;
constexpr UINT kPreviewDelayMs = 140;

enum ControlId {
    ID_PRESET = 1001,
    ID_RESET,
    ID_GLOW_MODE,
    ID_GLOW_WIDTH,
    ID_STRENGTH,
    ID_SOFTNESS,
    ID_GAMMA,
    ID_INVERT,
    ID_SOLID_CENTER,
    ID_SHAPE,
    ID_SPACING,
    ID_MIN_SIZE,
    ID_MAX_SIZE,
    ID_CULL_SIZE,
    ID_ANGLE,
    ID_FOLLOW_CURVE,
    ID_STAGGER,
    ID_SAMPLES,
    ID_CLIP_SOURCE,
    ID_PRESERVE_SOURCE,
    ID_PREVIEW,
    ID_STATUS,
    ID_OK_BUTTON,
    ID_CANCEL_BUTTON
};

struct DialogState {
    VectorHalftoneParams initial;
    VectorHalftoneParams working;
    VectorHalftonePreviewCallback previewCallback;
    bool populating = false;
    bool done = false;
    int result = kVectorHalftoneDialogClosed;
};

struct UiPalette {
    bool dark = false;
    COLORREF background = RGB(245, 245, 245);
    COLORREF field = RGB(255, 255, 255);
    COLORREF text = RGB(35, 35, 35);
    COLORREF fieldText = RGB(35, 35, 35);
    COLORREF muted = RGB(105, 105, 105);
    COLORREF divider = RGB(205, 205, 205);
    HBRUSH backgroundBrush = nullptr;
    HBRUSH fieldBrush = nullptr;
};

UiPalette gUi;

HFONT UiFont() {
    static HFONT font = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

HFONT SectionFont() {
    static HFONT font = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : UiFont();
}

HFONT SmallFont() {
    static HFONT font = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : UiFont();
}

COLORREF ThemeColorToWin(const AIUIThemeColor& c) {
    auto channel = [](AIReal v) -> BYTE {
        const double x = (std::max)(0.0, (std::min)(1.0, static_cast<double>(v)));
        return static_cast<BYTE>(std::lround(x * 255.0));
    };
    return RGB(channel(c.red), channel(c.green), channel(c.blue));
}

COLORREF BlendColor(COLORREF a, COLORREF b, double t) {
    t = (std::max)(0.0, (std::min)(1.0, t));
    auto blend = [t](BYTE x, BYTE y) -> BYTE {
        return static_cast<BYTE>(std::lround(static_cast<double>(x) +
            (static_cast<double>(y) - static_cast<double>(x)) * t));
    };
    return RGB(blend(GetRValue(a), GetRValue(b)),
               blend(GetGValue(a), GetGValue(b)),
               blend(GetBValue(a), GetBValue(b)));
}

void ResetPalette(bool dark) {
    if (gUi.backgroundBrush) DeleteObject(gUi.backgroundBrush);
    if (gUi.fieldBrush) DeleteObject(gUi.fieldBrush);

    gUi.dark = dark;
    if (dark) {
        gUi.background = RGB(50, 50, 50);
        gUi.field = RGB(68, 68, 68);
        gUi.text = RGB(226, 226, 226);
        gUi.fieldText = RGB(235, 235, 235);
    } else {
        gUi.background = RGB(245, 245, 245);
        gUi.field = RGB(255, 255, 255);
        gUi.text = RGB(38, 38, 38);
        gUi.fieldText = RGB(30, 30, 30);
    }

    // Pull the actual current Illustrator dialog colours when the suite is
    // available. This follows all four Illustrator UI brightness levels rather
    // than approximating dark/light mode from Windows.
    if (sVectorHalftoneUITheme) {
        AIUIThemeColor bg, text, field, fieldText;
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog,
                kAIUIComponentColorBackground, bg) == kNoErr)
            gUi.background = ThemeColorToWin(bg);
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog,
                kAIUIComponentColorText, text) == kNoErr)
            gUi.text = ThemeColorToWin(text);
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog,
                kAIUIComponentColorEditTextBackground, field) == kNoErr)
            gUi.field = ThemeColorToWin(field);
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog,
                kAIUIComponentColorEditText, fieldText) == kNoErr)
            gUi.fieldText = ThemeColorToWin(fieldText);
    }

    gUi.muted = BlendColor(gUi.text, gUi.background, 0.48);
    gUi.divider = BlendColor(gUi.background, gUi.text, gUi.dark ? 0.20 : 0.16);
    gUi.backgroundBrush = CreateSolidBrush(gUi.background);
    gUi.fieldBrush = CreateSolidBrush(gUi.field);
}

void SetFont(HWND h, HFONT font = nullptr) {
    if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : UiFont()), TRUE);
}

void ApplyNativeTheme(HWND h) {
    if (!h) return;
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    using SetWindowThemeFn = HRESULT (WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    auto fn = reinterpret_cast<SetWindowThemeFn>(GetProcAddress(ux, "SetWindowTheme"));
    if (fn) fn(h, gUi.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    FreeLibrary(ux);
}

void ApplyDarkTitleBar(HWND hwnd) {
    if (!gUi.dark) return;
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using DwmSetWindowAttributeFn = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto fn = reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (fn) {
        BOOL enabled = TRUE;
        // 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on current Windows builds. 19 is
        // retained as a fallback for older Windows 10 builds.
        if (FAILED(fn(hwnd, 20, &enabled, sizeof(enabled))))
            fn(hwnd, 19, &enabled, sizeof(enabled));
    }
    FreeLibrary(dwm);
}

HWND AddControl(HWND parent, const wchar_t* klass, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id, DWORD exStyle = 0, HFONT font = nullptr) {
    HWND ctrl = CreateWindowExW(
        exStyle, klass, text, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetFont(ctrl, font);
    ApplyNativeTheme(ctrl);
    return ctrl;
}

void AddLabel(HWND parent, const wchar_t* text, int x, int y, int w = 120) {
    AddControl(parent, L"STATIC", text, SS_LEFT | SS_NOPREFIX, x, y + 2, w, 20, 0);
}

void AddUnit(HWND parent, const wchar_t* text, int x, int y, int w = 34) {
    AddControl(parent, L"STATIC", text, SS_LEFT | SS_NOPREFIX, x, y + 2, w, 20, 0, 0, SmallFont());
}

void AddHint(HWND parent, const wchar_t* text, int x, int y, int w, int h = 34) {
    AddControl(parent, L"STATIC", text, SS_LEFT | SS_NOPREFIX, x, y, w, h, 0, 0, SmallFont());
}

void AddSection(HWND parent, const wchar_t* text, int y) {
    AddControl(parent, L"STATIC", text, SS_LEFT | SS_NOPREFIX, 20, y, 126, 20, 0, 0, SectionFont());
    AddControl(parent, L"STATIC", L"", SS_ETCHEDHORZ, 148, y + 10, 372, 2, 0);
}

void SetDouble(HWND h, double v) {
    wchar_t buf[64]{};
    swprintf_s(buf, L"%.4g", v);
    SetWindowTextW(h, buf);
}

bool ReadDouble(HWND h, double& out) {
    wchar_t buf[128]{};
    GetWindowTextW(h, buf, 127);
    wchar_t* end = nullptr;
    const double v = std::wcstod(buf, &end);
    if (end == buf || !std::isfinite(v)) return false;
    while (*end == L' ' || *end == L'\t') ++end;
    if (*end != L'\0') return false;
    out = v;
    return true;
}

void SetStatus(HWND hwnd, const wchar_t* text) {
    SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), text);
}

void ComboAddStringW(HWND combo, const wchar_t* text) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void PopulateCombos(HWND hwnd) {
    HWND preset = GetDlgItem(hwnd, ID_PRESET);
    ComboAddStringW(preset, L"Custom");
    ComboAddStringW(preset, L"Fine");
    ComboAddStringW(preset, L"Medium");
    ComboAddStringW(preset, L"Coarse");
    ComboAddStringW(preset, L"Screenprint");

    HWND mode = GetDlgItem(hwnd, ID_GLOW_MODE);
    ComboAddStringW(mode, L"Inward from edge");
    ComboAddStringW(mode, L"Outward from edge");

    HWND shape = GetDlgItem(hwnd, ID_SHAPE);
    ComboAddStringW(shape, L"Circle");
    ComboAddStringW(shape, L"Square");
    ComboAddStringW(shape, L"Diamond");
    ComboAddStringW(shape, L"Triangle");
    ComboAddStringW(shape, L"Hexagon");
    ComboAddStringW(shape, L"Star");
    ComboAddStringW(shape, L"Line");

    HWND samples = GetDlgItem(hwnd, ID_SAMPLES);
    ComboAddStringW(samples, L"Fast");
    ComboAddStringW(samples, L"Normal");
    ComboAddStringW(samples, L"High");
    ComboAddStringW(samples, L"Very high");
}

void UpdateDependentControls(HWND hwnd) {
    const bool inward = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_GLOW_MODE)) != 1;
    EnableWindow(GetDlgItem(hwnd, ID_SOLID_CENTER), inward ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd, ID_CLIP_SOURCE), inward ? TRUE : FALSE);
}

void PopulateFields(HWND hwnd, DialogState& state) {
    state.populating = true;
    VectorHalftoneParams& p = state.working;
    p.colorMode = 1; // v0.3+: source colour is no longer optional.

    ComboBox_SetCurSel(GetDlgItem(hwnd, ID_PRESET), (p.preset >= 0 && p.preset <= 4) ? p.preset : 0);
    ComboBox_SetCurSel(GetDlgItem(hwnd, ID_GLOW_MODE), p.glowMode == 1 ? 1 : 0);
    SetDouble(GetDlgItem(hwnd, ID_GLOW_WIDTH), p.glowWidth);
    SetDouble(GetDlgItem(hwnd, ID_STRENGTH), p.glowStrength * 100.0);
    SetDouble(GetDlgItem(hwnd, ID_SOFTNESS), p.edgeSoftness * 100.0);
    SetDouble(GetDlgItem(hwnd, ID_GAMMA), p.gamma);
    Button_SetCheck(GetDlgItem(hwnd, ID_INVERT), p.invert ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(hwnd, ID_SOLID_CENTER), p.solidCenter ? BST_CHECKED : BST_UNCHECKED);

    ComboBox_SetCurSel(GetDlgItem(hwnd, ID_SHAPE), (p.shape >= 0 && p.shape <= 6) ? p.shape : 0);
    SetDouble(GetDlgItem(hwnd, ID_SPACING), p.spacing);
    SetDouble(GetDlgItem(hwnd, ID_MIN_SIZE), p.minSize);
    SetDouble(GetDlgItem(hwnd, ID_MAX_SIZE), p.maxSize);
    SetDouble(GetDlgItem(hwnd, ID_CULL_SIZE), p.cullSize);
    SetDouble(GetDlgItem(hwnd, ID_ANGLE), p.gridAngle);
    Button_SetCheck(GetDlgItem(hwnd, ID_FOLLOW_CURVE), p.followCurve ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(hwnd, ID_STAGGER), p.stagger ? BST_CHECKED : BST_UNCHECKED);

    int qi = 1;
    if (p.curveSamples <= 4) qi = 0;
    else if (p.curveSamples >= 20) qi = 3;
    else if (p.curveSamples >= 12) qi = 2;
    ComboBox_SetCurSel(GetDlgItem(hwnd, ID_SAMPLES), qi);

    Button_SetCheck(GetDlgItem(hwnd, ID_CLIP_SOURCE), p.clipToSource ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(hwnd, ID_PRESERVE_SOURCE), p.preserveSourceAppearance ? BST_CHECKED : BST_UNCHECKED);
    UpdateDependentControls(hwnd);
    state.populating = false;
}

bool ShowError(HWND hwnd, bool showErrors, const wchar_t* message) {
    if (showErrors) MessageBoxW(hwnd, message, L"Vector Halftone", MB_OK | MB_ICONWARNING);
    return false;
}

bool ReadAndValidate(HWND hwnd, VectorHalftoneParams& p, bool showErrors) {
    double strengthPct = 0.0;
    double softnessPct = 0.0;

    if (!ReadDouble(GetDlgItem(hwnd, ID_GLOW_WIDTH), p.glowWidth) || p.glowWidth <= 0.0)
        return ShowError(hwnd, showErrors, L"Glow width must be greater than 0.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_STRENGTH), strengthPct) || strengthPct < 0.0 || strengthPct > 100.0)
        return ShowError(hwnd, showErrors, L"Glow strength must be between 0 and 100.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_SOFTNESS), softnessPct) || softnessPct < 0.0 || softnessPct > 100.0)
        return ShowError(hwnd, showErrors, L"Edge softness must be between 0 and 100.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_GAMMA), p.gamma) || p.gamma <= 0.0)
        return ShowError(hwnd, showErrors, L"Fade curve must be greater than 0.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_SPACING), p.spacing) || p.spacing <= 0.0)
        return ShowError(hwnd, showErrors, L"Spacing must be greater than 0.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_MIN_SIZE), p.minSize) || p.minSize < 0.0)
        return ShowError(hwnd, showErrors, L"Minimum mark size must be 0 or greater.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_MAX_SIZE), p.maxSize) || p.maxSize <= 0.0 || p.maxSize < p.minSize)
        return ShowError(hwnd, showErrors, L"Maximum mark size must be greater than 0 and at least the minimum size.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_CULL_SIZE), p.cullSize) || p.cullSize < 0.0)
        return ShowError(hwnd, showErrors, L"Cull size must be 0 or greater.");
    if (!ReadDouble(GetDlgItem(hwnd, ID_ANGLE), p.gridAngle))
        return ShowError(hwnd, showErrors, L"Screen angle must be a number.");

    p.glowStrength = strengthPct / 100.0;
    p.edgeSoftness = softnessPct / 100.0;
    p.glowMode = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_GLOW_MODE)) == 1 ? 1 : 0;
    p.invert = Button_GetCheck(GetDlgItem(hwnd, ID_INVERT)) == BST_CHECKED ? 1 : 0;
    p.solidCenter = Button_GetCheck(GetDlgItem(hwnd, ID_SOLID_CENTER)) == BST_CHECKED ? 1 : 0;
    p.shape = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_SHAPE));
    if (p.shape < 0 || p.shape > 6) p.shape = 0;
    p.followCurve = Button_GetCheck(GetDlgItem(hwnd, ID_FOLLOW_CURVE)) == BST_CHECKED ? 1 : 0;
    p.stagger = Button_GetCheck(GetDlgItem(hwnd, ID_STAGGER)) == BST_CHECKED ? 1 : 0;
    p.colorMode = 1;
    p.clipToSource = Button_GetCheck(GetDlgItem(hwnd, ID_CLIP_SOURCE)) == BST_CHECKED ? 1 : 0;
    p.preserveSourceAppearance = Button_GetCheck(GetDlgItem(hwnd, ID_PRESERVE_SOURCE)) == BST_CHECKED ? 1 : 0;
    p.preset = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_PRESET));
    if (p.preset < 0 || p.preset > 4) p.preset = 0;

    const int qi = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_SAMPLES));
    const int qv[] = {4, 8, 12, 20};
    p.curveSamples = (qi >= 0 && qi <= 3) ? qv[qi] : 8;
    return true;
}

bool IsPresetAffectingControl(int id) {
    switch (id) {
    case ID_GLOW_WIDTH:
    case ID_STRENGTH:
    case ID_SOFTNESS:
    case ID_GAMMA:
    case ID_SHAPE:
    case ID_SPACING:
    case ID_MIN_SIZE:
    case ID_MAX_SIZE:
    case ID_CULL_SIZE:
    case ID_ANGLE:
    case ID_FOLLOW_CURVE:
    case ID_STAGGER:
    case ID_SAMPLES:
        return true;
    default:
        return false;
    }
}

void MarkCustom(HWND hwnd, DialogState& state) {
    if (state.populating) return;
    state.working.preset = 0;
    ComboBox_SetCurSel(GetDlgItem(hwnd, ID_PRESET), 0);
}

void SchedulePreview(HWND hwnd) {
    if (Button_GetCheck(GetDlgItem(hwnd, ID_PREVIEW)) != BST_CHECKED) return;
    KillTimer(hwnd, kPreviewTimer);
    SetTimer(hwnd, kPreviewTimer, kPreviewDelayMs, nullptr);
    SetStatus(hwnd, L"Updating preview...");
}

void RunPreview(HWND hwnd, DialogState& state) {
    KillTimer(hwnd, kPreviewTimer);
    if (Button_GetCheck(GetDlgItem(hwnd, ID_PREVIEW)) != BST_CHECKED) {
        SetStatus(hwnd, L"Preview paused");
        return;
    }

    VectorHalftoneParams candidate = state.working;
    if (!ReadAndValidate(hwnd, candidate, false)) {
        SetStatus(hwnd, L"Enter valid values to update preview");
        return;
    }

    state.working = candidate;
    if (state.previewCallback && state.previewCallback(candidate))
        SetStatus(hwnd, L"Preview up to date");
    else
        SetStatus(hwnd, L"Preview could not be updated");
}

void CentreWindow(HWND hwnd, HWND parent) {
    RECT wr{}, pr{};
    GetWindowRect(hwnd, &wr);
    if (parent && IsWindow(parent)) GetWindowRect(parent, &pr);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &pr, 0);
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    const int x = pr.left + ((pr.right - pr.left) - w) / 2;
    const int y = pr.top + ((pr.bottom - pr.top) - h) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DialogState* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        state = reinterpret_cast<DialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (msg) {
    case WM_CREATE: {
        ApplyDarkTitleBar(hwnd);
        SetFont(hwnd);

        const int labelX = 26;
        const int editX = 144;
        const int editW = 82;
        const int unitX = 232;
        const int rightLabelX = 292;
        const int rightEditX = 410;

        AddLabel(hwnd, L"Preset", 22, 22, 90);
        AddControl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 118, 18, 260, 190, ID_PRESET);
        AddControl(hwnd, L"BUTTON", L"Reset", BS_PUSHBUTTON | BS_FLAT | WS_TABSTOP, 404, 18, 82, 26, ID_RESET);

        AddSection(hwnd, L"Glow", 66);
        AddLabel(hwnd, L"Direction", labelX, 94); AddControl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, editX, 90, 218, 140, ID_GLOW_MODE);
        AddLabel(hwnd, L"Width", labelX, 127); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 123, editW, 23, ID_GLOW_WIDTH); AddUnit(hwnd, L"pt", unitX, 123);
        AddLabel(hwnd, L"Strength", labelX, 159); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 155, editW, 23, ID_STRENGTH); AddUnit(hwnd, L"%", unitX, 155);
        AddLabel(hwnd, L"Softness", rightLabelX, 127, 94); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, rightEditX, 123, 72, 23, ID_SOFTNESS); AddUnit(hwnd, L"%", 488, 123);
        AddLabel(hwnd, L"Fade curve", rightLabelX, 159, 94); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, rightEditX, 155, 72, 23, ID_GAMMA);
        AddControl(hwnd, L"BUTTON", L"Invert tone", BS_AUTOCHECKBOX | WS_TABSTOP, labelX, 194, 150, 22, ID_INVERT);
        AddControl(hwnd, L"BUTTON", L"Make centre solid", BS_AUTOCHECKBOX | WS_TABSTOP, labelX, 221, 210, 22, ID_SOLID_CENTER);

        AddSection(hwnd, L"Halftone", 264);
        AddLabel(hwnd, L"Mark shape", labelX, 292); AddControl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, editX, 288, 218, 210, ID_SHAPE);
        AddLabel(hwnd, L"Spacing", labelX, 325); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 321, editW, 23, ID_SPACING); AddUnit(hwnd, L"pt", unitX, 321);
        AddLabel(hwnd, L"Minimum size", labelX, 357); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 353, editW, 23, ID_MIN_SIZE); AddUnit(hwnd, L"pt", unitX, 353);
        AddLabel(hwnd, L"Maximum size", labelX, 389); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 385, editW, 23, ID_MAX_SIZE); AddUnit(hwnd, L"pt", unitX, 385);
        AddLabel(hwnd, L"Cull below", labelX, 421); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, editX, 417, editW, 23, ID_CULL_SIZE); AddUnit(hwnd, L"pt", unitX, 417);
        AddLabel(hwnd, L"Screen angle", rightLabelX, 325, 104); AddControl(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, rightEditX, 321, 72, 23, ID_ANGLE); AddUnit(hwnd, L"°", 488, 321);
        AddLabel(hwnd, L"Curve quality", rightLabelX, 357, 104); AddControl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, rightEditX, 353, 98, 150, ID_SAMPLES);
        AddControl(hwnd, L"BUTTON", L"Marks perpendicular to edge", BS_AUTOCHECKBOX | WS_TABSTOP, rightLabelX, 386, 226, 24, ID_FOLLOW_CURVE);
        AddControl(hwnd, L"BUTTON", L"Stagger rows", BS_AUTOCHECKBOX | WS_TABSTOP, labelX, 453, 160, 22, ID_STAGGER);

        AddSection(hwnd, L"Output", 496);
        AddControl(hwnd, L"BUTTON", L"Clip inward halftone exactly to source", BS_AUTOCHECKBOX | WS_TABSTOP, labelX, 525, 330, 22, ID_CLIP_SOURCE);
        AddControl(hwnd, L"BUTTON", L"Preserve original appearance underneath", BS_AUTOCHECKBOX | WS_TABSTOP, labelX, 553, 340, 22, ID_PRESERVE_SOURCE);

        AddControl(hwnd, L"STATIC", L"", SS_ETCHEDHORZ, 20, 594, 500, 2, 0);
        AddControl(hwnd, L"BUTTON", L"Preview", BS_AUTOCHECKBOX | WS_TABSTOP, 20, 614, 92, 22, ID_PREVIEW);
        Button_SetCheck(GetDlgItem(hwnd, ID_PREVIEW), BST_CHECKED);
        AddControl(hwnd, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 116, 616, 220, 20, ID_STATUS, 0, SmallFont());

        AddControl(hwnd, L"BUTTON", L"Cancel", BS_PUSHBUTTON | BS_FLAT | WS_TABSTOP, 346, 608, 82, 28, ID_CANCEL_BUTTON);
        AddControl(hwnd, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 438, 608, 82, 28, ID_OK_BUTTON);

        PopulateCombos(hwnd);
        if (state) {
            PopulateFields(hwnd, *state);
            SetStatus(hwnd, L"Preview on");
            SetTimer(hwnd, kPreviewTimer, 60, nullptr);
        }
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT r{};
        GetClientRect(hwnd, &r);
        FillRect(reinterpret_cast<HDC>(wp), &r, gUi.backgroundBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        HWND child = reinterpret_cast<HWND>(lp);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, IsWindowEnabled(child) ? gUi.text : gUi.muted);
        return reinterpret_cast<INT_PTR>(gUi.backgroundBrush);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor(dc, gUi.field);
        SetTextColor(dc, gUi.fieldText);
        return reinterpret_cast<INT_PTR>(gUi.fieldBrush);
    }

    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor(dc, gUi.background);
        SetTextColor(dc, gUi.text);
        return reinterpret_cast<INT_PTR>(gUi.backgroundBrush);
    }

    case WM_TIMER:
        if (wp == kPreviewTimer && state) {
            RunPreview(hwnd, *state);
            return 0;
        }
        break;

    case WM_COMMAND:
        if (!state) break;
        {
            const int id = LOWORD(wp);
            const int notification = HIWORD(wp);

            if (id == ID_OK_BUTTON) {
                VectorHalftoneParams candidate = state->working;
                if (ReadAndValidate(hwnd, candidate, true)) {
                    state->working = candidate;
                    state->result = kVectorHalftoneDialogOK;
                    state->done = true;
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            if (id == ID_CANCEL_BUTTON) {
                state->result = kVectorHalftoneDialogCancel;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == ID_RESET && notification == BN_CLICKED) {
                state->working = VectorHalftoneFactoryDefaults();
                PopulateFields(hwnd, *state);
                SchedulePreview(hwnd);
                return 0;
            }
            if (id == ID_PRESET && notification == CBN_SELCHANGE && !state->populating) {
                const int preset = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_PRESET));
                if (preset > 0) {
                    // Presets replace the screen/tone recipe but retain user output
                    // choices and the current inward/outward direction.
                    state->working.glowMode = ComboBox_GetCurSel(GetDlgItem(hwnd, ID_GLOW_MODE)) == 1 ? 1 : 0;
                    state->working.invert = Button_GetCheck(GetDlgItem(hwnd, ID_INVERT)) == BST_CHECKED ? 1 : 0;
                    state->working.solidCenter = Button_GetCheck(GetDlgItem(hwnd, ID_SOLID_CENTER)) == BST_CHECKED ? 1 : 0;
                    state->working.colorMode = 1;
                    state->working.clipToSource = Button_GetCheck(GetDlgItem(hwnd, ID_CLIP_SOURCE)) == BST_CHECKED ? 1 : 0;
                    state->working.preserveSourceAppearance = Button_GetCheck(GetDlgItem(hwnd, ID_PRESERVE_SOURCE)) == BST_CHECKED ? 1 : 0;
                    state->working.followCurve = Button_GetCheck(GetDlgItem(hwnd, ID_FOLLOW_CURVE)) == BST_CHECKED ? 1 : 0;
                    VectorHalftoneApplyPreset(state->working, preset);
                    PopulateFields(hwnd, *state);
                } else {
                    state->working.preset = 0;
                }
                SchedulePreview(hwnd);
                return 0;
            }
            if (id == ID_GLOW_MODE && notification == CBN_SELCHANGE) {
                UpdateDependentControls(hwnd);
            }
            if (id == ID_PREVIEW && notification == BN_CLICKED) {
                if (Button_GetCheck(GetDlgItem(hwnd, ID_PREVIEW)) == BST_CHECKED) SchedulePreview(hwnd);
                else {
                    KillTimer(hwnd, kPreviewTimer);
                    SetStatus(hwnd, L"Preview paused");
                }
                return 0;
            }

            const bool changedEdit = notification == EN_CHANGE;
            const bool changedCombo = notification == CBN_SELCHANGE;
            const bool changedButton = notification == BN_CLICKED;
            if (!state->populating && (changedEdit || changedCombo || changedButton)) {
                if (IsPresetAffectingControl(id)) MarkCustom(hwnd, *state);
                SchedulePreview(hwnd);
            }
        }
        break;

    case WM_CLOSE:
        if (state) {
            state->result = kVectorHalftoneDialogClosed;
            state->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool EnsureWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(nullptr), kWindowClass, &wc)) return true;

    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // WM_ERASEBKGND follows Illustrator's current theme.
    wc.lpszClassName = kWindowClass;
    return RegisterClassExW(&wc) != 0;
}

bool PreferencesSane(const VectorHalftoneParams& p) {
    return std::isfinite(p.glowWidth) && p.glowWidth > 0 &&
           std::isfinite(p.spacing) && p.spacing > 0 &&
           std::isfinite(p.maxSize) && p.maxSize > 0 &&
           p.maxSize >= p.minSize && p.cullSize >= 0.0 && p.shape >= 0 && p.shape <= 6 &&
           p.glowMode >= 0 && p.glowMode <= 1 &&
           p.followCurve >= 0 && p.followCurve <= 1;
}

} // namespace

bool LoadVectorHalftonePreferences(VectorHalftoneParams& params) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    VectorHalftoneParams stored{};
    DWORD type = 0;
    DWORD bytes = static_cast<DWORD>(sizeof(stored));
    const LSTATUS status = RegQueryValueExW(
        key, kRegistryValue, nullptr, &type, reinterpret_cast<LPBYTE>(&stored), &bytes);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_BINARY || bytes != sizeof(stored) || !PreferencesSane(stored))
        return false;
    stored.colorMode = 1;
    params = stored;
    return true;
}

void SaveVectorHalftonePreferences(const VectorHalftoneParams& params) {
    VectorHalftoneParams stored = params;
    stored.colorMode = 1;
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS)
        return;
    RegSetValueExW(key, kRegistryValue, 0, REG_BINARY,
                   reinterpret_cast<const BYTE*>(&stored), static_cast<DWORD>(sizeof(stored)));
    RegCloseKey(key);
}

int ShowVectorHalftoneDialog(HWND parent, VectorHalftoneParams& params,
                             const VectorHalftonePreviewCallback& previewCallback) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    const bool dark = sVectorHalftoneUITheme ? (sVectorHalftoneUITheme->IsUIThemeDark() != 0) : false;
    ResetPalette(dark);

    if (!EnsureWindowClass()) return kVectorHalftoneDialogClosed;

    DialogState state;
    state.initial = params;
    state.working = params;
    state.working.colorMode = 1;
    state.previewCallback = previewCallback;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        kWindowClass,
        L"Vector Halftone",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 552, 704,
        parent, nullptr, GetModuleHandleW(nullptr), &state);

    if (!hwnd) return kVectorHalftoneDialogClosed;
    CentreWindow(hwnd, parent);
    if (parent && IsWindow(parent)) EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (!state.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (parent && IsWindow(parent)) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }

    if (state.result == kVectorHalftoneDialogOK) {
        state.working.colorMode = 1;
        params = state.working;
    }
    return state.result;
}

#endif // WIN_ENV
