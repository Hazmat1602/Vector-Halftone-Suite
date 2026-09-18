#define _USE_MATH_DEFINES
#include "IllustratorSDK.h"
#include "VectorHalftoneEffectPlugin.h"
#include "VectorHalftoneEffectSuites.h"
#include "VectorHalftoneGeometry.h"
#include "VectorHalftoneDialogWin.h"
#include "VectorHalftoneExtraDialogsWin.h"
#include "SDKErrors.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <vector>

#ifndef CHKERR
#define CHKERR aisdk::check_ai_error(error)
#endif

namespace {

constexpr const char* K_GLOW_WIDTH = "vigh.glowWidth";
constexpr const char* K_GLOW_STRENGTH = "vigh.glowStrength";
constexpr const char* K_GAMMA = "vigh.gamma";
constexpr const char* K_INVERT = "vigh.invert";
constexpr const char* K_SHAPE = "vigh.shape";
constexpr const char* K_SPACING = "vigh.spacing";
constexpr const char* K_MIN_SIZE = "vigh.minSize";
constexpr const char* K_MAX_SIZE = "vigh.maxSize";
constexpr const char* K_CULL_SIZE = "vigh.cullSize";
constexpr const char* K_GRID_ANGLE = "vigh.gridAngle";
constexpr const char* K_STAGGER = "vigh.stagger";
constexpr const char* K_CURVE_SAMPLES = "vigh.curveSamples";
constexpr const char* K_CONTAIN_DOTS = "vigh.containDots";
constexpr const char* K_GLOW_MODE = "vigh.glowMode";
constexpr const char* K_EDGE_SOFTNESS = "vigh.edgeSoftness";
constexpr const char* K_COLOR_MODE = "vigh.colorMode";
constexpr const char* K_CLIP_SOURCE = "vigh.clipToSource";
constexpr const char* K_PRESERVE_SOURCE = "vigh.preserveSource";
constexpr const char* K_SOLID_CENTER = "vigh.solidCenter";
constexpr const char* K_FOLLOW_CURVE = "vigh.followCurve";
constexpr const char* K_PRESET = "vigh.preset";

constexpr const char* KG_TYPE = "vhg.type";
constexpr const char* KG_ANGLE = "vhg.angle";
constexpr const char* KG_SPACING = "vhg.spacing";
constexpr const char* KG_MIN_SIZE = "vhg.minSize";
constexpr const char* KG_MAX_SIZE = "vhg.maxSize";
constexpr const char* KG_CULL_SIZE = "vhg.cullSize";
constexpr const char* KG_GRID_ANGLE = "vhg.gridAngle";
constexpr const char* KG_SHAPE = "vhg.shape";
constexpr const char* KG_REVERSE = "vhg.reverse";
constexpr const char* KG_STAGGER = "vhg.stagger";
constexpr const char* KG_CLIP = "vhg.clip";
constexpr const char* KG_PRESERVE = "vhg.preserve";

constexpr const char* KS_MODE = "vhs.mode";
constexpr const char* KS_SOURCE_MODE = "vhs.sourceMode";
constexpr const char* KS_GRADIENT_TYPE = "vhs.gradientType";
constexpr const char* KS_GRADIENT_ANGLE = "vhs.gradientAngle";
constexpr const char* KS_GRADIENT_OFFSET = "vhs.gradientOffset";
constexpr const char* KS_GRADIENT_SCALE = "vhs.gradientScale";
constexpr const char* KS_REVERSE = "vhs.reverse";
constexpr const char* KS_MIN_SIZE = "vhs.minSize";
constexpr const char* KS_MAX_SIZE = "vhs.maxSize";
constexpr const char* KS_CULL_SIZE = "vhs.cullSize";
constexpr const char* KS_CONNECT_STROKE = "vhs.connectStroke";
constexpr const char* KS_STROKE_WIDTH = "vhs.strokeWidth";
constexpr const char* KS_MAX_RADIUS = "vhs.maxRadius";
constexpr const char* KS_SCREEN_ANGLE = "vhs.screenAngle";
constexpr const char* KS_SHADE_STRENGTH = "vhs.shadeStrength";
// Legacy v0.5 flat-pattern keys.
constexpr const char* KS_SHAPE = "vhs.shape";
constexpr const char* KS_SIZE = "vhs.markSize";
constexpr const char* KS_SPACING = "vhs.spacing";
constexpr const char* KS_ANGLE = "vhs.gridAngle";
constexpr const char* KS_OPACITY = "vhs.opacity";
constexpr const char* KS_STAGGER = "vhs.stagger";
constexpr const char* KS_CLIP = "vhs.clip";
constexpr const char* KS_PRESERVE = "vhs.preserve";

constexpr const char* KP_RADIUS = "vhp.maxRadius";
constexpr const char* KP_A1 = "vhp.angle1";
constexpr const char* KP_A2 = "vhp.angle2";
constexpr const char* KP_A3 = "vhp.angle3";
constexpr const char* KP_A4 = "vhp.angle4";
constexpr const char* KP_CULL = "vhp.cullSize";
constexpr const char* KP_DPI = "vhp.sampleDpi";
constexpr const char* KP_PRESERVE = "vhp.preserve";

AIReal Clamp01(AIReal v) {
    return (std::max)(static_cast<AIReal>(0), (std::min)(static_cast<AIReal>(1), v));
}

AIReal SoftRamp(AIReal t, AIReal softness) {
    t = Clamp01(t);
    softness = Clamp01(softness);
    const AIReal smooth = t * t * (static_cast<AIReal>(3) - static_cast<AIReal>(2) * t);
    return t + (smooth - t) * softness;
}


// Photoshop Color Halftone calibration (Photoshop 2026, RGB, 72 ppi):
// - "Max Radius" is the distance from a screen-cell centre to its corner.
// - Therefore a square screen cell has side maxRadius * sqrt(2).
// - Each channel is screened as a periodic field of circular DARK dots.
// - Dot area (including overlap with neighbouring cells at high coverage)
//   preserves the channel's darkness/ink coverage.
//
// For coverage <= pi/4 the circle fits inside its square cell and the inverse
// area is analytic. Above pi/4 the circle crosses the four cell edges, so we
// invert the disk/square intersection area with a short bisection. At coverage
// 1 the radius reaches maxRadius and neighbouring circles meet at cell corners.
double PhotoshopDotRadiusPixels(double coverage, double maxRadius) {
    coverage = (std::max)(0.0, (std::min)(1.0, coverage));
    if (coverage <= 0.0 || maxRadius <= 0.0) return 0.0;
    if (coverage >= 1.0) return maxRadius;

    const double cell = maxRadius * std::sqrt(2.0);
    const double half = cell * 0.5;
    const double quarterCircleCoverage = M_PI / 4.0;

    if (coverage <= quarterCircleCoverage) {
        return cell * std::sqrt(coverage / M_PI);
    }

    const double targetArea = coverage * cell * cell;
    double lo = half;
    double hi = maxRadius;
    for (int i = 0; i < 28; ++i) {
        const double r = (lo + hi) * 0.5;
        const double root = std::sqrt((std::max)(0.0, r * r - half * half));
        const double segment = r * r * std::acos(half / r) - half * root;
        const double areaInsideCell = M_PI * r * r - 4.0 * segment;
        if (areaInsideCell < targetArea) lo = r;
        else hi = r;
    }
    return (lo + hi) * 0.5;
}

// Fast path used by Color Halftone, which creates thousands of circles.
// VHCreateMark is intentionally generic and rebuilds mark styling/temporary
// vectors for every mark. Photoshop Color Halftone only needs circles with an
// already-final path style, so avoid those allocations and redundant style
// conversion while producing the exact same four-point Bezier circle.
AIErr CreateFastHalftoneCircle(AIArtHandle parentGroup, AIReal cx, AIReal cy,
                               AIReal diameter, AIPathStyle& style) {
    if (!parentGroup || diameter <= static_cast<AIReal>(0.01)) return kNoErr;

    const AIReal r = diameter * static_cast<AIReal>(0.5);
    const AIReal h = r * static_cast<AIReal>(0.5522847498307936);
    AIPathSegment seg[4]{};

    auto setPoint = [](AIPathSegment& s, AIReal x, AIReal y) {
        s.p.h = x; s.p.v = y;
        s.in = s.p; s.out = s.p;
        s.corner = false;
    };

    setPoint(seg[0], cx, cy + r);
    seg[0].in.h = cx - h; seg[0].in.v = cy + r;
    seg[0].out.h = cx + h; seg[0].out.v = cy + r;

    setPoint(seg[1], cx + r, cy);
    seg[1].in.h = cx + r; seg[1].in.v = cy + h;
    seg[1].out.h = cx + r; seg[1].out.v = cy - h;

    setPoint(seg[2], cx, cy - r);
    seg[2].in.h = cx + h; seg[2].in.v = cy - r;
    seg[2].out.h = cx - h; seg[2].out.v = cy - r;

    setPoint(seg[3], cx - r, cy);
    seg[3].in.h = cx - r; seg[3].in.v = cy - h;
    seg[3].out.h = cx - r; seg[3].out.v = cy + h;

    AIArtHandle art = nullptr;
    AIErr error = sAIArt->NewArt(kPathArt, kPlaceInsideOnTop, parentGroup, &art);
    if (error != kNoErr) return error;
    error = sAIPath->SetPathSegmentCount(art, 4);
    if (error == kNoErr) error = sAIPath->SetPathSegments(art, 0, 4, seg);
    if (error == kNoErr) error = sAIPath->SetPathClosed(art, true);
    if (error == kNoErr) error = sAIPathStyle->SetPathStyle(art, &style);
    if (error != kNoErr && art) sAIArt->DisposeArt(art);
    return error;
}

AIReal ReadRealOr(const AILiveEffectParameters& dict, const char* keyName, AIReal fallback) {
    const AIDictKey key = sAIDictionary->Key(keyName);
    if (!sAIDictionary->IsKnown(dict, key)) return fallback;
    AIReal v = fallback;
    if (sAIDictionary->GetRealEntry(dict, key, &v) != kNoErr) return fallback;
    return v;
}

ai::int32 ReadIntOr(const AILiveEffectParameters& dict, const char* keyName, ai::int32 fallback) {
    const AIDictKey key = sAIDictionary->Key(keyName);
    if (!sAIDictionary->IsKnown(dict, key)) return fallback;
    ai::int32 v = fallback;
    if (sAIDictionary->GetIntegerEntry(dict, key, &v) != kNoErr) return fallback;
    return v;
}

AIErr WriteReal(const AILiveEffectParameters& dict, const char* keyName, AIReal value) {
    return sAIDictionary->SetRealEntry(dict, sAIDictionary->Key(keyName), value);
}

AIErr WriteInt(const AILiveEffectParameters& dict, const char* keyName, ai::int32 value) {
    return sAIDictionary->SetIntegerEntry(dict, sAIDictionary->Key(keyName), value);
}

struct LocalBounds {
    AIReal minX = 0;
    AIReal maxX = 0;
    AIReal minY = 0;
    AIReal maxY = 0;
};

LocalBounds RotatedLocalBounds(AIReal left, AIReal top, AIReal right, AIReal bottom,
                               AIReal cx, AIReal cy, AIReal cosA, AIReal sinA) {
    const AIReal xs[4] = {left, right, right, left};
    const AIReal ys[4] = {top, top, bottom, bottom};
    LocalBounds b{};
    for (int i = 0; i < 4; ++i) {
        const AIReal dx = xs[i] - cx;
        const AIReal dy = ys[i] - cy;
        // inverse rotate world -> local grid space
        const AIReal lx = dx * cosA + dy * sinA;
        const AIReal ly = -dx * sinA + dy * cosA;
        if (i == 0) {
            b.minX = b.maxX = lx;
            b.minY = b.maxY = ly;
        } else {
            b.minX = (std::min)(b.minX, lx);
            b.maxX = (std::max)(b.maxX, lx);
            b.minY = (std::min)(b.minY, ly);
            b.maxY = (std::max)(b.maxY, ly);
        }
    }
    return b;
}


AIErr CreateFilledRectangle(AIArtHandle parent, const AIRealRect& bounds,
                            const AIPathStyle& style, AIArtHandle& outArt) {
    outArt = nullptr;
    AIErr error = sAIArt->NewArt(kPathArt, kPlaceInsideOnBottom, parent, &outArt);
    if (error != kNoErr) return error;

    AIPathSegment segs[4]{};
    const AIReal xs[4] = {bounds.left, bounds.right, bounds.right, bounds.left};
    const AIReal ys[4] = {bounds.top, bounds.top, bounds.bottom, bounds.bottom};
    for (int i = 0; i < 4; ++i) {
        segs[i].p.h = xs[i];
        segs[i].p.v = ys[i];
        segs[i].in = segs[i].p;
        segs[i].out = segs[i].p;
        segs[i].corner = true;
    }

    error = sAIPath->SetPathSegmentCount(outArt, 4);
    if (error != kNoErr) return error;
    error = sAIPath->SetPathSegments(outArt, 0, 4, segs);
    if (error != kNoErr) return error;
    error = sAIPath->SetPathClosed(outArt, true);
    if (error != kNoErr) return error;
    return sAIPathStyle->SetPathStyle(outArt, const_cast<AIPathStyle*>(&style));
}

struct VHQuantKey {
    std::int64_t x = 0;
    std::int64_t y = 0;

    bool operator==(const VHQuantKey& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct VHQuantKeyHash {
    std::size_t operator()(const VHQuantKey& key) const noexcept {
        const std::uint64_t a = static_cast<std::uint64_t>(key.x);
        const std::uint64_t b = static_cast<std::uint64_t>(key.y);
        return static_cast<std::size_t>(a * 0x9E3779B185EBCA87ULL ^ (b + 0xC2B2AE3D27D4EB4FULL + (a << 6) + (a >> 2)));
    }
};

AIErr CollectHalftoneTargets(AIArtHandle art, std::vector<AIArtHandle>& targets) {
    if (!art) return kBadParameterErr;

    short type = 0;
    AIErr error = sAIArt->GetArtType(art, &type);
    if (error != kNoErr) return error;

    // Compound paths must stay intact so holes are evaluated as one shape.
    if (type == kPathArt || type == kCompoundPathArt) {
        targets.push_back(art);
        return kNoErr;
    }

    // Groups do not have one useful fill colour. Render their painted contents
    // independently so every child inherits its own fill/stroke and overlapping
    // sibling shapes are not accidentally interpreted as even/odd holes.
    if (type == kGroupArt || type == kPluginArt) {
        AIArtHandle child = nullptr;
        error = sAIArt->GetArtFirstChild(art, &child);
        if (error != kNoErr) return error;
        while (child) {
            error = CollectHalftoneTargets(child, targets);
            if (error != kNoErr) return error;
            AIArtHandle next = nullptr;
            error = sAIArt->GetArtSibling(child, &next);
            if (error != kNoErr) return error;
            child = next;
        }
    }
    return kNoErr;
}


AIColor SolidGrayColor(AIReal gray) {
    AIColor color{};
    color.kind = kGrayColor;
    color.c.g.gray = Clamp01(gray); // Illustrator gray: 0 white, 1 black.
    return color;
}

AIPathStyle MakeMonochromeStyle(const AIPathStyle& source, AIReal gray) {
    AIPathStyle style = source;
    const AIColor c = SolidGrayColor(gray);
    const bool hadFill = source.fillPaint && source.fill.color.kind != kNoneColor;
    const bool hadStroke = source.strokePaint && source.stroke.color.kind != kNoneColor;

    // Match the source silhouette: filled objects stay filled, stroke-only art
    // stays stroked. If Illustrator hands us an unpainted style, use a fill so
    // the Live Effect remains visible instead of silently producing nothing.
    style.fillPaint = hadFill || !hadStroke;
    if (style.fillPaint) style.fill.color = c;
    style.strokePaint = hadStroke;
    if (style.strokePaint) style.stroke.color = c;
    return style;
}

AIErr ApplyPathStyleRecursive(AIArtHandle art, const AIPathStyle& style) {
    if (!art) return kBadParameterErr;
    short type = 0;
    AIErr error = sAIArt->GetArtType(art, &type);
    if (error != kNoErr) return error;

    if (type == kPathArt) {
        return sAIPathStyle->SetPathStyle(art, const_cast<AIPathStyle*>(&style));
    }

    if (type == kCompoundPathArt || type == kGroupArt || type == kPluginArt) {
        // Some compound paths accept a style directly, but their actual paint
        // can live on the children. Walk the tree so the result is deterministic.
        AIArtHandle child = nullptr;
        error = sAIArt->GetArtFirstChild(art, &child);
        if (error != kNoErr) return error;
        while (child) {
            error = ApplyPathStyleRecursive(child, style);
            if (error != kNoErr) return error;
            AIArtHandle next = nullptr;
            error = sAIArt->GetArtSibling(child, &next);
            if (error != kNoErr) return error;
            child = next;
        }
        return kNoErr;
    }
    return kNoErr;
}

AIPathStyle SolidDotStyle(AIReal gray) {
    AIPathStyle style;
    style.Init();
    style.fillPaint = true;
    style.strokePaint = false;
    style.fill.color = SolidGrayColor(gray);
    return style;
}


// A duplicated source object is used only as clipping geometry. Illustrator can
// retain the duplicate's paint/appearance, especially when the effect is running
// post-style. Strip ordinary path paint recursively so enabling Solid Centre or
// Exact Clip never paints a second full-size copy of the source artwork.
AIErr MakeClipMaskNonPainting(AIArtHandle art) {
    if (!art) return kBadParameterErr;

    short type = 0;
    AIErr error = sAIArt->GetArtType(art, &type);
    if (error != kNoErr) return error;

    if (type == kPathArt || type == kCompoundPathArt) {
        AIPathStyle style;
        style.Init();
        AIBoolean hasAdvFill = false;
        if (sAIPathStyle->GetPathStyle(art, &style, &hasAdvFill) != kNoErr)
            style.Init();
        style.fillPaint = false;
        style.strokePaint = false;
        error = sAIPathStyle->SetPathStyle(art, &style);
        if (error != kNoErr) return error;
    }

    if (type == kCompoundPathArt || type == kGroupArt || type == kPluginArt) {
        AIArtHandle child = nullptr;
        error = sAIArt->GetArtFirstChild(art, &child);
        if (error != kNoErr) return error;
        while (child) {
            AIArtHandle next = nullptr;
            error = sAIArt->GetArtSibling(child, &next);
            if (error != kNoErr) return error;
            error = MakeClipMaskNonPainting(child);
            if (error != kNoErr) return error;
            child = next;
        }
    }

    return kNoErr;
}


struct VHEdgeSample {
    AIReal x = 0;
    AIReal y = 0;
    AIReal inX = 0;
    AIReal inY = 0;
    AIReal normalAngle = 0;
};

void BuildRingEdgeSamples(const VHRing& ring, const VHGeometry& geometry,
                          AIReal requestedSpacing, AIReal phase,
                          AIReal probe, std::vector<VHEdgeSample>& out) {
    out.clear();
    const size_t n = ring.points.size();
    if (n < 3 || requestedSpacing <= static_cast<AIReal>(0.01)) return;

    std::vector<AIReal> cumulative(n + 1, 0);
    for (size_t i = 0; i < n; ++i) {
        const VHPoint& a = ring.points[i];
        const VHPoint& b = ring.points[(i + 1) % n];
        const AIReal dx = b.x - a.x;
        const AIReal dy = b.y - a.y;
        cumulative[i + 1] = cumulative[i] +
            static_cast<AIReal>(std::hypot(static_cast<double>(dx), static_cast<double>(dy)));
    }

    const AIReal perimeter = cumulative.back();
    if (perimeter <= static_cast<AIReal>(0.01)) return;

    const int sampleCount = (std::max)(1,
        static_cast<int>(std::ceil(static_cast<double>(perimeter / requestedSpacing))));
    const AIReal step = perimeter / static_cast<AIReal>(sampleCount);
    out.reserve(static_cast<size_t>(sampleCount));

    phase -= static_cast<AIReal>(std::floor(static_cast<double>(phase)));
    for (int i = 0; i < sampleCount; ++i) {
        AIReal d = (static_cast<AIReal>(i) + static_cast<AIReal>(0.5) + phase) * step;
        if (d >= perimeter) d -= perimeter;

        auto it = std::upper_bound(cumulative.begin(), cumulative.end(), d);
        size_t segIndex = 0;
        if (it != cumulative.begin()) segIndex = static_cast<size_t>((it - cumulative.begin()) - 1);
        if (segIndex >= n) segIndex = n - 1;

        const VHPoint& a = ring.points[segIndex];
        const VHPoint& b = ring.points[(segIndex + 1) % n];
        const AIReal segStart = cumulative[segIndex];
        const AIReal segLength = cumulative[segIndex + 1] - segStart;
        if (segLength <= static_cast<AIReal>(0.0001)) continue;

        const AIReal t = Clamp01((d - segStart) / segLength);
        const AIReal ex = a.x + (b.x - a.x) * t;
        const AIReal ey = a.y + (b.y - a.y) * t;
        const AIReal tx = (b.x - a.x) / segLength;
        const AIReal ty = (b.y - a.y) / segLength;
        AIReal nx = -ty;
        AIReal ny = tx;

        // Determine which side of this local tangent is the filled side. The
        // doubled fallback probe helps at tight corners without assuming winding.
        bool plusInside = VHPointInside(ex + nx * probe, ey + ny * probe, geometry);
        bool minusInside = VHPointInside(ex - nx * probe, ey - ny * probe, geometry);
        if (plusInside == minusInside) {
            const AIReal probe2 = probe * static_cast<AIReal>(2.5);
            plusInside = VHPointInside(ex + nx * probe2, ey + ny * probe2, geometry);
            minusInside = VHPointInside(ex - nx * probe2, ey - ny * probe2, geometry);
        }
        if (plusInside == minusInside) continue;
        if (!plusInside && minusInside) {
            nx = -nx;
            ny = -ny;
        }

        VHEdgeSample sample;
        sample.x = ex;
        sample.y = ey;
        sample.inX = nx;
        sample.inY = ny;
        sample.normalAngle = static_cast<AIReal>(
            std::atan2(static_cast<double>(ny), static_cast<double>(nx)) * 180.0 / M_PI);
        out.push_back(sample);
    }
}

AIErr UniteGroupChildren(AIArtHandle group) {
    if (!group || !sVectorHalftonePathfinder) return kNoErr;

    std::vector<AIArtHandle> children;
    AIArtHandle child = nullptr;
    AIErr error = sAIArt->GetArtFirstChild(group, &child);
    if (error != kNoErr) return error;
    while (child) {
        children.push_back(child);
        AIArtHandle next = nullptr;
        error = sAIArt->GetArtSibling(child, &next);
        if (error != kNoErr) return error;
        child = next;
    }

    if (children.size() < 2) return kNoErr;

    // Pathfinder becomes disproportionately expensive on very dense cores. The
    // tiles already overlap enough to render as one solid region, so only ask
    // Illustrator to physically union moderate-size cores.
    constexpr size_t kMaxAutomaticUniteChildren = 700;
    if (children.size() > kMaxAutomaticUniteChildren) return kNoErr;

    AIPathfinderData data{};
    data.options.ipmPrecision = kDefaultPrecision;
    data.options.removeRedundantPoints = 1;
    data.options.flags = kSuppressProgressDialog | kDeselectResultArts;
    data.fSelectedArt = children.data();
    data.fSelectedArtCount = static_cast<ai::int32>(children.size());
    data.fAlertInfoID = 0;

    return sVectorHalftonePathfinder->DoUniteEffect(&data, nullptr);
}

} // namespace

Plugin* AllocatePlugin(SPPluginRef pluginRef) {
    return new VectorHalftoneEffectPlugin(pluginRef);
}

void FixupReload(Plugin* plugin) {
    VectorHalftoneEffectPlugin::FixupVTable(static_cast<VectorHalftoneEffectPlugin*>(plugin));
}

VectorHalftoneEffectPlugin::VectorHalftoneEffectPlugin(SPPluginRef pluginRef)
    : Plugin(pluginRef), fLiveEffect(nullptr), fGradientEffect(nullptr), fSimpleColourHalftoneEffect(nullptr), fPhotoshopHalftoneEffect(nullptr) {
#ifdef WIN_ENV
    strncpy_s(fPluginName, kMaxStringLength, kVectorHalftonePluginName, _TRUNCATE);
#else
    std::strncpy(fPluginName, kVectorHalftonePluginName, kMaxStringLength);
#endif
}

VectorHalftoneEffectPlugin::~VectorHalftoneEffectPlugin() = default;

ASErr VectorHalftoneEffectPlugin::StartupPlugin(SPInterfaceMessage* message) {
    AIErr error = kNoErr;
    try {
        error = Plugin::StartupPlugin(message);
        CHKERR;
        error = AddLiveEffect(message);
        CHKERR;
        error = AddExtraLiveEffects(message);
        CHKERR;
    } catch (ai::Error& ex) {
        error = ex;
    } catch (...) {
        error = kCantHappenErr;
    }
    return error;
}

ASErr VectorHalftoneEffectPlugin::ShutdownPlugin(SPInterfaceMessage* message) {
    return Plugin::ShutdownPlugin(message);
}

ASErr VectorHalftoneEffectPlugin::AddLiveEffect(SPInterfaceMessage* message) {
    // Register the original Vector Halftone for document compatibility, but do
    // not add a menu item from v0.5 onward. Existing artwork can still edit and
    // render the effect; new users only see Simple Colour Halftone + Color Halftone.
    AILiveEffectData effectData{};
    effectData.self = message->d.self;
    effectData.name = kVectorHalftoneEffectName;
    char effectTitle[128]{};
    ai::UnicodeString(kVectorHalftoneEffectTitle, kAIUTF8CharacterEncoding).as_Platform(effectTitle, 128);
    effectData.title = effectTitle;
    effectData.majorVersion = 5;
    effectData.minorVersion = 0;
    effectData.prefersAsInput = kPathInputArt | kCompoundPathInputArt | kGroupInputArt;
    effectData.styleFilterFlags = kPostEffectFilter;
    return sAILiveEffect->AddLiveEffect(&effectData, &fLiveEffect);
}

ASErr VectorHalftoneEffectPlugin::AddExtraLiveEffects(SPInterfaceMessage* message) {
    AIErr error = kNoErr;

    auto registerEffect = [&](const char* name, const char* titleText,
                              ai::int32 inputTypes, AILiveEffectHandle* outHandle) -> AIErr {
        AILiveEffectData effectData{};
        effectData.self = message->d.self;
        effectData.name = name;
        char title[160]{};
        ai::UnicodeString(titleText, kAIUTF8CharacterEncoding).as_Platform(title, 160);
        effectData.title = title;
        effectData.majorVersion = 5;
        effectData.minorVersion = 0;
        effectData.prefersAsInput = inputTypes;
        effectData.styleFilterFlags = kPostEffectFilter;
        return sAILiveEffect->AddLiveEffect(&effectData, outHandle);
    };

    auto addMenu = [&](AILiveEffectHandle effect, const char* name, const char* menuText) -> AIErr {
        AddLiveEffectMenuData menuData{};
        char category[128]{};
        char menuTitle[160]{};
        ai::UnicodeString(kVectorHalftoneEffectCategory, kAIUTF8CharacterEncoding).as_Platform(category, 128);
        ai::UnicodeString(menuText, kAIUTF8CharacterEncoding).as_Platform(menuTitle, 160);
        menuData.category = category;
        menuData.title = menuTitle;
        menuData.options = 0;
        return sAILiveEffect->AddLiveEffectMenuItem(effect, name, &menuData, nullptr, nullptr);
    };

    // Hidden legacy gradient registration for existing v0.4 documents.
    error = registerEffect(kVectorHalftoneGradientEffectName, kVectorHalftoneGradientEffectTitle,
                           kPathInputArt | kCompoundPathInputArt | kGroupInputArt,
                           &fGradientEffect);
    if (error != kNoErr) return error;

    error = registerEffect(kVectorSimpleColourHalftoneEffectName, kVectorSimpleColourHalftoneEffectTitle,
                           kPathInputArt | kCompoundPathInputArt | kGroupInputArt,
                           &fSimpleColourHalftoneEffect);
    if (error != kNoErr) return error;
    error = addMenu(fSimpleColourHalftoneEffect, kVectorSimpleColourHalftoneEffectName,
                    kVectorSimpleColourHalftoneEffectMenuTitle);
    if (error != kNoErr) return error;

    error = registerEffect(kVectorPhotoshopHalftoneEffectName, kVectorPhotoshopHalftoneEffectTitle,
                           kAnyInputArtButPluginArt, &fPhotoshopHalftoneEffect);
    if (error != kNoErr) return error;
    return addMenu(fPhotoshopHalftoneEffect, kVectorPhotoshopHalftoneEffectName,
                   kVectorPhotoshopHalftoneEffectMenuTitle);
}

ASErr VectorHalftoneEffectPlugin::ReadParameters(const AILiveEffectParameters& dict, VectorHalftoneParams& p) const {
    // Start from v0.1-compatible defaults. New-instance defaults/last-used values
    // are substituted in EditLiveEffectParameters before the first preview/OK.
    p = VectorHalftoneParams{};
    p.glowWidth = ReadRealOr(dict, K_GLOW_WIDTH, static_cast<AIReal>(p.glowWidth));
    p.glowStrength = ReadRealOr(dict, K_GLOW_STRENGTH, static_cast<AIReal>(p.glowStrength));
    p.gamma = ReadRealOr(dict, K_GAMMA, static_cast<AIReal>(p.gamma));
    p.invert = static_cast<int>(ReadIntOr(dict, K_INVERT, p.invert));
    p.shape = static_cast<int>(ReadIntOr(dict, K_SHAPE, p.shape));
    p.spacing = ReadRealOr(dict, K_SPACING, static_cast<AIReal>(p.spacing));
    p.minSize = ReadRealOr(dict, K_MIN_SIZE, static_cast<AIReal>(p.minSize));
    p.maxSize = ReadRealOr(dict, K_MAX_SIZE, static_cast<AIReal>(p.maxSize));
    p.cullSize = ReadRealOr(dict, K_CULL_SIZE, static_cast<AIReal>(p.cullSize));
    p.gridAngle = ReadRealOr(dict, K_GRID_ANGLE, static_cast<AIReal>(p.gridAngle));
    p.stagger = static_cast<int>(ReadIntOr(dict, K_STAGGER, p.stagger));
    p.curveSamples = static_cast<int>(ReadIntOr(dict, K_CURVE_SAMPLES, p.curveSamples));
    p.containDots = static_cast<int>(ReadIntOr(dict, K_CONTAIN_DOTS, p.containDots));

    p.glowMode = static_cast<int>(ReadIntOr(dict, K_GLOW_MODE, 0));
    p.edgeSoftness = ReadRealOr(dict, K_EDGE_SOFTNESS, static_cast<AIReal>(0.65));
    // v0.3+ always follows the source fill colour. Keep the legacy dictionary
    // entry readable so older documents remain compatible, but ignore its value.
    p.colorMode = 1;
    p.clipToSource = static_cast<int>(ReadIntOr(dict, K_CLIP_SOURCE, 0));
    p.preserveSourceAppearance = static_cast<int>(ReadIntOr(dict, K_PRESERVE_SOURCE, 0));
    p.solidCenter = static_cast<int>(ReadIntOr(dict, K_SOLID_CENTER, 0));
    p.followCurve = static_cast<int>(ReadIntOr(dict, K_FOLLOW_CURVE, 0));
    p.preset = static_cast<int>(ReadIntOr(dict, K_PRESET, 0));
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::WriteParameters(const AILiveEffectParameters& dict, const VectorHalftoneParams& p) const {
    AIErr error = kNoErr;
#define SET_REAL(KEY, VALUE) do { error = WriteReal(dict, KEY, static_cast<AIReal>(VALUE)); if (error != kNoErr) return error; } while (0)
#define SET_INT(KEY, VALUE) do { error = WriteInt(dict, KEY, static_cast<ai::int32>(VALUE)); if (error != kNoErr) return error; } while (0)
    SET_REAL(K_GLOW_WIDTH, p.glowWidth);
    SET_REAL(K_GLOW_STRENGTH, p.glowStrength);
    SET_REAL(K_GAMMA, p.gamma);
    SET_INT(K_INVERT, p.invert);
    SET_INT(K_SHAPE, p.shape);
    SET_REAL(K_SPACING, p.spacing);
    SET_REAL(K_MIN_SIZE, p.minSize);
    SET_REAL(K_MAX_SIZE, p.maxSize);
    SET_REAL(K_CULL_SIZE, p.cullSize);
    SET_REAL(K_GRID_ANGLE, p.gridAngle);
    SET_INT(K_STAGGER, p.stagger);
    SET_INT(K_CURVE_SAMPLES, p.curveSamples);
    SET_INT(K_CONTAIN_DOTS, p.containDots);
    SET_INT(K_GLOW_MODE, p.glowMode);
    SET_REAL(K_EDGE_SOFTNESS, p.edgeSoftness);
    SET_INT(K_COLOR_MODE, 1);
    SET_INT(K_CLIP_SOURCE, p.clipToSource);
    SET_INT(K_PRESERVE_SOURCE, p.preserveSourceAppearance);
    SET_INT(K_SOLID_CENTER, p.solidCenter);
    SET_INT(K_FOLLOW_CURVE, p.followCurve);
    SET_INT(K_PRESET, p.preset);
#undef SET_REAL
#undef SET_INT
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::EditLiveEffectParameters(AILiveEffectEditParamMessage* message) {
    AIErr error = kNoErr;
    if (message && message->effect == fGradientEffect) return EditGradientParameters(message);
    if (message && message->effect == fSimpleColourHalftoneEffect) return EditSimpleColourParameters(message);
    if (message && message->effect == fPhotoshopHalftoneEffect) return EditPhotoshopParameters(message);
    try {
        VectorHalftoneParams params;
        error = ReadParameters(message->parameters, params);
        CHKERR;

        if (message->isNewInstance) {
#ifdef WIN_ENV
            VectorHalftoneParams remembered;
            if (LoadVectorHalftonePreferences(remembered)) params = remembered;
            else params = VectorHalftoneFactoryDefaults();
#else
            params = VectorHalftoneFactoryDefaults();
#endif
        }

        const VectorHalftoneParams savedParams = params;
        bool previewed = false;

#ifdef WIN_ENV
        AIWindowRef appWindow = nullptr;
        error = sAIAppContext->GetPlatformAppWindow(&appWindow);
        CHKERR;

        const VectorHalftonePreviewCallback previewCallback =
            [this, message, &previewed](const VectorHalftoneParams& previewParams) -> bool {
                AIErr e = WriteParameters(message->parameters, previewParams);
                if (e != kNoErr) return false;
                e = sAILiveEffect->UpdateParameters(message->context);
                if (e == kNoErr) previewed = true;
                return e == kNoErr;
            };

        const int dialogResult = ShowVectorHalftoneDialog(
            reinterpret_cast<HWND>(appWindow), params, previewCallback);

        if (dialogResult == kVectorHalftoneDialogOK) {
            error = WriteParameters(message->parameters, params);
            CHKERR;
            error = sAILiveEffect->UpdateParameters(message->context);
            CHKERR;
            SaveVectorHalftonePreferences(params);
        } else if (previewed) {
            if (message->isNewInstance) {
                // Matches Adobe's Live Effect sample pattern: the effect instance has
                // already been redrawn by preview, so cancel removes that transaction.
                error = sAIUndo->UndoChanges();
                CHKERR;
            } else {
                error = WriteParameters(message->parameters, savedParams);
                CHKERR;
                error = sAILiveEffect->UpdateParameters(message->context);
                CHKERR;
            }
        }
#else
        error = WriteParameters(message->parameters, params);
        CHKERR;
        error = sAILiveEffect->UpdateParameters(message->context);
        CHKERR;
#endif
    } catch (ai::Error& ex) {
        error = ex;
    } catch (...) {
        error = kCantHappenErr;
    }
    return error;
}

ASErr VectorHalftoneEffectPlugin::GoLiveEffect(AILiveEffectGoMessage* message) {
    AIErr error = kNoErr;
    if (message && message->effect == fGradientEffect) {
        try {
            VectorHalftoneGradientParams params;
            AIErr e = ReadGradientParameters(message->parameters, params);
            if (e != kNoErr) return e;
            AIArtHandle inputArt = message->art;
            AIArtHandle output = nullptr;
            e = BuildGradientHalftone(inputArt, output, params);
            if (e != kNoErr) return e;
            if (output) {
                // This is the hidden legacy Gradient Halftone effect. Its
                // parameter struct has no mode field; preserve/dispose the
                // input exactly as the original gradient effect did.
                if (!params.preserveSourceAppearance) {
                    e = sAIArt->DisposeArt(inputArt);
                    if (e != kNoErr) return e;
                }
                message->art = output;
            }
            return kNoErr;
        } catch (ai::Error& ex) { return ex; } catch (...) { return kCantHappenErr; }
    }
    if (message && message->effect == fSimpleColourHalftoneEffect) {
        try {
            VectorSimpleColourHalftoneParams params;
            AIErr e = ReadSimpleColourParameters(message->parameters, params);
            if (e != kNoErr) return e;
            AIArtHandle inputArt = message->art;
            AIArtHandle output = nullptr;
            e = BuildSimpleColourHalftone(inputArt, output, params);
            if (e != kNoErr) return e;
            if (output) {
                if (!params.preserveSourceAppearance) {
                    e = sAIArt->DisposeArt(inputArt);
                    if (e != kNoErr) return e;
                }
                message->art = output;
            }
            return kNoErr;
        } catch (ai::Error& ex) { return ex; } catch (...) { return kCantHappenErr; }
    }
    if (message && message->effect == fPhotoshopHalftoneEffect) {
        try {
            VectorPhotoshopHalftoneParams params;
            AIErr e = ReadPhotoshopParameters(message->parameters, params);
            if (e != kNoErr) return e;
            AIArtHandle inputArt = message->art;
            AIArtHandle output = nullptr;
            e = BuildPhotoshopHalftone(inputArt, output, params);
            if (e != kNoErr) return e;
            if (output) {
                if (!params.preserveSourceAppearance) {
                    e = sAIArt->DisposeArt(inputArt);
                    if (e != kNoErr) return e;
                }
                message->art = output;
            }
            return kNoErr;
        } catch (ai::Error& ex) { return ex; } catch (...) { return kCantHappenErr; }
    }
    try {
        VectorHalftoneParams params;
        error = ReadParameters(message->parameters, params);
        CHKERR;

        AIArtHandle inputArt = message->art;
        AIArtHandle output = nullptr;
        error = BuildHalftone(inputArt, output, params);
        CHKERR;

        if (output) {
            // If preserveSourceAppearance is enabled BuildHalftone has moved the
            // effect input art into the output group. Otherwise replace it entirely.
            if (!params.preserveSourceAppearance) {
                error = sAIArt->DisposeArt(inputArt);
                CHKERR;
            }
            message->art = output;
        }
    } catch (ai::Error& ex) {
        error = ex;
    } catch (...) {
        error = kCantHappenErr;
    }
    return error;
}

ASErr VectorHalftoneEffectPlugin::BuildHalftone(AIArtHandle inputArt, AIArtHandle& outputArt,
                                                 const VectorHalftoneParams& p) const {
    outputArt = nullptr;
    if (!inputArt || p.spacing <= 0.0 || p.maxSize <= 0.0 || p.glowWidth <= 0.0)
        return kBadParameterErr;

    std::vector<AIArtHandle> targets;
    AIErr error = CollectHalftoneTargets(inputArt, targets);
    if (error != kNoErr) return error;
    if (targets.empty()) targets.push_back(inputArt);

    AIArtHandle outputGroup = nullptr;
    error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
    if (error != kNoErr) return error;

    const AIReal spacing = static_cast<AIReal>(p.spacing);

    for (AIArtHandle sourceArt : targets) {
        VHGeometry geometry;
        error = BuildVHGeometry(sourceArt, p.curveSamples, geometry);
        if (error != kNoErr) continue;

        AIPathStyle markStyle;
        VHGetSourceMarkStyle(sourceArt, markStyle);

        AIArtHandle targetGroup = nullptr;
        error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &targetGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }

        AIArtHandle markParent = targetGroup;
        AIArtHandle marksGroup = nullptr;
        const bool useExactClip = (p.clipToSource || p.solidCenter) && p.glowMode == 0;
        if (useExactClip) {
            AIArtHandle clipGroup = nullptr;
            error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, targetGroup, &clipGroup);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            error = sAIGroup->SetGroupClipped(clipGroup, true);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }

            AIArtHandle maskArt = nullptr;
            error = sAIArt->DuplicateArt(sourceArt, kPlaceInsideOnTop, clipGroup, &maskArt);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            error = MakeClipMaskNonPainting(maskArt);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            error = sAIArt->SetArtUserAttr(maskArt, kArtIsClipMask, kArtIsClipMask);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }

            error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &marksGroup);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            markParent = marksGroup;
        } else {
            error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, targetGroup, &marksGroup);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            markParent = marksGroup;
        }

        AIArtHandle coreGroup = nullptr;
        AIArtHandle transitionGroup = markParent;
        if (p.solidCenter && p.glowMode == 0) {
            error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, markParent, &coreGroup);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, markParent, &transitionGroup);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        }

        AIReal left = geometry.bounds.left;
        AIReal top = geometry.bounds.top;
        AIReal right = geometry.bounds.right;
        AIReal bottom = geometry.bounds.bottom;

        const AIReal expansion = (p.glowMode == 1)
            ? static_cast<AIReal>(p.glowWidth + p.maxSize + p.spacing)
            : static_cast<AIReal>(p.maxSize + p.spacing);
        if (p.glowMode == 1) {
            left -= expansion;
            right += expansion;
            top += expansion;
            bottom -= expansion;
        }
        if (right <= left || top <= bottom) continue;

        const AIReal cx = (geometry.bounds.left + geometry.bounds.right) / 2;
        const AIReal cy = (geometry.bounds.top + geometry.bounds.bottom) / 2;
        const AIReal radians = static_cast<AIReal>(p.gridAngle * M_PI / 180.0);
        const AIReal cosA = static_cast<AIReal>(std::cos(static_cast<double>(radians)));
        const AIReal sinA = static_cast<AIReal>(std::sin(static_cast<double>(radians)));

        // Solid Centre means "keep the interior solid after the requested glow
        // transition", not "paint the whole source path". Core coverage cells are
        // only emitted far enough inside that their corners cannot bleed back into
        // the glow band. Transition marks continue until they physically meet the
        // safe core coverage, so there is no gap between the two regions.
        const AIReal coreStart = static_cast<AIReal>(p.glowWidth);
        const AIReal coreTileSize = spacing * static_cast<AIReal>(1.42);
        const AIReal coreTileHalfDiagonal = coreTileSize * static_cast<AIReal>(0.7071067811865476);
        const AIReal coreSafeCentre = coreStart + coreTileHalfDiagonal + spacing * static_cast<AIReal>(0.08);
        const AIReal contourDepth = static_cast<AIReal>(p.glowWidth);

        auto emitTransition = [&](AIReal px, AIReal py, AIReal markAngle, AIReal distance) -> AIErr {
            if (p.solidCenter && p.glowMode == 0 && distance >= coreSafeCentre)
                return kNoErr;

            AIReal toneT = 0;
            if (p.glowMode == 0)
                toneT = Clamp01(distance / static_cast<AIReal>(p.glowWidth));
            else
                toneT = static_cast<AIReal>(1) - Clamp01(distance / static_cast<AIReal>(p.glowWidth));
            toneT = SoftRamp(toneT, static_cast<AIReal>(p.edgeSoftness));

            AIReal density = static_cast<AIReal>((1.0 - p.glowStrength) + p.glowStrength * toneT);
            density = static_cast<AIReal>(std::pow(static_cast<double>(Clamp01(density)), p.gamma));
            if (p.invert) density = static_cast<AIReal>(1) - density;

            AIReal markSize = static_cast<AIReal>(p.minSize + (p.maxSize - p.minSize) * density);
            if (p.cullSize > 0.0 && markSize < static_cast<AIReal>(p.cullSize)) return kNoErr;

            if (p.containDots && p.glowMode == 0 && !useExactClip) {
                const AIReal maxContained = (p.shape == 1)
                    ? distance * static_cast<AIReal>(std::sqrt(2.0))
                    : distance * 2;
                markSize = (std::min)(markSize, maxContained);
            }
            if (markSize <= static_cast<AIReal>(0.05)) return kNoErr;

            return VHCreateMark(transitionGroup, p.shape, px, py, markSize, spacing,
                                markAngle, markStyle, true);
        };

        auto emitRegularGrid = [&](bool deepInteriorOnly) -> AIErr {
            LocalBounds local = RotatedLocalBounds(left, top, right, bottom, cx, cy, cosA, sinA);
            const int rowStart = static_cast<int>(std::floor(static_cast<double>(local.minY / spacing))) - 1;
            const int rowEnd = static_cast<int>(std::ceil(static_cast<double>(local.maxY / spacing))) + 1;
            const int colStart = static_cast<int>(std::floor(static_cast<double>(local.minX / spacing))) - 1;
            const int colEnd = static_cast<int>(std::ceil(static_cast<double>(local.maxX / spacing))) + 1;
            const AIReal deepThreshold = contourDepth - spacing * static_cast<AIReal>(0.30);

            for (int row = rowStart; row <= rowEnd; ++row) {
                const AIReal localY = static_cast<AIReal>(row) * spacing;
                const AIReal rowOffset = (p.stagger && (row & 1)) ? spacing / 2 : 0;
                for (int col = colStart; col <= colEnd; ++col) {
                    const AIReal localX = static_cast<AIReal>(col) * spacing + rowOffset;
                    const AIReal px = cx + localX * cosA - localY * sinA;
                    const AIReal py = cy + localX * sinA + localY * cosA;
                    if (px < left || px > right || py > top || py < bottom) continue;

                    const bool inside = VHPointInside(px, py, geometry);
                    if (p.glowMode == 0) {
                        if (!inside) continue;
                    } else if (inside) {
                        continue;
                    }

                    const AIReal distance = VHDistanceToEdge(px, py, geometry);
                    if (p.glowMode == 1 && distance > static_cast<AIReal>(p.glowWidth)) continue;
                    if (deepInteriorOnly && distance < deepThreshold) continue;

                    const AIErr e = emitTransition(px, py, static_cast<AIReal>(p.gridAngle), distance);
                    if (e != kNoErr) return e;
                }
            }
            return kNoErr;
        };

        if (!p.followCurve) {
            // Fast path: no nearest-edge-normal calculation at all. v0.3.5 did
            // that expensive scan for every grid point even though the result was
            // discarded when contour following was disabled.
            error = emitRegularGrid(false);
            if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        } else {
            // Follow only the edge-transition band. Deep offset contours fold over
            // themselves on concave/thin artwork and were responsible for the
            // dense black fans in v0.3.5. Beyond the glow band, ordinary grid
            // marks take over (unless Solid Centre replaces them entirely).
            const int depthRows = (std::max)(1,
                static_cast<int>(std::ceil(static_cast<double>(contourDepth / spacing))) + 1);
            const AIReal probe = (std::max)(static_cast<AIReal>(0.18),
                (std::min)(static_cast<AIReal>(1.25), spacing * static_cast<AIReal>(0.18)));
            const AIReal quant = (std::max)(static_cast<AIReal>(0.20), spacing * static_cast<AIReal>(0.70));
            const AIReal distanceTolerance = spacing * static_cast<AIReal>(0.48);
            std::unordered_set<VHQuantKey, VHQuantKeyHash> emitted;

            std::vector<VHEdgeSample> samplesA;
            std::vector<VHEdgeSample> samplesB;
            for (const VHRing& ring : geometry.rings) {
                BuildRingEdgeSamples(ring, geometry, spacing, static_cast<AIReal>(0.0), probe, samplesA);
                BuildRingEdgeSamples(ring, geometry, spacing, static_cast<AIReal>(0.5), probe, samplesB);
                if (samplesA.empty() && samplesB.empty()) continue;

                for (int depthRow = 0; depthRow < depthRows; ++depthRow) {
                    const AIReal depth = (static_cast<AIReal>(depthRow) + static_cast<AIReal>(0.50)) * spacing;
                    if (depth > contourDepth + spacing * static_cast<AIReal>(0.30)) break;
                    const std::vector<VHEdgeSample>& samples = (depthRow & 1) ? samplesB : samplesA;
                    const AIReal dir = p.glowMode == 0 ? static_cast<AIReal>(1) : static_cast<AIReal>(-1);

                    for (const VHEdgeSample& sample : samples) {
                        const AIReal px = sample.x + sample.inX * depth * dir;
                        const AIReal py = sample.y + sample.inY * depth * dir;
                        if (px < left || px > right || py > top || py < bottom) continue;

                        const bool inside = VHPointInside(px, py, geometry);
                        if ((p.glowMode == 0 && !inside) || (p.glowMode == 1 && inside)) continue;

                        const AIReal actualDistance = VHDistanceToEdge(px, py, geometry);
                        if (p.glowMode == 1 && actualDistance > static_cast<AIReal>(p.glowWidth)) continue;

                        // Reject normals that have crossed another nearby edge. This
                        // is the key guard against contour rows piling up in narrow,
                        // concave and self-approaching parts of the artwork.
                        if (std::fabs(static_cast<double>(actualDistance - depth)) >
                            static_cast<double>(distanceTolerance))
                            continue;

                        const VHQuantKey key{
                            static_cast<std::int64_t>(std::llround(static_cast<double>(px / quant))),
                            static_cast<std::int64_t>(std::llround(static_cast<double>(py / quant)))
                        };
                        if (!emitted.insert(key).second) continue;

                        error = emitTransition(px, py, sample.normalAngle, actualDistance);
                        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
                    }
                }
            }

            if (p.glowMode == 0) {
                // Once contour rows reach the end of the glow band, switch back to
                // the stable regular grid. With Solid Centre this also bridges the
                // short safety band between glowWidth and the first fully-contained
                // core coverage cells, preventing either a gap or a solid-fill bleed.
                error = emitRegularGrid(true);
                if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
            }
        }

        if (coreGroup) {
            // Build the centre from overlapping square coverage cells on a stable
            // regular lattice. A cell centre must be at least one half-diagonal
            // beyond glowWidth, so no part of a core cell can intrude into the
            // visible halftone transition. This prevents Solid Centre from looking
            // like a full flat fill while still eliminating pinholes in the core.
            const AIReal tileSize = coreTileSize;
            const AIReal centreThreshold = coreSafeCentre;
            const int xStart = static_cast<int>(std::floor(static_cast<double>(geometry.bounds.left / spacing))) - 2;
            const int xEnd = static_cast<int>(std::ceil(static_cast<double>(geometry.bounds.right / spacing))) + 2;
            const int yStart = static_cast<int>(std::floor(static_cast<double>(geometry.bounds.bottom / spacing))) - 2;
            const int yEnd = static_cast<int>(std::ceil(static_cast<double>(geometry.bounds.top / spacing))) + 2;

            for (int gy = yStart; gy <= yEnd; ++gy) {
                const AIReal py = static_cast<AIReal>(gy) * spacing;
                for (int gx = xStart; gx <= xEnd; ++gx) {
                    const AIReal px = static_cast<AIReal>(gx) * spacing;
                    if (!VHPointInside(px, py, geometry)) continue;
                    const AIReal distance = VHDistanceToEdge(px, py, geometry);
                    if (distance < centreThreshold) continue;
                    error = VHCreateMark(coreGroup, 1, px, py, tileSize, spacing,
                                         static_cast<AIReal>(0), markStyle, true);
                    if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
                }
            }

            // This changes topology only; the visual solidity no longer depends on
            // Pathfinder succeeding. Keeping it as a best-effort pass means small
            // and medium cores expand to a cleaner single shape without making
            // giant artwork painfully slow.
            (void)UniteGroupChildren(coreGroup);
        }
    }

    if (p.preserveSourceAppearance) {
        error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
    }

    outputArt = outputGroup;
    return kNoErr;
}



ASErr VectorHalftoneEffectPlugin::ReadGradientParameters(const AILiveEffectParameters& dict,
                                                          VectorHalftoneGradientParams& p) const {
    p = VectorHalftoneGradientDefaults();
    p.gradientType = static_cast<int>(ReadIntOr(dict, KG_TYPE, p.gradientType));
    p.gradientAngle = ReadRealOr(dict, KG_ANGLE, static_cast<AIReal>(p.gradientAngle));
    p.spacing = ReadRealOr(dict, KG_SPACING, static_cast<AIReal>(p.spacing));
    p.minSize = ReadRealOr(dict, KG_MIN_SIZE, static_cast<AIReal>(p.minSize));
    p.maxSize = ReadRealOr(dict, KG_MAX_SIZE, static_cast<AIReal>(p.maxSize));
    p.cullSize = ReadRealOr(dict, KG_CULL_SIZE, static_cast<AIReal>(p.cullSize));
    p.gridAngle = ReadRealOr(dict, KG_GRID_ANGLE, static_cast<AIReal>(p.gridAngle));
    p.shape = static_cast<int>(ReadIntOr(dict, KG_SHAPE, p.shape));
    p.reverse = static_cast<int>(ReadIntOr(dict, KG_REVERSE, p.reverse));
    p.stagger = static_cast<int>(ReadIntOr(dict, KG_STAGGER, p.stagger));
    p.clipToSource = static_cast<int>(ReadIntOr(dict, KG_CLIP, p.clipToSource));
    p.preserveSourceAppearance = static_cast<int>(ReadIntOr(dict, KG_PRESERVE, p.preserveSourceAppearance));
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::WriteGradientParameters(const AILiveEffectParameters& dict,
                                                           const VectorHalftoneGradientParams& p) const {
    AIErr error = kNoErr;
#define GREAL(K,V) do { error=WriteReal(dict,K,static_cast<AIReal>(V)); if(error!=kNoErr)return error; } while(0)
#define GINT(K,V) do { error=WriteInt(dict,K,static_cast<ai::int32>(V)); if(error!=kNoErr)return error; } while(0)
    GINT(KG_TYPE,p.gradientType); GREAL(KG_ANGLE,p.gradientAngle); GREAL(KG_SPACING,p.spacing);
    GREAL(KG_MIN_SIZE,p.minSize); GREAL(KG_MAX_SIZE,p.maxSize); GREAL(KG_CULL_SIZE,p.cullSize);
    GREAL(KG_GRID_ANGLE,p.gridAngle); GINT(KG_SHAPE,p.shape); GINT(KG_REVERSE,p.reverse);
    GINT(KG_STAGGER,p.stagger); GINT(KG_CLIP,p.clipToSource); GINT(KG_PRESERVE,p.preserveSourceAppearance);
#undef GREAL
#undef GINT
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::ReadSimpleColourParameters(const AILiveEffectParameters& dict,
                                                                VectorSimpleColourHalftoneParams& p) const {
    p = VectorSimpleColourHalftoneDefaults();

    // v0.5 had no mode key; v0.6 used mode=1. Keep both renderers available so
    // existing documents retain their appearance. New instances default to 2.
    const AIDictKey modeKey = sAIDictionary->Key(KS_MODE);
    const bool hasMode = sAIDictionary->IsKnown(dict, modeKey);
    p.mode = hasMode ? static_cast<int>(ReadIntOr(dict, KS_MODE, p.mode)) : 0;

    p.sourceMode = static_cast<int>(ReadIntOr(dict, KS_SOURCE_MODE, p.sourceMode));
    p.gradientType = static_cast<int>(ReadIntOr(dict, KS_GRADIENT_TYPE, p.gradientType));
    p.gradientAngle = ReadRealOr(dict, KS_GRADIENT_ANGLE, static_cast<AIReal>(p.gradientAngle));
    p.gradientOffset = ReadRealOr(dict, KS_GRADIENT_OFFSET, static_cast<AIReal>(p.gradientOffset));
    p.gradientScale = ReadRealOr(dict, KS_GRADIENT_SCALE, static_cast<AIReal>(p.gradientScale));
    p.reverse = static_cast<int>(ReadIntOr(dict, KS_REVERSE, p.reverse));

    p.shape = static_cast<int>(ReadIntOr(dict, KS_SHAPE, p.shape));
    p.spacing = ReadRealOr(dict, KS_SPACING, static_cast<AIReal>(p.spacing));
    p.gridAngle = ReadRealOr(dict, KS_ANGLE, static_cast<AIReal>(p.gridAngle));
    p.minSize = ReadRealOr(dict, KS_MIN_SIZE, static_cast<AIReal>(p.minSize));
    p.maxSize = ReadRealOr(dict, KS_MAX_SIZE, static_cast<AIReal>(p.maxSize));
    p.cullSize = ReadRealOr(dict, KS_CULL_SIZE, static_cast<AIReal>(p.cullSize));
    p.stagger = static_cast<int>(ReadIntOr(dict, KS_STAGGER, p.stagger));
    p.connectStroke = static_cast<int>(ReadIntOr(dict, KS_CONNECT_STROKE, p.connectStroke));
    p.strokeWidth = ReadRealOr(dict, KS_STROKE_WIDTH, static_cast<AIReal>(p.strokeWidth));
    p.clipToSource = static_cast<int>(ReadIntOr(dict, KS_CLIP, p.clipToSource));
    p.preserveSourceAppearance = static_cast<int>(ReadIntOr(dict, KS_PRESERVE, p.preserveSourceAppearance));

    // v0.6 compatibility fields.
    p.maxRadius = ReadRealOr(dict, KS_MAX_RADIUS, static_cast<AIReal>(p.maxRadius));
    p.screenAngle = ReadRealOr(dict, KS_SCREEN_ANGLE, static_cast<AIReal>(p.screenAngle));
    p.shadeStrength = ReadRealOr(dict, KS_SHADE_STRENGTH, static_cast<AIReal>(p.shadeStrength));

    // v0.5 compatibility fields.
    p.markSize = ReadRealOr(dict, KS_SIZE, static_cast<AIReal>(p.markSize));
    p.opacity = ReadRealOr(dict, KS_OPACITY, static_cast<AIReal>(p.opacity));

    if (!hasMode) {
        // Seed the v0.7 controls from the old flat pattern if the user later
        // chooses to edit/upgrade it.
        p.minSize = p.markSize;
        p.maxSize = p.markSize;
        p.cullSize = 0.0;
        p.connectStroke = 0;
    } else if (p.mode == 1) {
        // Seed the new direct-vector controls from v0.6's screen settings.
        p.spacing = (std::max)(1.0, p.maxRadius * std::sqrt(2.0));
        p.gridAngle = p.screenAngle;
        p.maxSize = (std::max)(1.0, p.maxRadius * 2.0 * 0.70);
        p.minSize = 0.0;
    }
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::WriteSimpleColourParameters(const AILiveEffectParameters& dict,
                                                                 const VectorSimpleColourHalftoneParams& p) const {
    AIErr error = kNoErr;
#define SREAL(K,V) do { error=WriteReal(dict,K,static_cast<AIReal>(V)); if(error!=kNoErr)return error; } while(0)
#define SINT(K,V) do { error=WriteInt(dict,K,static_cast<ai::int32>(V)); if(error!=kNoErr)return error; } while(0)
    SINT(KS_MODE,p.mode); SINT(KS_SOURCE_MODE,p.sourceMode);
    SINT(KS_GRADIENT_TYPE,p.gradientType); SREAL(KS_GRADIENT_ANGLE,p.gradientAngle);
    SREAL(KS_GRADIENT_OFFSET,p.gradientOffset); SREAL(KS_GRADIENT_SCALE,p.gradientScale);
    SINT(KS_REVERSE,p.reverse);

    SINT(KS_SHAPE,p.shape); SREAL(KS_SPACING,p.spacing); SREAL(KS_ANGLE,p.gridAngle);
    SREAL(KS_MIN_SIZE,p.minSize); SREAL(KS_MAX_SIZE,p.maxSize); SREAL(KS_CULL_SIZE,p.cullSize);
    SINT(KS_STAGGER,p.stagger); SINT(KS_CONNECT_STROKE,p.connectStroke);
    SREAL(KS_STROKE_WIDTH,p.strokeWidth); SINT(KS_CLIP,p.clipToSource);
    SINT(KS_PRESERVE,p.preserveSourceAppearance);

    // Keep legacy values serialised so older development builds do not silently
    // discard them when opening files created by this suite.
    SREAL(KS_MAX_RADIUS,p.maxRadius); SREAL(KS_SCREEN_ANGLE,p.screenAngle);
    SREAL(KS_SHADE_STRENGTH,p.shadeStrength); SREAL(KS_SIZE,p.markSize);
    SREAL(KS_OPACITY,p.opacity);
#undef SREAL
#undef SINT
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::ReadPhotoshopParameters(const AILiveEffectParameters& dict,
                                                           VectorPhotoshopHalftoneParams& p) const {
    p = VectorPhotoshopHalftoneDefaults();
    p.maxRadius = ReadRealOr(dict, KP_RADIUS, static_cast<AIReal>(p.maxRadius));
    p.angle1 = ReadRealOr(dict, KP_A1, static_cast<AIReal>(p.angle1));
    p.angle2 = ReadRealOr(dict, KP_A2, static_cast<AIReal>(p.angle2));
    p.angle3 = ReadRealOr(dict, KP_A3, static_cast<AIReal>(p.angle3));
    p.angle4 = ReadRealOr(dict, KP_A4, static_cast<AIReal>(p.angle4));
    p.cullSize = ReadRealOr(dict, KP_CULL, static_cast<AIReal>(p.cullSize));
    p.sampleDpi = ReadRealOr(dict, KP_DPI, static_cast<AIReal>(p.sampleDpi));
    p.preserveSourceAppearance = static_cast<int>(ReadIntOr(dict, KP_PRESERVE, p.preserveSourceAppearance));
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::WritePhotoshopParameters(const AILiveEffectParameters& dict,
                                                            const VectorPhotoshopHalftoneParams& p) const {
    AIErr error = kNoErr;
#define PREAL(K,V) do { error=WriteReal(dict,K,static_cast<AIReal>(V)); if(error!=kNoErr)return error; } while(0)
#define PINT(K,V) do { error=WriteInt(dict,K,static_cast<ai::int32>(V)); if(error!=kNoErr)return error; } while(0)
    PREAL(KP_RADIUS,p.maxRadius); PREAL(KP_A1,p.angle1); PREAL(KP_A2,p.angle2);
    PREAL(KP_A3,p.angle3); PREAL(KP_A4,p.angle4); PREAL(KP_CULL,p.cullSize);
    PREAL(KP_DPI,p.sampleDpi); PINT(KP_PRESERVE,p.preserveSourceAppearance);
#undef PREAL
#undef PINT
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::EditGradientParameters(AILiveEffectEditParamMessage* message) {
    AIErr error = kNoErr;
    VectorHalftoneGradientParams p;
    error = ReadGradientParameters(message->parameters, p); if (error != kNoErr) return error;
    if (message->isNewInstance) p = VectorHalftoneGradientDefaults();
    const auto saved = p; bool previewed = false;
#ifdef WIN_ENV
    AIWindowRef appWindow = nullptr; error = sAIAppContext->GetPlatformAppWindow(&appWindow); if(error!=kNoErr)return error;
    GradientPreviewCallback cb = [this,message,&previewed](const VectorHalftoneGradientParams& v){
        AIErr e=WriteGradientParameters(message->parameters,v); if(e!=kNoErr)return false;
        e=sAILiveEffect->UpdateParameters(message->context); if(e==kNoErr)previewed=true; return e==kNoErr; };
    const int r=ShowVectorHalftoneGradientDialog(reinterpret_cast<HWND>(appWindow),p,cb);
    if(r==2){ error=WriteGradientParameters(message->parameters,p); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context); }
    if(previewed){ if(message->isNewInstance)return sAIUndo->UndoChanges(); error=WriteGradientParameters(message->parameters,saved); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context); }
#else
    error=WriteGradientParameters(message->parameters,p); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context);
#endif
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::EditSimpleColourParameters(AILiveEffectEditParamMessage* message) {
    AIErr error = kNoErr;
    VectorSimpleColourHalftoneParams p;
    error = ReadSimpleColourParameters(message->parameters, p); if (error != kNoErr) return error;
    const auto originallySaved = p;
    if (message->isNewInstance) p = VectorSimpleColourHalftoneDefaults();
    else if (p.mode != 2) p.mode = 2; // editing a legacy instance opts into the direct-vector workflow
    const auto saved = message->isNewInstance ? p : originallySaved;
    bool previewed = false;
#ifdef WIN_ENV
    AIWindowRef appWindow = nullptr;
    error=sAIAppContext->GetPlatformAppWindow(&appWindow); if(error!=kNoErr)return error;
    SimpleColourHalftonePreviewCallback cb=[this,message,&previewed](const VectorSimpleColourHalftoneParams& v){
        AIErr e=WriteSimpleColourParameters(message->parameters,v); if(e!=kNoErr)return false;
        e=sAILiveEffect->UpdateParameters(message->context); if(e==kNoErr)previewed=true; return e==kNoErr; };
    const int r=ShowVectorSimpleColourHalftoneDialog(reinterpret_cast<HWND>(appWindow),p,cb);
    if(r==2){error=WriteSimpleColourParameters(message->parameters,p);if(error!=kNoErr)return error;return sAILiveEffect->UpdateParameters(message->context);}
    if(previewed){if(message->isNewInstance)return sAIUndo->UndoChanges();error=WriteSimpleColourParameters(message->parameters,saved);if(error!=kNoErr)return error;return sAILiveEffect->UpdateParameters(message->context);}
#else
    error=WriteSimpleColourParameters(message->parameters,p); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context);
#endif
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::EditPhotoshopParameters(AILiveEffectEditParamMessage* message) {
    AIErr error = kNoErr;
    VectorPhotoshopHalftoneParams p;
    error = ReadPhotoshopParameters(message->parameters, p); if (error != kNoErr) return error;
    if (message->isNewInstance) p = VectorPhotoshopHalftoneDefaults();
    const auto saved = p; bool previewed = false;
#ifdef WIN_ENV
    AIWindowRef appWindow = nullptr; error=sAIAppContext->GetPlatformAppWindow(&appWindow); if(error!=kNoErr)return error;
    PhotoshopHalftonePreviewCallback cb=[this,message,&previewed](const VectorPhotoshopHalftoneParams& v){
        AIErr e=WritePhotoshopParameters(message->parameters,v); if(e!=kNoErr)return false;
        e=sAILiveEffect->UpdateParameters(message->context); if(e==kNoErr)previewed=true; return e==kNoErr; };
    const int r=ShowVectorPhotoshopHalftoneDialog(reinterpret_cast<HWND>(appWindow),p,cb);
    if(r==2){ error=WritePhotoshopParameters(message->parameters,p); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context); }
    if(previewed){ if(message->isNewInstance)return sAIUndo->UndoChanges(); error=WritePhotoshopParameters(message->parameters,saved); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context); }
#else
    error=WritePhotoshopParameters(message->parameters,p); if(error!=kNoErr)return error; return sAILiveEffect->UpdateParameters(message->context);
#endif
    return kNoErr;
}

ASErr VectorHalftoneEffectPlugin::BuildGradientHalftone(AIArtHandle inputArt, AIArtHandle& outputArt,
                                                         const VectorHalftoneGradientParams& p) const {
    outputArt=nullptr;
    if(!inputArt||p.spacing<=0||p.maxSize<=0||p.maxSize<p.minSize)return kBadParameterErr;
    std::vector<AIArtHandle> targets; AIErr error=CollectHalftoneTargets(inputArt,targets);
    if(error!=kNoErr)return error; if(targets.empty())return kBadParameterErr;
    AIArtHandle outputGroup=nullptr; error=sAIArt->NewArt(kGroupArt,kPlaceBelow,inputArt,&outputGroup); if(error!=kNoErr)return error;
    const AIReal spacing=static_cast<AIReal>(p.spacing);
    for(AIArtHandle sourceArt:targets){
        VHGeometry g; error=BuildVHGeometry(sourceArt,8,g); if(error!=kNoErr)continue;
        AIPathStyle style; VHGetSourceMarkStyle(sourceArt,style);
        AIArtHandle parent=outputGroup;
        if(p.clipToSource){
            AIArtHandle clip=nullptr; error=sAIArt->NewArt(kGroupArt,kPlaceInsideOnTop,outputGroup,&clip); if(error!=kNoErr)goto gradient_fail;
            error=sAIGroup->SetGroupClipped(clip,true); if(error!=kNoErr)goto gradient_fail;
            AIArtHandle mask=nullptr; error=sAIArt->DuplicateArt(sourceArt,kPlaceInsideOnTop,clip,&mask); if(error!=kNoErr)goto gradient_fail;
            error=MakeClipMaskNonPainting(mask); if(error!=kNoErr)goto gradient_fail;
            error=sAIArt->SetArtUserAttr(mask,kArtIsClipMask,kArtIsClipMask); if(error!=kNoErr)goto gradient_fail;
            error=sAIArt->NewArt(kGroupArt,kPlaceInsideOnBottom,clip,&parent); if(error!=kNoErr)goto gradient_fail;
        } else { error=sAIArt->NewArt(kGroupArt,kPlaceInsideOnTop,outputGroup,&parent); if(error!=kNoErr)goto gradient_fail; }
        {
            const AIReal left=g.bounds.left,right=g.bounds.right,top=g.bounds.top,bottom=g.bounds.bottom;
            const AIReal cx=(left+right)/2,cy=(top+bottom)/2;
            const AIReal ga=static_cast<AIReal>(p.gridAngle*M_PI/180.0),gc=static_cast<AIReal>(std::cos((double)ga)),gs=static_cast<AIReal>(std::sin((double)ga));
            LocalBounds lb=RotatedLocalBounds(left,top,right,bottom,cx,cy,gc,gs);
            const int rs=(int)std::floor((double)(lb.minY/spacing))-1,re=(int)std::ceil((double)(lb.maxY/spacing))+1;
            const int cs=(int)std::floor((double)(lb.minX/spacing))-1,ce=(int)std::ceil((double)(lb.maxX/spacing))+1;
            const AIReal gradA=static_cast<AIReal>(p.gradientAngle*M_PI/180.0),gx=static_cast<AIReal>(std::cos((double)gradA)),gy=static_cast<AIReal>(std::sin((double)gradA));
            AIReal pmin=1e30f,pmax=-1e30f,maxR=0;
            const AIReal xx[4]={left,right,right,left}, yy[4]={top,top,bottom,bottom};
            for(int i=0;i<4;++i){AIReal dx=xx[i]-cx,dy=yy[i]-cy;AIReal q=dx*gx+dy*gy;pmin=(std::min)(pmin,q);pmax=(std::max)(pmax,q);maxR=(std::max)(maxR,static_cast<AIReal>(std::hypot((double)dx,(double)dy)));}
            for(int row=rs;row<=re;++row){AIReal ly=(AIReal)row*spacing;AIReal off=(p.stagger&&(row&1))?spacing/2:0;for(int col=cs;col<=ce;++col){
                AIReal lx=(AIReal)col*spacing+off,px=cx+lx*gc-ly*gs,py=cy+lx*gs+ly*gc;
                if(px<left||px>right||py>top||py<bottom||!VHPointInside(px,py,g))continue;
                AIReal t=0;if(p.gradientType==1){t=maxR>0?static_cast<AIReal>(std::hypot((double)(px-cx),(double)(py-cy)))/maxR:0;}else{AIReal q=(px-cx)*gx+(py-cy)*gy;t=(pmax>pmin)?(q-pmin)/(pmax-pmin):0;}t=Clamp01(t);if(p.reverse)t=1-t;
                AIReal size=static_cast<AIReal>(p.minSize+(p.maxSize-p.minSize)*t);if(size<(AIReal)p.cullSize||size<=0.05)continue;
                error=VHCreateMark(parent,p.shape,px,py,size,spacing,(AIReal)p.gridAngle,style,true);if(error!=kNoErr)goto gradient_fail;
            }}
        }
    }
    if(p.preserveSourceAppearance){error=sAIArt->ReorderArt(inputArt,kPlaceInsideOnBottom,outputGroup);if(error!=kNoErr)goto gradient_fail;}
    outputArt=outputGroup; return kNoErr;
gradient_fail:
    sAIArt->DisposeArt(outputGroup); return error;
}

ASErr VectorHalftoneEffectPlugin::BuildSimpleColourHalftone(AIArtHandle inputArt, AIArtHandle& outputArt,
                                                               const VectorSimpleColourHalftoneParams& p) const {
    outputArt = nullptr;
    if (!inputArt) return kBadParameterErr;

    // Preserve v0.5 flat-pattern artwork exactly. New instances use mode=2.
    if (p.mode == 0) {
        if (p.spacing <= 0.0 || p.markSize <= 0.0 || p.opacity < 0.0 || p.opacity > 100.0)
            return kBadParameterErr;

        std::vector<AIArtHandle> targets;
        AIErr error = CollectHalftoneTargets(inputArt, targets);
        if (error != kNoErr) return error;
        if (targets.empty()) return kBadParameterErr;

        AIArtHandle outputGroup = nullptr;
        error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
        if (error != kNoErr) return error;

        const AIReal spacing = static_cast<AIReal>(p.spacing);
        const AIReal markSize = static_cast<AIReal>(p.markSize);
        const AIReal angle = static_cast<AIReal>(p.gridAngle * M_PI / 180.0);
        const AIReal cosA = static_cast<AIReal>(std::cos(static_cast<double>(angle)));
        const AIReal sinA = static_cast<AIReal>(std::sin(static_cast<double>(angle)));

        AIRealRect inputBounds{};
        error = sAIArt->GetArtBounds(inputArt, &inputBounds);
        if (error != kNoErr) goto simple_legacy_fail;
        const AIReal patternCx = (inputBounds.left + inputBounds.right) / 2;
        const AIReal patternCy = (inputBounds.top + inputBounds.bottom) / 2;

        for (AIArtHandle sourceArt : targets) {
            VHGeometry g;
            error = BuildVHGeometry(sourceArt, 8, g);
            if (error != kNoErr) continue;

            AIPathStyle style;
            error = VHGetSourceMarkStyle(sourceArt, style);
            if (error != kNoErr) goto simple_legacy_fail;

            AIArtHandle markGroup = nullptr;
            if (p.clipToSource) {
                AIArtHandle clipGroup = nullptr;
                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &clipGroup);
                if (error != kNoErr) goto simple_legacy_fail;
                error = sAIGroup->SetGroupClipped(clipGroup, true);
                if (error != kNoErr) goto simple_legacy_fail;

                AIArtHandle mask = nullptr;
                error = sAIArt->DuplicateArt(sourceArt, kPlaceInsideOnTop, clipGroup, &mask);
                if (error != kNoErr) goto simple_legacy_fail;
                error = MakeClipMaskNonPainting(mask);
                if (error != kNoErr) goto simple_legacy_fail;
                error = sAIArt->SetArtUserAttr(mask, kArtIsClipMask, kArtIsClipMask);
                if (error != kNoErr) goto simple_legacy_fail;

                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &markGroup);
                if (error != kNoErr) goto simple_legacy_fail;
            } else {
                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &markGroup);
                if (error != kNoErr) goto simple_legacy_fail;
            }

            if (sVectorHalftoneBlendStyle && p.opacity < 99.999) {
                error = sVectorHalftoneBlendStyle->SetOpacity(markGroup,
                    static_cast<AIReal>(p.opacity / 100.0));
                if (error != kNoErr) goto simple_legacy_fail;
            }

            const AIReal left = g.bounds.left;
            const AIReal right = g.bounds.right;
            const AIReal top = g.bounds.top;
            const AIReal bottom = g.bounds.bottom;
            const LocalBounds lb = RotatedLocalBounds(left, top, right, bottom,
                patternCx, patternCy, cosA, sinA);
            const int rowStart = static_cast<int>(std::floor(static_cast<double>(lb.minY / spacing))) - 2;
            const int rowEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxY / spacing))) + 2;
            const int colStart = static_cast<int>(std::floor(static_cast<double>(lb.minX / spacing))) - 2;
            const int colEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxX / spacing))) + 2;

            for (int row = rowStart; row <= rowEnd; ++row) {
                const AIReal ly = static_cast<AIReal>(row) * spacing;
                const AIReal stagger = (p.stagger && (row & 1)) ? spacing / 2 : 0;
                for (int col = colStart; col <= colEnd; ++col) {
                    const AIReal lx = static_cast<AIReal>(col) * spacing + stagger;
                    const AIReal px = patternCx + lx * cosA - ly * sinA;
                    const AIReal py = patternCy + lx * sinA + ly * cosA;
                    if (!VHPointInside(px, py, g)) continue;
                    error = VHCreateMark(markGroup, p.shape, px, py, markSize, spacing,
                                         static_cast<AIReal>(p.gridAngle), style, true);
                    if (error != kNoErr) goto simple_legacy_fail;
                }
            }
        }

        if (p.preserveSourceAppearance) {
            error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
            if (error != kNoErr) goto simple_legacy_fail;
        }
        outputArt = outputGroup;
        return kNoErr;

    simple_legacy_fail:
        if (outputGroup) sAIArt->DisposeArt(outputGroup);
        return error;
    }

    // v0.7+: direct vector replacement for the tutorial's
    // Rasterize -> Color Halftone -> Image Trace -> Pathfinder Unite ->
    // replaceItems.jsx workflow. A clean regular grid is evaluated directly and
    // every final mark is created as editable Illustrator vector geometry.
    if (p.mode == 2) {
        if (p.spacing <= 0.0 || p.maxSize <= 0.0 || p.minSize < 0.0 ||
            p.maxSize < p.minSize || p.cullSize < 0.0 || p.gradientScale <= 0.0 ||
            p.strokeWidth < 0.0)
            return kBadParameterErr;

        AIErr error = kNoErr;
        AIRealRect inputBounds{};
        error = sAIArt->GetArtBounds(inputArt, &inputBounds);
        if (error != kNoErr) return error;
        if (inputBounds.right <= inputBounds.left || inputBounds.top <= inputBounds.bottom)
            return kBadParameterErr;

        AIArtHandle outputGroup = nullptr;
        error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
        if (error != kNoErr) return error;

        const AIReal spacing = static_cast<AIReal>(p.spacing);
        const AIReal gridRad = static_cast<AIReal>(p.gridAngle * M_PI / 180.0);
        const AIReal gridCos = static_cast<AIReal>(std::cos(static_cast<double>(gridRad)));
        const AIReal gridSin = static_cast<AIReal>(std::sin(static_cast<double>(gridRad)));
        const AIReal cx = (inputBounds.left + inputBounds.right) * static_cast<AIReal>(0.5);
        const AIReal cy = (inputBounds.top + inputBounds.bottom) * static_cast<AIReal>(0.5);

        const LocalBounds lb = RotatedLocalBounds(inputBounds.left - static_cast<AIReal>(p.maxSize),
            inputBounds.top + static_cast<AIReal>(p.maxSize),
            inputBounds.right + static_cast<AIReal>(p.maxSize),
            inputBounds.bottom - static_cast<AIReal>(p.maxSize),
            cx, cy, gridCos, gridSin);
        const int rowStart = static_cast<int>(std::floor(static_cast<double>(lb.minY / spacing))) - 1;
        const int rowEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxY / spacing))) + 1;
        const int colStart = static_cast<int>(std::floor(static_cast<double>(lb.minX / spacing))) - 1;
        const int colEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxX / spacing))) + 1;

        const double transitionScale = (std::max)(0.01, p.gradientScale / 100.0);
        const double transitionCentre = 0.5 + (std::max)(-100.0,
            (std::min)(100.0, p.gradientOffset)) / 200.0;
        const AIReal gradRad = static_cast<AIReal>(p.gradientAngle * M_PI / 180.0);
        const AIReal gradX = static_cast<AIReal>(std::cos(static_cast<double>(gradRad)));
        const AIReal gradY = static_cast<AIReal>(std::sin(static_cast<double>(gradRad)));
        AIReal projMin = static_cast<AIReal>(1e30);
        AIReal projMax = static_cast<AIReal>(-1e30);
        AIReal radialMax = static_cast<AIReal>(0);
        const AIReal bx[4] = {inputBounds.left, inputBounds.right, inputBounds.right, inputBounds.left};
        const AIReal by[4] = {inputBounds.top, inputBounds.top, inputBounds.bottom, inputBounds.bottom};
        for (int i = 0; i < 4; ++i) {
            const AIReal dx = bx[i] - cx;
            const AIReal dy = by[i] - cy;
            const AIReal q = dx * gradX + dy * gradY;
            projMin = (std::min)(projMin, q);
            projMax = (std::max)(projMax, q);
            radialMax = (std::max)(radialMax,
                static_cast<AIReal>(std::hypot(static_cast<double>(dx), static_cast<double>(dy))));
        }

        auto proceduralTone = [&](AIReal px, AIReal py) -> double {
            double raw = 0.0;
            if (p.gradientType == 1) {
                raw = radialMax > static_cast<AIReal>(0)
                    ? std::hypot(static_cast<double>(px - cx), static_cast<double>(py - cy)) /
                        static_cast<double>(radialMax)
                    : 0.0;
            } else {
                const AIReal q = (px - cx) * gradX + (py - cy) * gradY;
                raw = projMax > projMin ? static_cast<double>((q - projMin) / (projMax - projMin)) : 0.0;
            }
            raw = (std::max)(0.0, (std::min)(1.0, raw));
            double tone = (raw - transitionCentre) / transitionScale + 0.5;
            tone = (std::max)(0.0, (std::min)(1.0, tone));
            if (p.reverse) tone = 1.0 - tone;
            return tone;
        };

        auto sanitiseMarkStyle = [](AIPathStyle source) -> AIPathStyle {
            const bool usableFill = source.fillPaint &&
                source.fill.color.kind != kNoneColor &&
                source.fill.color.kind != kGradient &&
                source.fill.color.kind != kPattern &&
                source.fill.color.kind != kAdvanceColor;
            const bool usableStroke = source.strokePaint &&
                source.stroke.color.kind != kNoneColor &&
                source.stroke.color.kind != kGradient &&
                source.stroke.color.kind != kPattern &&
                source.stroke.color.kind != kAdvanceColor;
            if (!usableFill && !usableStroke) return SolidDotStyle(static_cast<AIReal>(1));
            AIPathStyle out = source;
            out.fillPaint = true;
            if (usableFill) out.fill = source.fill;
            else {
                out.fill.color = source.stroke.color;
                out.fill.overprint = source.stroke.overprint;
            }
            out.strokePaint = false;
            return out;
        };

        // Artwork/image sampling cache. It is only constructed when requested.
        AIArtSet sampleSet = nullptr;
        AIArtHandle sampleRaster = nullptr;
        AIRasterRecord sampleInfo{};
        AIRealRect sampleBounds{};
        int sampleWidth = 0, sampleHeight = 0, sampleBytes = 0;
        std::vector<unsigned char> samplePixels;
        auto cleanupSampler = [&]() {
            if (sampleRaster) { sAIArt->DisposeArt(sampleRaster); sampleRaster = nullptr; }
            if (sampleSet) { sVectorHalftoneArtSet->DisposeArtSet(&sampleSet); sampleSet = nullptr; }
        };
        auto failDirect = [&](AIErr e) -> AIErr {
            cleanupSampler();
            if (outputGroup) sAIArt->DisposeArt(outputGroup);
            return e;
        };

        if (p.sourceMode == 1) {
            if (!sVectorHalftoneArtSet || !sVectorHalftoneRasterize || !sVectorHalftoneRaster)
                return failDirect(kBadParameterErr);
            error = sVectorHalftoneArtSet->NewArtSet(&sampleSet);
            if (error != kNoErr) return failDirect(error);
            error = sVectorHalftoneArtSet->AddArtToArtSet(sampleSet, inputArt);
            if (error != kNoErr) return failDirect(error);
            error = sVectorHalftoneRasterize->ComputeArtBounds(sampleSet, &sampleBounds, false);
            if (error != kNoErr) return failDirect(error);
            AIRasterizeSettings rs;
            rs.type = kRasterizeARGB;
            rs.resolution = static_cast<AIReal>(72.0); // 1 document point per sample pixel
            rs.antialiasing = 1;
            rs.options = kRasterizeOptionsDoLayers;
            rs.preserveSpotColors = false;
            error = sVectorHalftoneRasterize->Rasterize(sampleSet, &rs, &sampleBounds,
                kPlaceBelow, inputArt, &sampleRaster, nullptr);
            if (error != kNoErr) return failDirect(error);
            error = sVectorHalftoneRaster->GetRasterInfo(sampleRaster, &sampleInfo);
            if (error != kNoErr) return failDirect(error);
            sampleWidth = sampleInfo.bounds.right - sampleInfo.bounds.left;
            sampleHeight = sampleInfo.bounds.bottom - sampleInfo.bounds.top;
            sampleBytes = sampleInfo.bitsPerPixel / 8;
            if (sampleWidth <= 0 || sampleHeight <= 0 || sampleBytes < 4)
                return failDirect(kBadParameterErr);
            samplePixels.resize(static_cast<size_t>(sampleWidth) * static_cast<size_t>(sampleHeight) *
                static_cast<size_t>(sampleBytes));
            AISlice artSlice{}, workSlice{};
            artSlice.top = workSlice.top = sampleInfo.bounds.top;
            artSlice.bottom = workSlice.bottom = sampleInfo.bounds.bottom;
            artSlice.left = workSlice.left = sampleInfo.bounds.left;
            artSlice.right = workSlice.right = sampleInfo.bounds.right;
            artSlice.back = workSlice.back = sampleBytes;
            AITile tile{};
            tile.data = samplePixels.data();
            tile.bounds = artSlice;
            tile.rowBytes = sampleWidth * sampleBytes;
            tile.colBytes = sampleBytes;
            tile.planeBytes = 0;
            tile.channelInterleave[0] = 3; // R
            tile.channelInterleave[1] = 0; // G
            tile.channelInterleave[2] = 1; // B
            tile.channelInterleave[3] = 2; // A
            for (int i = 4; i < kMaxChannels; ++i) tile.channelInterleave[i] = static_cast<short>(i);
            error = sVectorHalftoneRaster->GetRasterTile(sampleRaster, &artSlice, &tile, &workSlice);
            if (error != kNoErr) return failDirect(error);
        }

        auto sampledTone = [&](AIReal px, AIReal py) -> double {
            if (p.sourceMode != 1) return proceduralTone(px, py);
            if (px < sampleBounds.left || px > sampleBounds.right ||
                py > sampleBounds.top || py < sampleBounds.bottom) return 0.0;
            const double fx = (static_cast<double>(px - sampleBounds.left) /
                static_cast<double>(sampleBounds.right - sampleBounds.left)) * sampleWidth;
            const double fy = (static_cast<double>(sampleBounds.top - py) /
                static_cast<double>(sampleBounds.top - sampleBounds.bottom)) * sampleHeight;
            int ix = (std::max)(0, (std::min)(sampleWidth - 1, static_cast<int>(std::floor(fx))));
            int iy = (std::max)(0, (std::min)(sampleHeight - 1, static_cast<int>(std::floor(fy))));
            const unsigned char* sp = samplePixels.data() +
                (static_cast<size_t>(iy) * sampleWidth + static_cast<size_t>(ix)) * sampleBytes;
            const double alpha = static_cast<double>(sp[3]) / 255.0;
            if (alpha <= 0.001) return 0.0;
            const double lum = (0.2126 * sp[0] + 0.7152 * sp[1] + 0.0722 * sp[2]) / 255.0;
            const double darkness = p.reverse ? lum : (1.0 - lum);
            return (std::max)(0.0, (std::min)(1.0, darkness * alpha));
        };

        if (p.sourceMode == 1) {
            // Artwork/image mode samples the complete rendered appearance, then
            // emits one monochrome vector screen. Alpha naturally suppresses
            // marks outside the source silhouette.
            AIPathStyle markStyle = SolidDotStyle(static_cast<AIReal>(1));
            AIArtHandle markGroup = nullptr;
            short inputType = 0;
            const bool exactVectorClip = p.clipToSource &&
                sAIArt->GetArtType(inputArt, &inputType) == kNoErr &&
                (inputType == kPathArt || inputType == kCompoundPathArt);
            if (exactVectorClip) {
                AIArtHandle clipGroup = nullptr;
                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &clipGroup);
                if (error != kNoErr) return failDirect(error);
                error = sAIGroup->SetGroupClipped(clipGroup, true);
                if (error != kNoErr) return failDirect(error);
                AIArtHandle mask = nullptr;
                error = sAIArt->DuplicateArt(inputArt, kPlaceInsideOnTop, clipGroup, &mask);
                if (error != kNoErr) return failDirect(error);
                error = MakeClipMaskNonPainting(mask);
                if (error != kNoErr) return failDirect(error);
                error = sAIArt->SetArtUserAttr(mask, kArtIsClipMask, kArtIsClipMask);
                if (error != kNoErr) return failDirect(error);
                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &markGroup);
                if (error != kNoErr) return failDirect(error);
            } else {
                error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &markGroup);
                if (error != kNoErr) return failDirect(error);
            }

            for (int row = rowStart; row <= rowEnd; ++row) {
                const AIReal ly = static_cast<AIReal>(row) * spacing;
                const AIReal stagger = (p.stagger && (row & 1)) ? spacing / 2 : 0;
                for (int col = colStart; col <= colEnd; ++col) {
                    const AIReal lx = static_cast<AIReal>(col) * spacing + stagger;
                    const AIReal px = cx + lx * gridCos - ly * gridSin;
                    const AIReal py = cy + lx * gridSin + ly * gridCos;
                    const double tone = sampledTone(px, py);
                    if (tone <= 0.000001) continue;
                    // Photoshop/printing-style halftones preserve tone primarily
                    // through mark AREA, so diameter follows sqrt(tone).
                    const double shapeT = std::sqrt(tone);
                    const AIReal size = static_cast<AIReal>(p.minSize +
                        (p.maxSize - p.minSize) * shapeT);
                    if (size < static_cast<AIReal>(p.cullSize) || size <= static_cast<AIReal>(0.01)) continue;
                    error = VHCreateMark(markGroup, p.shape, px, py, size, spacing,
                        static_cast<AIReal>(p.gridAngle), markStyle, !p.connectStroke,
                        p.connectStroke ? static_cast<AIReal>(p.strokeWidth) : static_cast<AIReal>(0));
                    if (error != kNoErr) return failDirect(error);
                }
            }
        } else {
            std::vector<AIArtHandle> targets;
            error = CollectHalftoneTargets(inputArt, targets);
            if (error != kNoErr) return failDirect(error);
            if (targets.empty()) return failDirect(kBadParameterErr);

            for (AIArtHandle sourceArt : targets) {
                VHGeometry g;
                error = BuildVHGeometry(sourceArt, 8, g);
                if (error != kNoErr) continue;
                AIPathStyle sourceStyle;
                error = VHGetSourceMarkStyle(sourceArt, sourceStyle);
                if (error != kNoErr) return failDirect(error);
                AIPathStyle markStyle = sanitiseMarkStyle(sourceStyle);

                AIArtHandle markGroup = nullptr;
                if (p.clipToSource) {
                    AIArtHandle clipGroup = nullptr;
                    error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &clipGroup);
                    if (error != kNoErr) return failDirect(error);
                    error = sAIGroup->SetGroupClipped(clipGroup, true);
                    if (error != kNoErr) return failDirect(error);
                    AIArtHandle mask = nullptr;
                    error = sAIArt->DuplicateArt(sourceArt, kPlaceInsideOnTop, clipGroup, &mask);
                    if (error != kNoErr) return failDirect(error);
                    error = MakeClipMaskNonPainting(mask);
                    if (error != kNoErr) return failDirect(error);
                    error = sAIArt->SetArtUserAttr(mask, kArtIsClipMask, kArtIsClipMask);
                    if (error != kNoErr) return failDirect(error);
                    error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &markGroup);
                    if (error != kNoErr) return failDirect(error);
                } else {
                    error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &markGroup);
                    if (error != kNoErr) return failDirect(error);
                }

                for (int row = rowStart; row <= rowEnd; ++row) {
                    const AIReal ly = static_cast<AIReal>(row) * spacing;
                    const AIReal stagger = (p.stagger && (row & 1)) ? spacing / 2 : 0;
                    for (int col = colStart; col <= colEnd; ++col) {
                        const AIReal lx = static_cast<AIReal>(col) * spacing + stagger;
                        const AIReal px = cx + lx * gridCos - ly * gridSin;
                        const AIReal py = cy + lx * gridSin + ly * gridCos;
                        if (!VHPointInside(px, py, g)) continue;
                        const double tone = proceduralTone(px, py);
                        const double shapeT = std::sqrt((std::max)(0.0, (std::min)(1.0, tone)));
                        const AIReal size = static_cast<AIReal>(p.minSize +
                            (p.maxSize - p.minSize) * shapeT);
                        if (size < static_cast<AIReal>(p.cullSize) || size <= static_cast<AIReal>(0.01)) continue;
                        error = VHCreateMark(markGroup, p.shape, px, py, size, spacing,
                            static_cast<AIReal>(p.gridAngle), markStyle, !p.connectStroke,
                            p.connectStroke ? static_cast<AIReal>(p.strokeWidth) : static_cast<AIReal>(0));
                        if (error != kNoErr) return failDirect(error);
                    }
                }
            }
        }

        if (p.preserveSourceAppearance) {
            error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
            if (error != kNoErr) return failDirect(error);
        }
        cleanupSampler();
        outputArt = outputGroup;
        return kNoErr;
    }

    // v0.6+: one-click version of the common Illustrator workflow:
    // duplicate the object as a darker shade, create an opacity mask, put a
    // black->white gradient in the mask, then Color Halftone it with all channel
    // angles equal. Here the same result is generated directly as vector mask art.
    if (p.maxRadius <= 0.0 || p.gradientScale <= 0.0 ||
        p.shadeStrength < 0.0 || p.shadeStrength > 100.0 ||
        !sVectorHalftoneMask || !sVectorHalftoneBlendStyle)
        return kBadParameterErr;

    std::vector<AIArtHandle> targets;
    AIErr error = CollectHalftoneTargets(inputArt, targets);
    if (error != kNoErr) return error;
    if (targets.empty()) return kBadParameterErr;

    AIArtHandle outputGroup = nullptr;
    error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
    if (error != kNoErr) return error;

    // No visible shade at zero strength: keep the source artwork exactly as-is.
    if (p.shadeStrength <= 0.001) {
        error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        outputArt = outputGroup;
        return kNoErr;
    }

    const AIReal maxRadius = static_cast<AIReal>(p.maxRadius);
    const AIReal cell = maxRadius * static_cast<AIReal>(std::sqrt(2.0));
    const AIReal screenRad = static_cast<AIReal>(p.screenAngle * M_PI / 180.0);
    const AIReal screenCos = static_cast<AIReal>(std::cos(static_cast<double>(screenRad)));
    const AIReal screenSin = static_cast<AIReal>(std::sin(static_cast<double>(screenRad)));
    AIPathStyle dotStyle = SolidDotStyle(static_cast<AIReal>(1)); // black mask dots

    for (AIArtHandle sourceArt : targets) {
        VHGeometry g;
        error = BuildVHGeometry(sourceArt, 8, g);
        if (error != kNoErr) continue;

        AIPathStyle sourceStyle;
        error = VHGetSourceMarkStyle(sourceArt, sourceStyle);
        if (error != kNoErr) goto simple_shade_fail;

        // The visible shade is a black copy at partial opacity. Compositing black
        // over the source creates a predictable darker version of any source fill,
        // including colours that would not darken reliably with Multiply.
        AIArtHandle shadeArt = nullptr;
        error = sAIArt->DuplicateArt(sourceArt, kPlaceInsideOnTop, outputGroup, &shadeArt);
        if (error != kNoErr) goto simple_shade_fail;
        const AIPathStyle shadeStyle = MakeMonochromeStyle(sourceStyle, static_cast<AIReal>(1));
        error = ApplyPathStyleRecursive(shadeArt, shadeStyle);
        if (error != kNoErr) goto simple_shade_fail;
        error = sVectorHalftoneBlendStyle->SetBlendingMode(shadeArt, kAINormalBlendingMode);
        if (error != kNoErr) goto simple_shade_fail;
        error = sVectorHalftoneBlendStyle->SetOpacity(shadeArt,
            static_cast<AIReal>(p.shadeStrength / 100.0));
        if (error != kNoErr) goto simple_shade_fail;

        // Build the real Illustrator opacity mask. Clip is deliberately OFF,
        // matching the tutorial workflow. A white copy of the source reveals the
        // shade; black halftone dots conceal it and expose the untouched artwork.
        error = sVectorHalftoneMask->CreateMask(shadeArt);
        if (error != kNoErr) goto simple_shade_fail;
        AIMaskRef maskRef = nullptr;
        error = sVectorHalftoneMask->GetMask(shadeArt, &maskRef);
        if (error != kNoErr || !maskRef) goto simple_shade_fail;
        error = sVectorHalftoneMask->SetClipping(maskRef, false);
        if (error != kNoErr) { sVectorHalftoneMask->Release(maskRef); goto simple_shade_fail; }
        error = sVectorHalftoneMask->SetInverted(maskRef, false);
        if (error != kNoErr) { sVectorHalftoneMask->Release(maskRef); goto simple_shade_fail; }
        AIArtHandle maskArt = sVectorHalftoneMask->GetArt(maskRef);
        sVectorHalftoneMask->Release(maskRef);
        maskRef = nullptr;
        if (!maskArt) { error = kCantHappenErr; goto simple_shade_fail; }

        AIArtHandle whiteBase = nullptr;
        error = sAIArt->DuplicateArt(sourceArt, kPlaceInsideOnBottom, maskArt, &whiteBase);
        if (error != kNoErr) goto simple_shade_fail;
        const AIPathStyle whiteStyle = MakeMonochromeStyle(sourceStyle, static_cast<AIReal>(0));
        error = ApplyPathStyleRecursive(whiteBase, whiteStyle);
        if (error != kNoErr) goto simple_shade_fail;
        sVectorHalftoneBlendStyle->SetBlendingMode(whiteBase, kAINormalBlendingMode);
        sVectorHalftoneBlendStyle->SetOpacity(whiteBase, static_cast<AIReal>(1));

        const AIReal left = g.bounds.left;
        const AIReal right = g.bounds.right;
        const AIReal top = g.bounds.top;
        const AIReal bottom = g.bounds.bottom;
        const AIReal cx = (left + right) * static_cast<AIReal>(0.5);
        const AIReal cy = (top + bottom) * static_cast<AIReal>(0.5);

        // Gradient coordinates. Linear uses the projection of the object's four
        // corners. Radial uses the farthest corner as 100%.
        const AIReal gradRad = static_cast<AIReal>(p.gradientAngle * M_PI / 180.0);
        const AIReal gradX = static_cast<AIReal>(std::cos(static_cast<double>(gradRad)));
        const AIReal gradY = static_cast<AIReal>(std::sin(static_cast<double>(gradRad)));
        AIReal projMin = static_cast<AIReal>(1e30);
        AIReal projMax = static_cast<AIReal>(-1e30);
        AIReal radialMax = static_cast<AIReal>(0);
        const AIReal bx[4] = {left, right, right, left};
        const AIReal by[4] = {top, top, bottom, bottom};
        for (int i = 0; i < 4; ++i) {
            const AIReal dx = bx[i] - cx;
            const AIReal dy = by[i] - cy;
            const AIReal q = dx * gradX + dy * gradY;
            projMin = (std::min)(projMin, q);
            projMax = (std::max)(projMax, q);
            radialMax = (std::max)(radialMax,
                static_cast<AIReal>(std::hypot(static_cast<double>(dx), static_cast<double>(dy))));
        }

        // Screen grid is centred on each source shape, matching Illustrator's
        // normal gradient-mask workflow. Expand by one radius so partial edge dots
        // are not lost when their centres sit just outside the object's bounds.
        const LocalBounds lb = RotatedLocalBounds(left - maxRadius, top + maxRadius,
            right + maxRadius, bottom - maxRadius, cx, cy, screenCos, screenSin);
        const int rowStart = static_cast<int>(std::floor(static_cast<double>(lb.minY / cell))) - 1;
        const int rowEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxY / cell))) + 1;
        const int colStart = static_cast<int>(std::floor(static_cast<double>(lb.minX / cell))) - 1;
        const int colEnd = static_cast<int>(std::ceil(static_cast<double>(lb.maxX / cell))) + 1;

        const double transitionScale = (std::max)(0.01, p.gradientScale / 100.0);
        const double transitionCentre = 0.5 + (std::max)(-100.0,
            (std::min)(100.0, p.gradientOffset)) / 200.0;

        for (int row = rowStart; row <= rowEnd; ++row) {
            const AIReal ly = static_cast<AIReal>(row) * cell;
            for (int col = colStart; col <= colEnd; ++col) {
                const AIReal lx = static_cast<AIReal>(col) * cell;
                const AIReal px = cx + lx * screenCos - ly * screenSin;
                const AIReal py = cy + lx * screenSin + ly * screenCos;

                double raw = 0.0;
                if (p.gradientType == 1) {
                    raw = radialMax > static_cast<AIReal>(0)
                        ? std::hypot(static_cast<double>(px - cx), static_cast<double>(py - cy)) /
                            static_cast<double>(radialMax)
                        : 0.0;
                } else {
                    const AIReal q = (px - cx) * gradX + (py - cy) * gradY;
                    raw = projMax > projMin ? static_cast<double>((q - projMin) / (projMax - projMin)) : 0.0;
                }
                raw = (std::max)(0.0, (std::min)(1.0, raw));

                double whiteAmount = (raw - transitionCentre) / transitionScale + 0.5;
                whiteAmount = (std::max)(0.0, (std::min)(1.0, whiteAmount));
                if (p.reverse) whiteAmount = 1.0 - whiteAmount;

                // Color Halftone converts black portions of the opacity-mask
                // gradient into black dots. White reveals the darker duplicate.
                const double blackCoverage = 1.0 - whiteAmount;
                const double radius = PhotoshopDotRadiusPixels(blackCoverage,
                    static_cast<double>(maxRadius));
                if (radius <= 0.025) continue;

                error = CreateFastHalftoneCircle(maskArt, px, py,
                    static_cast<AIReal>(radius * 2.0), dotStyle);
                if (error != kNoErr) goto simple_shade_fail;
            }
        }
    }

    // The tutorial starts with the original artwork, then puts the masked darker
    // duplicate above it. Do the same inside the Live Effect output group.
    error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
    if (error != kNoErr) goto simple_shade_fail;

    outputArt = outputGroup;
    return kNoErr;

simple_shade_fail:
    if (outputGroup) sAIArt->DisposeArt(outputGroup);
    return error;
}

ASErr VectorHalftoneEffectPlugin::BuildPhotoshopHalftone(AIArtHandle inputArt, AIArtHandle& outputArt,
                                                          const VectorPhotoshopHalftoneParams& p) const {
    outputArt = nullptr;
    if (!inputArt || p.maxRadius <= 0 || p.sampleDpi < 36 ||
        !sVectorHalftoneArtSet || !sVectorHalftoneRasterize || !sVectorHalftoneRaster ||
        !sVectorHalftoneBlendStyle)
        return kBadParameterErr;

    AIErr error = kNoErr;
    AIArtSet set = nullptr;
    AIArtHandle raster = nullptr;
    AIArtHandle outputGroup = nullptr;
    AIRealRect bounds{};
    AIRasterizeSettings settings;
    AIRasterRecord info{};
    int width = 0, height = 0, bytes = 0;
    std::vector<unsigned char> pixels;
    AISlice artSlice{}, workSlice{};
    AITile tile{};

    ai::int16 documentColorModel = kDocRGBColor;
    if (sVectorHalftoneDocument) {
        ai::int16 model = kDocUnknownColor;
        if (sVectorHalftoneDocument->GetDocumentColorModel(&model) == kNoErr &&
            (model == kDocRGBColor || model == kDocCMYKColor))
            documentColorModel = model;
    }
    const bool rgbMode = documentColorModel != kDocCMYKColor;
    const int channelCount = rgbMode ? 3 : 4;

    auto cleanup = [&]() {
        if (raster) { sAIArt->DisposeArt(raster); raster = nullptr; }
        if (set) { sVectorHalftoneArtSet->DisposeArtSet(&set); set = nullptr; }
    };
    auto fail = [&](AIErr e) -> AIErr {
        cleanup();
        if (outputGroup) { sAIArt->DisposeArt(outputGroup); outputGroup = nullptr; }
        return e;
    };

    error = sVectorHalftoneArtSet->NewArtSet(&set);
    if (error != kNoErr) return error;
    error = sVectorHalftoneArtSet->AddArtToArtSet(set, inputArt);
    if (error != kNoErr) return fail(error);
    error = sVectorHalftoneRasterize->ComputeArtBounds(set, &bounds, false);
    if (error != kNoErr) return fail(error);
    if (bounds.right <= bounds.left || bounds.top <= bounds.bottom) return fail(kBadParameterErr);

    // Match the Photoshop document-channel model first, then vectorise the
    // screened channels. RGB mode is the calibrated path used by the supplied
    // Photoshop 2026 reference set.
    settings.type = rgbMode ? kRasterizeARGB : kRasterizeACMYK;
    settings.resolution = static_cast<AIReal>(p.sampleDpi);
    settings.antialiasing = 1;
    settings.options = kRasterizeOptionsDoLayers;
    settings.preserveSpotColors = false;
    error = sVectorHalftoneRasterize->Rasterize(set, &settings, &bounds,
                                                 kPlaceBelow, inputArt, &raster, nullptr);
    if (error != kNoErr) return fail(error);

    error = sVectorHalftoneRaster->GetRasterInfo(raster, &info);
    if (error != kNoErr) return fail(error);
    width = info.bounds.right - info.bounds.left;
    height = info.bounds.bottom - info.bounds.top;
    bytes = info.bitsPerPixel / 8;
    if (width <= 0 || height <= 0 || bytes < 4) return fail(kBadParameterErr);

    pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(bytes));
    artSlice.top = workSlice.top = info.bounds.top;
    artSlice.bottom = workSlice.bottom = info.bounds.bottom;
    artSlice.left = workSlice.left = info.bounds.left;
    artSlice.right = workSlice.right = info.bounds.right;
    artSlice.back = workSlice.back = bytes;
    tile.data = pixels.data();
    tile.bounds = artSlice;
    tile.rowBytes = width * bytes;
    tile.colBytes = bytes;
    tile.planeBytes = 0;

    if (rgbMode) {
        // AIRaster ARGB storage is not ordinary RGBA. This interleave requests
        // the byte order R,G,B,A so channel averages match Photoshop's RGB
        // channel values directly.
        tile.channelInterleave[0] = 3;
        tile.channelInterleave[1] = 0;
        tile.channelInterleave[2] = 1;
        tile.channelInterleave[3] = 2;
        for (int i = 4; i < kMaxChannels; ++i) tile.channelInterleave[i] = static_cast<short>(i);
    } else {
        // ACMYK storage is A,C,M,Y,K. Request a C,M,Y,K,A byte layout so the
        // first four bytes remain process channels and byte 4 is alpha.
        tile.channelInterleave[0] = 4;
        tile.channelInterleave[1] = 0;
        tile.channelInterleave[2] = 1;
        tile.channelInterleave[3] = 2;
        tile.channelInterleave[4] = 3;
        for (int i = 5; i < kMaxChannels; ++i) tile.channelInterleave[i] = static_cast<short>(i);
    }

    error = sVectorHalftoneRaster->GetRasterTile(raster, &artSlice, &tile, &workSlice);
    if (error != kNoErr) return fail(error);

    // Cheap whole-raster scan lets flat/saturated artwork completely skip
    // channels that cannot create any dots. This is a large win for common
    // design artwork while leaving active channel sampling unchanged.
    bool channelHasInk[4] = {false, false, false, false};
    const int alphaOffset = rgbMode ? 3 : 4;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        const unsigned char* px = pixels.data() + pixelIndex * static_cast<size_t>(bytes);
        if (px[alphaOffset] == 0) continue;
        for (int scanCh = 0; scanCh < channelCount; ++scanCh) {
            if (channelHasInk[scanCh]) continue;
            channelHasInk[scanCh] = rgbMode ? (px[scanCh] < 255) : (px[scanCh] > 0);
        }
        bool allActive = true;
        for (int scanCh = 0; scanCh < channelCount; ++scanCh)
            allActive = allActive && channelHasInk[scanCh];
        if (allActive) break;
    }

    error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
    if (error != kNoErr) return fail(error);
    sVectorHalftoneBlendStyle->SetIsolated(outputGroup, true);

    // Photoshop preserves the source alpha/silhouette. For a vector path or
    // compound path we can reproduce that exactly with a genuine Illustrator
    // clipping path instead of clipping to the rectangular raster bounds. This
    // is what prevents the white rectangle and the ring of full circles that
    // used to protrude beyond a circular source object.
    AIArtHandle clipGroup = nullptr;
    AIArtHandle contentGroup = nullptr;
    error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &clipGroup);
    if (error != kNoErr) return fail(error);
    error = sAIGroup->SetGroupClipped(clipGroup, true);
    if (error != kNoErr) return fail(error);

    short inputType = 0;
    const bool canUseSourceVectorClip =
        sAIArt->GetArtType(inputArt, &inputType) == kNoErr &&
        (inputType == kPathArt || inputType == kCompoundPathArt);

    AIArtHandle clipMask = nullptr;
    if (canUseSourceVectorClip) {
        error = sAIArt->DuplicateArt(inputArt, kPlaceInsideOnTop, clipGroup, &clipMask);
        if (error != kNoErr) return fail(error);
        error = MakeClipMaskNonPainting(clipMask);
        if (error != kNoErr) return fail(error);
    } else {
        // Groups/plugin art cannot be used directly as a single Illustrator
        // clipping path. Keep the raster-bounds fallback for those inputs; the
        // alpha-aware cell coverage below stops transparent pixels generating
        // full-size edge dots, and no rectangular background is painted.
        AIPathStyle clipStyle;
        clipStyle.Init();
        clipStyle.fillPaint = false;
        clipStyle.strokePaint = false;
        error = CreateFilledRectangle(clipGroup, bounds, clipStyle, clipMask);
        if (error != kNoErr) return fail(error);
    }
    error = sAIArt->SetArtUserAttr(clipMask, kArtIsClipMask, kArtIsClipMask);
    if (error != kNoErr) return fail(error);
    error = sAIArt->ReorderArt(clipMask, kPlaceInsideOnTop, clipGroup);
    if (error != kNoErr) return fail(error);
    error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &contentGroup);
    if (error != kNoErr) return fail(error);
    sVectorHalftoneBlendStyle->SetIsolated(contentGroup, true);

    // The calibrated Photoshop RGB result is NOT "RGB dots on black". Each
    // RGB image channel is independently converted into a black-dot screen on
    // white. In vector form the exact compositing equivalent is:
    //   R-channel darkness -> Cyan circles
    //   G-channel darkness -> Magenta circles
    //   B-channel darkness -> Yellow circles
    // all multiplied over white. Their overlaps produce the same RGB channel
    // combinations as Photoshop (C/M/Y, R/G/B and black rosettes).
    AIPathStyle backgroundStyle;
    backgroundStyle.Init();
    backgroundStyle.fillPaint = true;
    backgroundStyle.strokePaint = false;
    if (rgbMode) {
        backgroundStyle.fill.color.kind = kThreeColor;
        backgroundStyle.fill.color.c.rgb.red = 1;
        backgroundStyle.fill.color.c.rgb.green = 1;
        backgroundStyle.fill.color.c.rgb.blue = 1;
    } else {
        backgroundStyle.fill.color.kind = kFourColor;
        backgroundStyle.fill.color.c.f.cyan = 0;
        backgroundStyle.fill.color.c.f.magenta = 0;
        backgroundStyle.fill.color.c.f.yellow = 0;
        backgroundStyle.fill.color.c.f.black = 0;
    }
    // Photoshop keeps opaque white pixels white, but it does not invent an
    // opaque rectangle outside transparent artwork. When we have an exact
    // vector silhouette the white base is safe because the clip path confines
    // it to the source shape. For group/plugin-art fallbacks, omit the base so
    // transparent raster bounds never appear as a white box.
    if (canUseSourceVectorClip) {
        AIArtHandle background = nullptr;
        error = CreateFilledRectangle(contentGroup, bounds, backgroundStyle, background);
        if (error != kNoErr) return fail(error);
    }

    const double angles[4] = {p.angle1, p.angle2, p.angle3, p.angle4};
    const double maxRadiusPx = p.maxRadius;
    const double cellPx = maxRadiusPx * std::sqrt(2.0);
    const double imageCxPx = (static_cast<double>(width) - 1.0) * 0.5;
    const double imageCyPx = (static_cast<double>(height) - 1.0) * 0.5;
    const AIReal worldCx = (bounds.left + bounds.right) * static_cast<AIReal>(0.5);
    const AIReal worldCy = (bounds.top + bounds.bottom) * static_cast<AIReal>(0.5);
    const double pxToPtX = static_cast<double>(bounds.right - bounds.left) / static_cast<double>(width);
    const double pxToPtY = static_cast<double>(bounds.top - bounds.bottom) / static_cast<double>(height);
    const double radiusScalePt = 72.0 / p.sampleDpi;

    const double maxRadiusPt = maxRadiusPx * radiusScalePt;
    const double minDiameterPt = (std::max)(0.05, static_cast<double>(p.cullSize));

    for (int ch = 0; ch < channelCount; ++ch) {
        if (!channelHasInk[ch]) continue;

        AIPathStyle style;
        style.Init();
        style.fillPaint = true;
        style.strokePaint = false;
        if (rgbMode) {
            style.fill.color.kind = kThreeColor;
            // Complement of the Photoshop RGB channel being screened.
            style.fill.color.c.rgb.red   = ch == 0 ? 0 : 1;
            style.fill.color.c.rgb.green = ch == 1 ? 0 : 1;
            style.fill.color.c.rgb.blue  = ch == 2 ? 0 : 1;
        } else {
            style.fill.color.kind = kFourColor;
            style.fill.color.c.f.cyan = ch == 0 ? 1 : 0;
            style.fill.color.c.f.magenta = ch == 1 ? 1 : 0;
            style.fill.color.c.f.yellow = ch == 2 ? 1 : 0;
            style.fill.color.c.f.black = ch == 3 ? 1 : 0;
        }

        const double a = angles[ch] * M_PI / 180.0;
        const double ca = std::cos(a);
        const double sa = std::sin(a);

        // Photoshop anchors every rotated screen to the exact image centre,
        // (width-1)/2,(height-1)/2 in pixel-centre coordinates. This phase was
        // measured from the supplied 0/15/30/45/60/75/90 degree references.
        const double cornerDx[4] = {
            -imageCxPx, static_cast<double>(width - 1) - imageCxPx,
            static_cast<double>(width - 1) - imageCxPx, -imageCxPx
        };
        const double cornerDy[4] = {
            -imageCyPx, -imageCyPx,
            static_cast<double>(height - 1) - imageCyPx,
            static_cast<double>(height - 1) - imageCyPx
        };
        double minU = 0, maxU = 0, minV = 0, maxV = 0;
        for (int i = 0; i < 4; ++i) {
            const double u = cornerDx[i] * ca + cornerDy[i] * sa;
            const double v = -cornerDx[i] * sa + cornerDy[i] * ca;
            if (i == 0) { minU = maxU = u; minV = maxV = v; }
            else {
                minU = (std::min)(minU, u); maxU = (std::max)(maxU, u);
                minV = (std::min)(minV, v); maxV = (std::max)(maxV, v);
            }
        }

        const int colStart = static_cast<int>(std::floor(minU / cellPx)) - 1;
        const int colEnd   = static_cast<int>(std::ceil (maxU / cellPx)) + 1;
        const int rowStart = static_cast<int>(std::floor(minV / cellPx)) - 1;
        const int rowEnd   = static_cast<int>(std::ceil (maxV / cellPx)) + 1;
        const int cols = colEnd - colStart + 1;
        const int rows = rowEnd - rowStart + 1;
        if (cols <= 0 || rows <= 0) continue;

        const size_t cellCount = static_cast<size_t>(cols) * static_cast<size_t>(rows);
        std::vector<double> sums(cellCount, 0.0);
        std::vector<double> alphaSums(cellCount, 0.0);
        // Exact vector clipping does not use cell alpha coverage, so avoid a
        // third per-cell buffer in the common path/compound-path case.
        std::vector<std::uint32_t> sampleCounts;
        if (!canUseSourceVectorClip) sampleCounts.assign(cellCount, 0);

        // Precompute the X portion of the rotation. This preserves the same
        // screen maths while replacing repeated per-pixel multiplications with
        // two table lookups and additions. Y terms are computed once per row.
        std::vector<double> xU(static_cast<size_t>(width));
        std::vector<double> xV(static_cast<size_t>(width));
        for (int ix = 0; ix < width; ++ix) {
            const double dx = static_cast<double>(ix) - imageCxPx;
            xU[static_cast<size_t>(ix)] = dx * ca;
            xV[static_cast<size_t>(ix)] = -dx * sa;
        }

        // Photoshop averages the source channel over each ROTATED square screen
        // cell. Alpha weighting and cell assignment are unchanged from v0.5.3.
        for (int iy = 0; iy < height; ++iy) {
            const double dy = static_cast<double>(iy) - imageCyPx;
            const double yU = dy * sa;
            const double yV = dy * ca;
            const unsigned char* pixel = pixels.data() +
                static_cast<size_t>(iy) * static_cast<size_t>(width) * static_cast<size_t>(bytes);

            for (int ix = 0; ix < width; ++ix, pixel += bytes) {
                const double u = xU[static_cast<size_t>(ix)] + yU;
                const double v = xV[static_cast<size_t>(ix)] + yV;
                const int col = static_cast<int>(std::floor(u / cellPx + 0.5));
                const int row = static_cast<int>(std::floor(v / cellPx + 0.5));
                if (col < colStart || col > colEnd || row < rowStart || row > rowEnd) continue;

                const size_t ci = static_cast<size_t>(row - rowStart) * static_cast<size_t>(cols) +
                                  static_cast<size_t>(col - colStart);
                if (!canUseSourceVectorClip) ++sampleCounts[ci];

                const unsigned char alphaByte = pixel[alphaOffset];
                if (alphaByte == 0) continue;
                const double alpha = alphaByte / 255.0;
                const double value = pixel[ch] / 255.0;
                sums[ci] += value * alpha;
                alphaSums[ci] += alpha;
            }
        }

        // Do not create the Illustrator channel group until we find the first
        // surviving dot. Empty screens therefore add no art-tree overhead.
        AIArtHandle channelGroup = nullptr;
        auto ensureChannelGroup = [&]() -> AIErr {
            if (channelGroup) return kNoErr;
            AIErr e = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, contentGroup, &channelGroup);
            if (e != kNoErr) return e;
            e = sVectorHalftoneBlendStyle->SetBlendingMode(channelGroup, kAIMultiplyBlendingMode);
            if (e != kNoErr) return e;
            return sVectorHalftoneBlendStyle->SetOpacity(channelGroup, static_cast<AIReal>(1.0));
        };

        for (int row = rowStart; row <= rowEnd; ++row) {
            const double screenV = static_cast<double>(row) * cellPx;
            for (int col = colStart; col <= colEnd; ++col) {
                const size_t ci = static_cast<size_t>(row - rowStart) * static_cast<size_t>(cols) +
                                  static_cast<size_t>(col - colStart);
                if (alphaSums[ci] <= 0.0) continue;

                double average = sums[ci] / alphaSums[ci];
                average = (std::max)(0.0, (std::min)(1.0, average));
                double coverage = rgbMode ? (1.0 - average) : average;
                if (!canUseSourceVectorClip) {
                    if (sampleCounts[ci] == 0) continue;
                    const double alphaCoverage = (std::max)(0.0,
                        (std::min)(1.0, alphaSums[ci] / static_cast<double>(sampleCounts[ci])));
                    coverage *= alphaCoverage;
                }
                if (coverage <= 0.0) continue;

                const double dotRadiusPx = PhotoshopDotRadiusPixels(coverage, maxRadiusPx);
                if (dotRadiusPx <= 0.0) continue;
                const double diameterPt = 2.0 * dotRadiusPx * radiusScalePt;
                if (diameterPt < minDiameterPt) continue;

                const double screenU = static_cast<double>(col) * cellPx;
                const double dxPx = screenU * ca - screenV * sa;
                const double dyPx = screenU * sa + screenV * ca;
                const AIReal px = worldCx + static_cast<AIReal>(dxPx * pxToPtX);
                const AIReal py = worldCy - static_cast<AIReal>(dyPx * pxToPtY);

                // Only centres whose Photoshop circle can touch the image need
                // vector art. The clip group handles the final exact boundary.
                if (px < bounds.left - maxRadiusPt || px > bounds.right + maxRadiusPt ||
                    py > bounds.top + maxRadiusPt || py < bounds.bottom - maxRadiusPt)
                    continue;

                error = ensureChannelGroup();
                if (error != kNoErr) return fail(error);
                error = CreateFastHalftoneCircle(channelGroup, px, py,
                    static_cast<AIReal>(diameterPt), style);
                if (error != kNoErr) return fail(error);
            }
        }
    }

    cleanup();
    if (p.preserveSourceAppearance) {
        error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
        if (error != kNoErr) {
            sAIArt->DisposeArt(outputGroup);
            return error;
        }
    }
    outputArt = outputGroup;
    return kNoErr;
}

