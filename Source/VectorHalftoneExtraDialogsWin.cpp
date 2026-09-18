#include "IllustratorSDK.h"
#include "VectorHalftoneExtraDialogsWin.h"
#include "VectorHalftoneEffectSuites.h"

#ifdef WIN_ENV

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace {

constexpr UINT_PTR kPreviewTimer = 4;
constexpr UINT kPreviewDelayMs = 140;

HFONT UiFont() {
    static HFONT font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

HFONT SectionFont() {
    static HFONT font = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : UiFont();
}

struct Palette {
    bool dark = false;
    COLORREF bg = RGB(245,245,245);
    COLORREF field = RGB(255,255,255);
    COLORREF text = RGB(35,35,35);
    HBRUSH bgBrush = nullptr;
    HBRUSH fieldBrush = nullptr;
} gPal;

COLORREF ToColor(const AIUIThemeColor& c) {
    auto cv = [](AIReal v) -> BYTE {
        double x = static_cast<double>(v);
        if (x < 0) x = 0; if (x > 1) x = 1;
        return static_cast<BYTE>(std::lround(x * 255.0));
    };
    return RGB(cv(c.red), cv(c.green), cv(c.blue));
}

void InitPalette() {
    if (gPal.bgBrush) DeleteObject(gPal.bgBrush);
    if (gPal.fieldBrush) DeleteObject(gPal.fieldBrush);
    gPal.dark = sVectorHalftoneUITheme && sVectorHalftoneUITheme->IsUIThemeDark();
    gPal.bg = gPal.dark ? RGB(50,50,50) : RGB(245,245,245);
    gPal.field = gPal.dark ? RGB(68,68,68) : RGB(255,255,255);
    gPal.text = gPal.dark ? RGB(230,230,230) : RGB(35,35,35);
    if (sVectorHalftoneUITheme) {
        AIUIThemeColor c{};
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog, kAIUIComponentColorBackground, c) == kNoErr)
            gPal.bg = ToColor(c);
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog, kAIUIComponentColorText, c) == kNoErr)
            gPal.text = ToColor(c);
        if (sVectorHalftoneUITheme->GetUIThemeColor(kAIUIThemeSelectorDialog, kAIUIComponentColorEditTextBackground, c) == kNoErr)
            gPal.field = ToColor(c);
    }
    gPal.bgBrush = CreateSolidBrush(gPal.bg);
    gPal.fieldBrush = CreateSolidBrush(gPal.field);
}

void ApplyTheme(HWND h) {
    if (!h) return;
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (ux) {
        using Fn = HRESULT (WINAPI*)(HWND,LPCWSTR,LPCWSTR);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(ux, "SetWindowTheme"));
        if (fn) fn(h, gPal.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        FreeLibrary(ux);
    }
}

HWND Add(HWND p, const wchar_t* cls, const wchar_t* text, DWORD style,
         int x,int y,int w,int h,int id, HFONT f=nullptr) {
    HWND c = CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,p,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(f?f:UiFont()),TRUE);
    ApplyTheme(c);
    return c;
}
void Label(HWND p,const wchar_t* t,int x,int y,int w=125) { Add(p,L"STATIC",t,SS_LEFT|SS_NOPREFIX,x,y+2,w,20,0); }
void Unit(HWND p,const wchar_t* t,int x,int y) { Add(p,L"STATIC",t,SS_LEFT|SS_NOPREFIX,x,y+2,35,20,0); }
void Section(HWND p,const wchar_t* t,int y) {
    Add(p,L"STATIC",t,SS_LEFT|SS_NOPREFIX,20,y,145,20,0,SectionFont());
    Add(p,L"STATIC",L"",SS_ETCHEDHORZ,160,y+10,285,2,0);
}
void SetD(HWND h,double v){ wchar_t b[64]{}; swprintf_s(b,L"%.4g",v); SetWindowTextW(h,b); }
bool GetD(HWND h,double& v){ wchar_t b[64]{}; GetWindowTextW(h,b,63); wchar_t* e=nullptr; v=std::wcstod(b,&e); return e!=b&&std::isfinite(v); }
void ComboAdd(HWND h,const wchar_t* t){ SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(t)); }
void Centre(HWND w, HWND parent) {
    RECT r{}, pr{}; GetWindowRect(w,&r);
    if (parent && IsWindow(parent)) GetWindowRect(parent,&pr); else SystemParametersInfoW(SPI_GETWORKAREA,0,&pr,0);
    const int x=pr.left+((pr.right-pr.left)-(r.right-r.left))/2;
    const int y=pr.top+((pr.bottom-pr.top)-(r.bottom-r.top))/2;
    SetWindowPos(w,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
}

// ---------------- Gradient ----------------
enum GId { G_TYPE=2001,G_ANGLE,G_SHAPE,G_SPACING,G_MIN,G_MAX,G_CULL,G_GRID,G_REVERSE,G_STAGGER,G_CLIP,G_PRESERVE,G_PREVIEW,G_STATUS,G_OK,G_CANCEL };
struct GState { VectorHalftoneGradientParams p; GradientPreviewCallback cb; bool done=false; int result=0; bool populating=false; };

bool ReadGradient(HWND h,GState& s,bool final) {
    auto& p=s.p; double d=0;
    p.gradientType=ComboBox_GetCurSel(GetDlgItem(h,G_TYPE))==1?1:0;
    if(!GetD(GetDlgItem(h,G_ANGLE),d)) return false; p.gradientAngle=d;
    p.shape=ComboBox_GetCurSel(GetDlgItem(h,G_SHAPE));
    if(!GetD(GetDlgItem(h,G_SPACING),d)||d<=0) return false; p.spacing=d;
    if(!GetD(GetDlgItem(h,G_MIN),d)||d<0) return false; p.minSize=d;
    if(!GetD(GetDlgItem(h,G_MAX),d)||d<=0||d<p.minSize) return false; p.maxSize=d;
    if(!GetD(GetDlgItem(h,G_CULL),d)||d<0) return false; p.cullSize=d;
    if(!GetD(GetDlgItem(h,G_GRID),d)) return false; p.gridAngle=d;
    p.reverse=Button_GetCheck(GetDlgItem(h,G_REVERSE))==BST_CHECKED;
    p.stagger=Button_GetCheck(GetDlgItem(h,G_STAGGER))==BST_CHECKED;
    p.clipToSource=Button_GetCheck(GetDlgItem(h,G_CLIP))==BST_CHECKED;
    p.preserveSourceAppearance=Button_GetCheck(GetDlgItem(h,G_PRESERVE))==BST_CHECKED;
    if(final) SetWindowTextW(GetDlgItem(h,G_STATUS),L"");
    return true;
}
void FillGradient(HWND h,GState& s){
    s.populating=true;
    ComboBox_SetCurSel(GetDlgItem(h,G_TYPE),s.p.gradientType?1:0); SetD(GetDlgItem(h,G_ANGLE),s.p.gradientAngle);
    ComboBox_SetCurSel(GetDlgItem(h,G_SHAPE),s.p.shape); SetD(GetDlgItem(h,G_SPACING),s.p.spacing); SetD(GetDlgItem(h,G_MIN),s.p.minSize);
    SetD(GetDlgItem(h,G_MAX),s.p.maxSize); SetD(GetDlgItem(h,G_CULL),s.p.cullSize); SetD(GetDlgItem(h,G_GRID),s.p.gridAngle);
    Button_SetCheck(GetDlgItem(h,G_REVERSE),s.p.reverse?BST_CHECKED:BST_UNCHECKED); Button_SetCheck(GetDlgItem(h,G_STAGGER),s.p.stagger?BST_CHECKED:BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(h,G_CLIP),s.p.clipToSource?BST_CHECKED:BST_UNCHECKED); Button_SetCheck(GetDlgItem(h,G_PRESERVE),s.p.preserveSourceAppearance?BST_CHECKED:BST_UNCHECKED);
    s.populating=false;
}
LRESULT CALLBACK GradientProc(HWND h,UINT m,WPARAM w,LPARAM l){
    GState* s=reinterpret_cast<GState*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){ auto* c=reinterpret_cast<CREATESTRUCTW*>(l); s=reinterpret_cast<GState*>(c->lpCreateParams); SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s)); }
    switch(m){
    case WM_CREATE:{
        const int lx=26,ex=150,ux=242; Section(h,L"Gradient",24);
        Label(h,L"Type",lx,54); Add(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,ex,50,190,120,G_TYPE);
        Label(h,L"Gradient angle",lx,86); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,82,84,23,G_ANGLE); Unit(h,L"°",ux,82);
        Add(h,L"BUTTON",L"Reverse gradient",BS_AUTOCHECKBOX|WS_TABSTOP,lx,116,170,22,G_REVERSE);
        Section(h,L"Halftone",154);
        Label(h,L"Mark shape",lx,184); Add(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,ex,180,190,180,G_SHAPE);
        Label(h,L"Spacing",lx,216); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,212,84,23,G_SPACING); Unit(h,L"pt",ux,212);
        Label(h,L"Minimum size",lx,248); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,244,84,23,G_MIN); Unit(h,L"pt",ux,244);
        Label(h,L"Maximum size",lx,280); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,276,84,23,G_MAX); Unit(h,L"pt",ux,276);
        Label(h,L"Cull below",lx,312); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,308,84,23,G_CULL); Unit(h,L"pt",ux,308);
        Label(h,L"Grid angle",lx,344); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,340,84,23,G_GRID); Unit(h,L"°",ux,340);
        Add(h,L"BUTTON",L"Stagger rows",BS_AUTOCHECKBOX|WS_TABSTOP,lx,374,150,22,G_STAGGER);
        Section(h,L"Output",410);
        Add(h,L"BUTTON",L"Clip exactly to source",BS_AUTOCHECKBOX|WS_TABSTOP,lx,440,210,22,G_CLIP);
        Add(h,L"BUTTON",L"Preserve original appearance underneath",BS_AUTOCHECKBOX|WS_TABSTOP,lx,468,310,22,G_PRESERVE);
        Add(h,L"STATIC",L"",SS_ETCHEDHORZ,20,508,425,2,0);
        Add(h,L"BUTTON",L"Preview",BS_AUTOCHECKBOX|WS_TABSTOP,20,526,90,22,G_PREVIEW); Button_SetCheck(GetDlgItem(h,G_PREVIEW),BST_CHECKED);
        Add(h,L"STATIC",L"Preview on",SS_LEFT,115,528,150,20,G_STATUS);
        Add(h,L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,276,520,78,28,G_CANCEL); Add(h,L"BUTTON",L"OK",BS_DEFPUSHBUTTON|WS_TABSTOP,365,520,78,28,G_OK);
        auto type=GetDlgItem(h,G_TYPE); ComboAdd(type,L"Linear"); ComboAdd(type,L"Radial");
        auto shape=GetDlgItem(h,G_SHAPE); const wchar_t* names[]={L"Circle",L"Square",L"Diamond",L"Triangle",L"Hexagon",L"Star",L"Line"}; for(auto n:names) ComboAdd(shape,n);
        if(s){FillGradient(h,*s); SetTimer(h,kPreviewTimer,60,nullptr);} return 0; }
    case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,gPal.bgBrush);return 1;}
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:{HDC d=reinterpret_cast<HDC>(w);SetBkMode(d,TRANSPARENT);SetTextColor(d,gPal.text);return reinterpret_cast<INT_PTR>(gPal.bgBrush);}
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{HDC d=reinterpret_cast<HDC>(w);SetBkColor(d,gPal.field);SetTextColor(d,gPal.text);return reinterpret_cast<INT_PTR>(gPal.fieldBrush);}
    case WM_TIMER: if(s&&w==kPreviewTimer&&Button_GetCheck(GetDlgItem(h,G_PREVIEW))==BST_CHECKED){KillTimer(h,kPreviewTimer);VectorHalftoneGradientParams old=s->p;if(ReadGradient(h,*s,false)&&s->cb&&s->cb(s->p))SetWindowTextW(GetDlgItem(h,G_STATUS),L"Preview on");else{s->p=old;SetWindowTextW(GetDlgItem(h,G_STATUS),L"Invalid settings");}return 0;} break;
    case WM_COMMAND: if(s){int id=LOWORD(w),note=HIWORD(w); if(id==G_OK){if(ReadGradient(h,*s,true)){s->result=2;s->done=true;DestroyWindow(h);}return 0;} if(id==G_CANCEL){s->result=1;s->done=true;DestroyWindow(h);return 0;} if(id==G_PREVIEW&&note==BN_CLICKED){if(Button_GetCheck(GetDlgItem(h,G_PREVIEW))==BST_CHECKED)SetTimer(h,kPreviewTimer,50,nullptr);return 0;} if(!s->populating&&(note==EN_CHANGE||note==CBN_SELCHANGE||note==BN_CLICKED))SetTimer(h,kPreviewTimer,kPreviewDelayMs,nullptr);} break;
    case WM_CLOSE: if(s){s->result=1;s->done=true;}DestroyWindow(h);return 0;
    } return DefWindowProcW(h,m,w,l);
}


// ---------------- Simple Colour Halftone ----------------
// Direct-vector version of the common Illustrator workflow: a gradient or the
// selected artwork controls mark size on a clean regular grid. The plug-in
// creates the final editable circles/shapes directly instead of rasterising,
// tracing, uniting and replacing traced blobs.
enum SId {
    S_SOURCE=4001, S_GRADIENT_TYPE, S_GRADIENT_ANGLE, S_GRADIENT_OFFSET, S_GRADIENT_SCALE,
    S_REVERSE, S_SHAPE, S_SPACING, S_GRID_ANGLE, S_MIN_SIZE, S_MAX_SIZE, S_CULL,
    S_STAGGER, S_CONNECT_STROKE, S_STROKE_WIDTH, S_CLIP, S_PRESERVE,
    S_PREVIEW, S_STATUS, S_BTN_OK, S_BTN_CANCEL
};

struct SimpleUnitInfo {
    double pointsPerUnit = 1.0;
    const wchar_t* label = L"pt";
};

SimpleUnitInfo CurrentDocumentUnit() {
    SimpleUnitInfo out{};
    ai::int16 units = kPointsUnits;
    if (!sVectorHalftoneDocument || sVectorHalftoneDocument->GetDocumentRulerUnits(&units) != kNoErr)
        units = kPointsUnits;
    switch (units) {
    case kInchesUnits:      out.pointsPerUnit=72.0;          out.label=L"in"; break;
    case kCentimetersUnits: out.pointsPerUnit=72.0/2.54;     out.label=L"cm"; break;
    case kPicasUnits:       out.pointsPerUnit=12.0;          out.label=L"pc"; break;
    case kMillimetersUnits: out.pointsPerUnit=72.0/25.4;     out.label=L"mm"; break;
    case kQUnits:           out.pointsPerUnit=72.0/(25.4*4); out.label=L"Q"; break;
    case kFeetInchesUnits:
    case kFeetsUnits:       out.pointsPerUnit=72.0*12.0;     out.label=L"ft"; break;
    case kMetersUnits:      out.pointsPerUnit=72.0/0.0254;   out.label=L"m"; break;
    case kYardsUnits:       out.pointsPerUnit=72.0*36.0;     out.label=L"yd"; break;
    case kPixelsUnits:      out.pointsPerUnit=1.0;           out.label=L"px"; break;
    case kPointsUnits:
    case kUnknownUnits:
    default:                out.pointsPerUnit=1.0;           out.label=L"pt"; break;
    }
    return out;
}

struct SState {
    VectorSimpleColourHalftoneParams p;
    SimpleColourHalftonePreviewCallback cb;
    bool done=false;
    int result=0;
    bool populating=false;
    SimpleUnitInfo unit{};
};

void UpdateSimpleAvailability(HWND h) {
    const int source = ComboBox_GetCurSel(GetDlgItem(h,S_SOURCE));
    const int type = ComboBox_GetCurSel(GetDlgItem(h,S_GRADIENT_TYPE));
    const BOOL gradient = source != 1;
    EnableWindow(GetDlgItem(h,S_GRADIENT_TYPE),gradient);
    EnableWindow(GetDlgItem(h,S_GRADIENT_ANGLE),gradient && type != 1);
    EnableWindow(GetDlgItem(h,S_GRADIENT_OFFSET),gradient);
    EnableWindow(GetDlgItem(h,S_GRADIENT_SCALE),gradient);
    EnableWindow(GetDlgItem(h,S_REVERSE),TRUE);
    const BOOL stroke = Button_GetCheck(GetDlgItem(h,S_CONNECT_STROKE))==BST_CHECKED;
    EnableWindow(GetDlgItem(h,S_STROKE_WIDTH),stroke);
}

bool ReadSimple(HWND h, SState& s, bool final) {
    double d=0;
    s.p.mode=2;
    int source=ComboBox_GetCurSel(GetDlgItem(h,S_SOURCE)); if(source<0)source=0;
    s.p.sourceMode=source;
    int type=ComboBox_GetCurSel(GetDlgItem(h,S_GRADIENT_TYPE)); if(type<0)type=0;
    s.p.gradientType=type;
    int shape=ComboBox_GetCurSel(GetDlgItem(h,S_SHAPE)); if(shape<0)shape=0;
    s.p.shape=shape;

    if(!GetD(GetDlgItem(h,S_GRADIENT_ANGLE),d)) return false; s.p.gradientAngle=d;
    if(!GetD(GetDlgItem(h,S_GRADIENT_OFFSET),d) || d < -100.0 || d > 100.0) return false; s.p.gradientOffset=d;
    if(!GetD(GetDlgItem(h,S_GRADIENT_SCALE),d) || d < 10.0 || d > 400.0) return false; s.p.gradientScale=d;
    s.p.reverse=Button_GetCheck(GetDlgItem(h,S_REVERSE))==BST_CHECKED;

    if(!GetD(GetDlgItem(h,S_SPACING),d) || d<=0) return false; s.p.spacing=d*s.unit.pointsPerUnit;
    if(!GetD(GetDlgItem(h,S_GRID_ANGLE),d)) return false; s.p.gridAngle=d;
    if(!GetD(GetDlgItem(h,S_MIN_SIZE),d) || d<0) return false; s.p.minSize=d*s.unit.pointsPerUnit;
    if(!GetD(GetDlgItem(h,S_MAX_SIZE),d) || d<=0) return false; s.p.maxSize=d*s.unit.pointsPerUnit;
    if(s.p.maxSize < s.p.minSize) return false;
    if(!GetD(GetDlgItem(h,S_CULL),d) || d<0) return false; s.p.cullSize=d*s.unit.pointsPerUnit;
    s.p.stagger=Button_GetCheck(GetDlgItem(h,S_STAGGER))==BST_CHECKED;
    s.p.connectStroke=Button_GetCheck(GetDlgItem(h,S_CONNECT_STROKE))==BST_CHECKED;
    if(!GetD(GetDlgItem(h,S_STROKE_WIDTH),d) || d<0) return false; s.p.strokeWidth=d*s.unit.pointsPerUnit;
    s.p.clipToSource=Button_GetCheck(GetDlgItem(h,S_CLIP))==BST_CHECKED;
    s.p.preserveSourceAppearance=Button_GetCheck(GetDlgItem(h,S_PRESERVE))==BST_CHECKED;

    if(final) SetWindowTextW(GetDlgItem(h,S_STATUS),L"");
    return true;
}

void FillSimple(HWND h, SState& s) {
    s.populating=true;
    ComboBox_SetCurSel(GetDlgItem(h,S_SOURCE),s.p.sourceMode==1?1:0);
    ComboBox_SetCurSel(GetDlgItem(h,S_GRADIENT_TYPE),s.p.gradientType?1:0);
    SetD(GetDlgItem(h,S_GRADIENT_ANGLE),s.p.gradientAngle);
    SetD(GetDlgItem(h,S_GRADIENT_OFFSET),s.p.gradientOffset);
    SetD(GetDlgItem(h,S_GRADIENT_SCALE),s.p.gradientScale);
    Button_SetCheck(GetDlgItem(h,S_REVERSE),s.p.reverse?BST_CHECKED:BST_UNCHECKED);
    ComboBox_SetCurSel(GetDlgItem(h,S_SHAPE),(std::max)(0,(std::min)(6,s.p.shape)));
    SetD(GetDlgItem(h,S_SPACING),s.p.spacing/s.unit.pointsPerUnit);
    SetD(GetDlgItem(h,S_GRID_ANGLE),s.p.gridAngle);
    SetD(GetDlgItem(h,S_MIN_SIZE),s.p.minSize/s.unit.pointsPerUnit);
    SetD(GetDlgItem(h,S_MAX_SIZE),s.p.maxSize/s.unit.pointsPerUnit);
    SetD(GetDlgItem(h,S_CULL),s.p.cullSize/s.unit.pointsPerUnit);
    Button_SetCheck(GetDlgItem(h,S_STAGGER),s.p.stagger?BST_CHECKED:BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(h,S_CONNECT_STROKE),s.p.connectStroke?BST_CHECKED:BST_UNCHECKED);
    SetD(GetDlgItem(h,S_STROKE_WIDTH),s.p.strokeWidth/s.unit.pointsPerUnit);
    Button_SetCheck(GetDlgItem(h,S_CLIP),s.p.clipToSource?BST_CHECKED:BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(h,S_PRESERVE),s.p.preserveSourceAppearance?BST_CHECKED:BST_UNCHECKED);
    UpdateSimpleAvailability(h);
    s.populating=false;
}

LRESULT CALLBACK SimpleProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    SState* s=reinterpret_cast<SState*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){auto* c=reinterpret_cast<CREATESTRUCTW*>(l);s=reinterpret_cast<SState*>(c->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
    switch(m){
    case WM_CREATE:{
        const int lx=26, ex=168, ux=260;
        Section(h,L"Source",22);
        Label(h,L"Size source",lx,54); Add(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,ex,50,190,100,S_SOURCE);
        Label(h,L"Gradient type",lx,86); Add(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,ex,82,190,100,S_GRADIENT_TYPE);
        Label(h,L"Gradient angle",lx,118); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,114,84,23,S_GRADIENT_ANGLE); Unit(h,L"°",ux,114);
        Label(h,L"Tone midpoint",lx,150); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,146,84,23,S_GRADIENT_OFFSET); Unit(h,L"%",ux,146);
        Label(h,L"Tone scale",lx,182); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,178,84,23,S_GRADIENT_SCALE); Unit(h,L"%",ux,178);
        Add(h,L"BUTTON",L"Reverse",BS_AUTOCHECKBOX|WS_TABSTOP,lx,210,120,22,S_REVERSE);

        Section(h,L"Pattern",246);
        Label(h,L"Shape",lx,278); Add(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,ex,274,190,180,S_SHAPE);
        Label(h,L"Grid spacing",lx,310); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,306,84,23,S_SPACING); Unit(h,s?s->unit.label:L"pt",ux,306);
        Label(h,L"Grid angle",lx,342); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,338,84,23,S_GRID_ANGLE); Unit(h,L"°",ux,338);
        Label(h,L"Minimum size",lx,374); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,370,84,23,S_MIN_SIZE); Unit(h,s?s->unit.label:L"pt",ux,370);
        Label(h,L"Maximum size",lx,406); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,402,84,23,S_MAX_SIZE); Unit(h,s?s->unit.label:L"pt",ux,402);
        Label(h,L"Cull below",lx,438); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,434,84,23,S_CULL); Unit(h,s?s->unit.label:L"pt",ux,434);
        Add(h,L"BUTTON",L"Stagger rows",BS_AUTOCHECKBOX|WS_TABSTOP,lx,466,140,22,S_STAGGER);

        Section(h,L"Output",500);
        Add(h,L"BUTTON",L"Connecting stroke",BS_AUTOCHECKBOX|WS_TABSTOP,lx,532,150,22,S_CONNECT_STROKE);
        Label(h,L"Stroke width",lx,564); Add(h,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ex,560,84,23,S_STROKE_WIDTH); Unit(h,s?s->unit.label:L"pt",ux,560);
        Add(h,L"BUTTON",L"Clip exactly to source",BS_AUTOCHECKBOX|WS_TABSTOP,lx,592,210,22,S_CLIP);
        Add(h,L"BUTTON",L"Preserve original appearance underneath",BS_AUTOCHECKBOX|WS_TABSTOP,lx,620,310,22,S_PRESERVE);

        Add(h,L"STATIC",L"",SS_ETCHEDHORZ,20,658,425,2,0);
        Add(h,L"BUTTON",L"Preview",BS_AUTOCHECKBOX|WS_TABSTOP,20,676,90,22,S_PREVIEW); Button_SetCheck(GetDlgItem(h,S_PREVIEW),BST_CHECKED);
        Add(h,L"STATIC",L"Preview on",SS_LEFT,115,678,140,20,S_STATUS);
        Add(h,L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,276,670,78,28,S_BTN_CANCEL);
        Add(h,L"BUTTON",L"OK",BS_DEFPUSHBUTTON|WS_TABSTOP,365,670,78,28,S_BTN_OK);

        auto source=GetDlgItem(h,S_SOURCE); ComboAdd(source,L"Gradient"); ComboAdd(source,L"Artwork / image");
        auto type=GetDlgItem(h,S_GRADIENT_TYPE); ComboAdd(type,L"Linear"); ComboAdd(type,L"Radial");
        auto shape=GetDlgItem(h,S_SHAPE); const wchar_t* names[]={L"Circle",L"Square",L"Diamond",L"Triangle",L"Hexagon",L"Star",L"Line"}; for(auto n:names) ComboAdd(shape,n);
        if(s){FillSimple(h,*s);SetTimer(h,kPreviewTimer,60,nullptr);} return 0;
    }
    case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,gPal.bgBrush);return 1;}
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:{HDC d=reinterpret_cast<HDC>(w);SetBkMode(d,TRANSPARENT);SetTextColor(d,gPal.text);return reinterpret_cast<INT_PTR>(gPal.bgBrush);}
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{HDC d=reinterpret_cast<HDC>(w);SetBkColor(d,gPal.field);SetTextColor(d,gPal.text);return reinterpret_cast<INT_PTR>(gPal.fieldBrush);}
    case WM_TIMER:
        if(s&&w==kPreviewTimer&&Button_GetCheck(GetDlgItem(h,S_PREVIEW))==BST_CHECKED){
            KillTimer(h,kPreviewTimer); auto old=s->p;
            if(ReadSimple(h,*s,false)&&s->cb&&s->cb(s->p)) SetWindowTextW(GetDlgItem(h,S_STATUS),L"Preview on");
            else {s->p=old;SetWindowTextW(GetDlgItem(h,S_STATUS),L"Invalid settings");}
            return 0;
        } break;
    case WM_COMMAND:
        if(s){int id=LOWORD(w),note=HIWORD(w);
            if(id==S_BTN_OK){if(ReadSimple(h,*s,true)){s->result=2;s->done=true;DestroyWindow(h);}return 0;}
            if(id==S_BTN_CANCEL){s->result=1;s->done=true;DestroyWindow(h);return 0;}
            if(id==S_PREVIEW&&note==BN_CLICKED){if(Button_GetCheck(GetDlgItem(h,S_PREVIEW))==BST_CHECKED)SetTimer(h,kPreviewTimer,50,nullptr);return 0;}
            if((id==S_SOURCE||id==S_GRADIENT_TYPE||id==S_CONNECT_STROKE)&&note==BN_CLICKED) UpdateSimpleAvailability(h);
            if((id==S_SOURCE||id==S_GRADIENT_TYPE)&&note==CBN_SELCHANGE) UpdateSimpleAvailability(h);
            if(!s->populating&&(note==EN_CHANGE||note==CBN_SELCHANGE||note==BN_CLICKED)) SetTimer(h,kPreviewTimer,kPreviewDelayMs,nullptr);
        } break;
    case WM_CLOSE: if(s){s->result=1;s->done=true;}DestroyWindow(h);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ---------------- Photoshop Color Halftone ----------------
// This dialog intentionally mirrors Photoshop's Color Halftone dialog rather
// than the suite's larger Illustrator-style dialogs.  Only the five controls
// Photoshop exposes are shown.  Vector-only implementation details (sampling,
// culling, preserving the source appearance) remain internal parameters.
enum PId { P_RADIUS=3001,P_A1,P_A2,P_A3,P_A4,P_OK,P_CANCEL };
struct PState {
    VectorPhotoshopHalftoneParams p;
    PhotoshopHalftonePreviewCallback cb;
    bool done=false;
    int result=0;
    bool populating=false;
    // The renderer remains Photoshop-pixel based for parity.  The dialog can
    // present that physical radius in the Illustrator document's ruler unit.
    double pointsPerDisplayUnit = 1.0;
    const wchar_t* displayUnitLabel = L"(px)";
};

constexpr int kPsDialogW = 364;
constexpr int kPsDialogH = 236;
constexpr int kPsTitleH = 31;
constexpr COLORREF kPsTitle = RGB(82,114,182);
constexpr COLORREF kPsBody = RGB(83,83,83);
constexpr COLORREF kPsField = RGB(69,69,69);
constexpr COLORREF kPsFieldBorder = RGB(104,104,104);
constexpr COLORREF kPsFocusBorder = RGB(20,115,230);
constexpr COLORREF kPsText = RGB(214,214,214);
constexpr COLORREF kPsButtonBorder = RGB(185,185,185);
constexpr COLORREF kPsButtonBorderDim = RGB(112,112,112);

HFONT PhotoshopFont() {
    static HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font ? font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

HBRUSH PhotoshopBodyBrush() {
    static HBRUSH brush = CreateSolidBrush(kPsBody);
    return brush;
}
HBRUSH PhotoshopFieldBrush() {
    static HBRUSH brush = CreateSolidBrush(kPsField);
    return brush;
}

HWND PsAdd(HWND p, const wchar_t* cls, const wchar_t* text, DWORD style,
           int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, p, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(PhotoshopFont()), TRUE);
    return c;
}

void PsLabel(HWND p, const wchar_t* text, int x, int y, int w=150) {
    PsAdd(p, L"STATIC", text, SS_LEFT | SS_NOPREFIX, x, y, w, 17, 0);
}

void PsEditBorder(HDC dc, HWND h, int id, int x, int y, int w, int ht) {
    const bool focused = GetFocus() == GetDlgItem(h, id);
    HPEN pen = CreatePen(PS_SOLID, 1, focused ? kPsFocusBorder : kPsFieldBorder);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, x, y, x + w, y + ht);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void PsRoundRect(HDC dc, RECT r, COLORREF border, COLORREF fill) {
    // Win32 GDI RoundRect is hard-edged, which made the Photoshop-style
    // buttons visibly stair-step at the curved ends. Render the button at 4x
    // into an off-screen bitmap and downsample it with HALFTONE filtering. This
    // keeps the project dependency-free while producing smooth sub-pixel edges.
    constexpr int kScale = 4;
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;

    HDC hi = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w * kScale, h * kScale);
    if (!hi || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (hi) DeleteDC(hi);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(hi, bmp);
    HBRUSH outside = CreateSolidBrush(kPsBody);
    RECT full{0, 0, w * kScale, h * kScale};
    FillRect(hi, &full, outside);
    DeleteObject(outside);

    HPEN pen = CreatePen(PS_SOLID, kScale, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(hi, pen);
    HGDIOBJ oldBrush = SelectObject(hi, brush);

    const int inset = kScale;
    RoundRect(hi,
        inset, inset,
        w * kScale - inset, h * kScale - inset,
        22 * kScale, 22 * kScale);

    SelectObject(hi, oldBrush);
    SelectObject(hi, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    const int oldMode = SetStretchBltMode(dc, HALFTONE);
    POINT oldOrigin{};
    SetBrushOrgEx(dc, 0, 0, &oldOrigin);
    StretchBlt(dc, r.left, r.top, w, h,
               hi, 0, 0, w * kScale, h * kScale, SRCCOPY);
    SetBrushOrgEx(dc, oldOrigin.x, oldOrigin.y, nullptr);
    SetStretchBltMode(dc, oldMode);

    SelectObject(hi, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hi);
}


void ConfigurePhotoshopDisplayUnit(PState& s) {
    // maxRadius is stored in Photoshop raster pixels.  Convert those pixels to
    // document points with 72/sampleDpi, then present the value using the
    // current Illustrator ruler unit.  This is display-only: rendering still
    // receives the exact same Photoshop-pixel radius.
    ai::int16 units = kPixelsUnits;
    if (!sVectorHalftoneDocument || sVectorHalftoneDocument->GetDocumentRulerUnits(&units) != kNoErr)
        units = kPixelsUnits;

    switch (units) {
    case kInchesUnits:
        s.pointsPerDisplayUnit = 72.0; s.displayUnitLabel = L"(in)"; break;
    case kCentimetersUnits:
        s.pointsPerDisplayUnit = 72.0 / 2.54; s.displayUnitLabel = L"(cm)"; break;
    case kPointsUnits:
        s.pointsPerDisplayUnit = 1.0; s.displayUnitLabel = L"(pt)"; break;
    case kPicasUnits:
        s.pointsPerDisplayUnit = 12.0; s.displayUnitLabel = L"(pc)"; break;
    case kMillimetersUnits:
        s.pointsPerDisplayUnit = 72.0 / 25.4; s.displayUnitLabel = L"(mm)"; break;
    case kQUnits:
        // 1 Q = 0.25 mm.
        s.pointsPerDisplayUnit = 72.0 / (25.4 * 4.0); s.displayUnitLabel = L"(Q)"; break;
    case kFeetInchesUnits:
        // A single radius field cannot express Illustrator's compound feet/inches
        // notation cleanly, so present the scalar as decimal feet.
        s.pointsPerDisplayUnit = 72.0 * 12.0; s.displayUnitLabel = L"(ft)"; break;
    case kMetersUnits:
        s.pointsPerDisplayUnit = 72.0 / 0.0254; s.displayUnitLabel = L"(m)"; break;
    case kYardsUnits:
        s.pointsPerDisplayUnit = 72.0 * 36.0; s.displayUnitLabel = L"(yd)"; break;
    case kFeetsUnits:
        s.pointsPerDisplayUnit = 72.0 * 12.0; s.displayUnitLabel = L"(ft)"; break;
    case kPixelsUnits:
    case kUnknownUnits:
    default:
        // Illustrator's pixel ruler is 72 px/in in document coordinates.
        s.pointsPerDisplayUnit = 1.0; s.displayUnitLabel = L"(px)"; break;
    }
}

double PhotoshopPixelsToDisplay(const PState& s, double pixels) {
    const double dpi = s.p.sampleDpi > 0.0 ? s.p.sampleDpi : 72.0;
    const double points = pixels * 72.0 / dpi;
    return points / s.pointsPerDisplayUnit;
}

double PhotoshopDisplayToPixels(const PState& s, double displayValue) {
    const double dpi = s.p.sampleDpi > 0.0 ? s.p.sampleDpi : 72.0;
    const double points = displayValue * s.pointsPerDisplayUnit;
    return points * dpi / 72.0;
}

void DrawPhotoshopButton(const DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    // An owner-drawn Win32 BUTTON still gets a normal control background before
    // WM_DRAWITEM.  The rounded button only covers the middle of that rectangle,
    // so the untouched corner pixels showed through as bright/jagged wedges on
    // some Windows/Illustrator themes.  Paint the whole control rectangle with
    // the Photoshop dialog body first, then draw the rounded button on top.
    HBRUSH outside = CreateSolidBrush(kPsBody);
    FillRect(dis->hDC, &dis->rcItem, outside);
    DeleteObject(outside);

    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool focused = (dis->itemState & ODS_FOCUS) != 0;

    RECT button = dis->rcItem;
    InflateRect(&button, -1, -1);

    COLORREF border = dis->CtlID == P_OK ? kPsButtonBorder : kPsButtonBorderDim;
    COLORREF fill = pressed ? RGB(96,96,96) : kPsBody;
    if (focused && !disabled) border = RGB(210,210,210);
    if (disabled) border = RGB(100,100,100);

    PsRoundRect(dis->hDC, button, border, fill);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, disabled ? RGB(145,145,145) : RGB(238,238,238));
    SelectObject(dis->hDC, PhotoshopFont());
    wchar_t text[32]{};
    GetWindowTextW(dis->hwndItem, text, 31);

    RECT textRect = button;
    if (pressed) OffsetRect(&textRect, 0, 1);
    DrawTextW(dis->hDC, text, -1, &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

bool ReadPhotoshop(HWND h, PState& s, bool) {
    double d = 0;
    if (!GetD(GetDlgItem(h,P_RADIUS),d) || d <= 0) return false;
    const double radiusPixels = PhotoshopDisplayToPixels(s, d);
    // Photoshop's native Max Radius contract remains 4..127 pixels even when
    // the UI is displaying another Illustrator document unit.
    if (radiusPixels < 4.0 || radiusPixels > 127.0) return false;
    s.p.maxRadius = radiusPixels;
    if (!GetD(GetDlgItem(h,P_A1),d)) return false; s.p.angle1 = d;
    if (!GetD(GetDlgItem(h,P_A2),d)) return false; s.p.angle2 = d;
    if (!GetD(GetDlgItem(h,P_A3),d)) return false; s.p.angle3 = d;
    if (!GetD(GetDlgItem(h,P_A4),d)) return false; s.p.angle4 = d;
    // Photoshop does not expose our vector-only implementation controls.
    // Leave cullSize/sampleDpi/preserveSourceAppearance unchanged.
    return true;
}

void FillPhotoshop(HWND h, PState& s) {
    s.populating = true;
    SetD(GetDlgItem(h,P_RADIUS),PhotoshopPixelsToDisplay(s,s.p.maxRadius));
    SetD(GetDlgItem(h,P_A1),s.p.angle1);
    SetD(GetDlgItem(h,P_A2),s.p.angle2);
    SetD(GetDlgItem(h,P_A3),s.p.angle3);
    SetD(GetDlgItem(h,P_A4),s.p.angle4);
    s.populating = false;
}

LRESULT CALLBACK PhotoshopProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    PState* s = reinterpret_cast<PState*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        auto* c = reinterpret_cast<CREATESTRUCTW*>(l);
        s = reinterpret_cast<PState*>(c->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
    }

    switch (m) {
    case WM_CREATE: {
        // Photoshop 2026 Color Halftone layout, measured from the supplied
        // reference screenshot at 100% Windows scaling.
        PsLabel(h, L"Max. Radius:", 9, 43, 72);
        PsAdd(h, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 83, 40, 104, 19, P_RADIUS);
        PsLabel(h, s ? s->displayUnitLabel : L"(px)", 196, 43, 55);

        PsLabel(h, L"Screen Angles (Degrees):", 12, 72, 190);

        PsLabel(h, L"Channel 1:", 17, 99, 64);
        PsAdd(h, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 82, 95, 103, 19, P_A1);
        PsLabel(h, L"Channel 2:", 17, 134, 64);
        PsAdd(h, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 82, 130, 103, 19, P_A2);
        PsLabel(h, L"Channel 3:", 17, 169, 64);
        PsAdd(h, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 82, 165, 103, 19, P_A3);
        PsLabel(h, L"Channel 4:", 17, 204, 64);
        PsAdd(h, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 82, 200, 103, 19, P_A4);

        PsAdd(h, L"BUTTON", L"OK", BS_OWNERDRAW | WS_TABSTOP, 256, 45, 91, 25, P_OK);
        PsAdd(h, L"BUTTON", L"Cancel", BS_OWNERDRAW | WS_TABSTOP, 256, 80, 91, 25, P_CANCEL);

        if (s) FillPhotoshop(h,*s);
        SetFocus(GetDlgItem(h,P_RADIUS));
        SendMessageW(GetDlgItem(h,P_RADIUS), EM_SETSEL, 0, -1);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h,&ps);
        RECT r{}; GetClientRect(h,&r);
        HBRUSH body = PhotoshopBodyBrush();
        FillRect(dc,&r,body);
        RECT title{0,0,r.right,kPsTitleH};
        HBRUSH tb = CreateSolidBrush(kPsTitle);
        FillRect(dc,&title,tb);
        DeleteObject(tb);

        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,RGB(250,250,250));
        SelectObject(dc,PhotoshopFont());
        RECT tr{10,7,250,28};
        DrawTextW(dc,L"Color Halftone",-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

        // Adobe-style close glyph.
        HPEN closePen = CreatePen(PS_SOLID,1,RGB(245,245,245));
        HGDIOBJ oldPen = SelectObject(dc,closePen);
        MoveToEx(dc,343,11,nullptr); LineTo(dc,353,21);
        MoveToEx(dc,353,11,nullptr); LineTo(dc,343,21);
        SelectObject(dc,oldPen); DeleteObject(closePen);

        PsEditBorder(dc,h,P_RADIUS,82,39,106,21);
        PsEditBorder(dc,h,P_A1,81,94,105,21);
        PsEditBorder(dc,h,P_A2,81,129,105,21);
        PsEditBorder(dc,h,P_A3,81,164,105,21);
        PsEditBorder(dc,h,P_A4,81,199,105,21);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(w);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,kPsText);
        return reinterpret_cast<INT_PTR>(PhotoshopBodyBrush());
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(w);
        SetBkColor(dc,kPsField);
        SetTextColor(dc,RGB(225,225,225));
        return reinterpret_cast<INT_PTR>(PhotoshopFieldBrush());
    }
    case WM_DRAWITEM:
        DrawPhotoshopButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));
        return TRUE;
    case WM_COMMAND:
        if (s) {
            const int id = LOWORD(w);
            const int note = HIWORD(w);
            if (id == P_OK) {
                if (ReadPhotoshop(h,*s,true)) {
                    s->result = 2; s->done = true; DestroyWindow(h);
                } else {
                    MessageBeep(MB_ICONWARNING);
                }
                return 0;
            }
            if (id == P_CANCEL) {
                s->result = 1; s->done = true; DestroyWindow(h); return 0;
            }
            if (note == EN_SETFOCUS || note == EN_KILLFOCUS) {
                InvalidateRect(h,nullptr,FALSE);
                return 0;
            }
        }
        break;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE && s) { s->result=1; s->done=true; DestroyWindow(h); return 0; }
        break;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (y < kPsTitleH) {
            if (x >= 334) {
                if (s) { s->result=1; s->done=true; }
                DestroyWindow(h);
            } else {
                ReleaseCapture();
                SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (s) { s->result=1; s->done=true; }
        DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

bool EnsureClass(const wchar_t* name, WNDPROC proc){WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);if(GetClassInfoExW(GetModuleHandleW(nullptr),name,&wc))return true;wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.style=CS_DROPSHADOW;wc.lpszClassName=name;return RegisterClassExW(&wc)!=0;}

template<typename State>
int RunModal(HWND parent,const wchar_t* cls,const wchar_t* title,int w,int h,State& state,WNDPROC proc){
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES};InitCommonControlsEx(&icc);InitPalette();if(!EnsureClass(cls,proc))return 0;HWND wnd=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,cls,title,WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,w,h,parent,nullptr,GetModuleHandleW(nullptr),&state);if(!wnd)return 0;Centre(wnd,parent);if(parent&&IsWindow(parent))EnableWindow(parent,FALSE);ShowWindow(wnd,SW_SHOW);UpdateWindow(wnd);MSG msg{};while(!state.done&&GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(wnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(parent&&IsWindow(parent)){EnableWindow(parent,TRUE);SetForegroundWindow(parent);}return state.result;
}

} // namespace

int ShowVectorHalftoneGradientDialog(HWND parent, VectorHalftoneGradientParams& params,
                                     const GradientPreviewCallback& previewCallback) {
    GState s; s.p=params; s.cb=previewCallback; int r=RunModal(parent,L"VectorHalftoneGradientDialogV40",L"Halftone Gradient",476,602,s,GradientProc); if(r==2)params=s.p;return r;
}

int ShowVectorSimpleColourHalftoneDialog(HWND parent, VectorSimpleColourHalftoneParams& params,
                                         const SimpleColourHalftonePreviewCallback& previewCallback) {
    SState s; s.p=params; s.cb=previewCallback; s.unit=CurrentDocumentUnit();
    int r=RunModal(parent,L"VectorSimpleColourHalftoneDialogV70",L"Simple Colour Halftone",476,748,s,SimpleProc);
    if(r==2) params=s.p;
    return r;
}

int ShowVectorPhotoshopHalftoneDialog(HWND parent, VectorPhotoshopHalftoneParams& params,
                                      const PhotoshopHalftonePreviewCallback& previewCallback) {
    PState s; s.p=params; s.cb=previewCallback; ConfigurePhotoshopDisplayUnit(s);
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icc);
    if(!EnsureClass(L"VectorPhotoshopColorHalftoneDialogV52",PhotoshopProc)) return 0;
    HWND wnd=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_TOOLWINDOW,
        L"VectorPhotoshopColorHalftoneDialogV52",L"",
        WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,kPsDialogW,kPsDialogH,
        parent,nullptr,GetModuleHandleW(nullptr),&s);
    if(!wnd) return 0;
    HRGN region=CreateRoundRectRgn(0,0,kPsDialogW+1,kPsDialogH+1,11,11);
    SetWindowRgn(wnd,region,TRUE); // system owns region after success
    Centre(wnd,parent);
    if(parent&&IsWindow(parent)) EnableWindow(parent,FALSE);
    ShowWindow(wnd,SW_SHOW); UpdateWindow(wnd);
    MSG msg{};
    while(!s.done&&GetMessageW(&msg,nullptr,0,0)>0){
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){
            SendMessageW(wnd,WM_COMMAND,MAKEWPARAM(P_OK,BN_CLICKED),0);
            continue;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){
            SendMessageW(wnd,WM_COMMAND,MAKEWPARAM(P_CANCEL,BN_CLICKED),0);
            continue;
        }
        if(!IsDialogMessageW(wnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    if(parent&&IsWindow(parent)){EnableWindow(parent,TRUE);SetForegroundWindow(parent);}
    if(s.result==2) params=s.p;
    return s.result;
}

#endif
