#define _USE_MATH_DEFINES
#include "IllustratorSDK.h"
#include "VectorHalftoneEffectPlugin.h"
#include "VectorHalftoneEffectSuites.h"
#include "VectorHalftoneGeometry.h"
#include "VectorHalftoneDialogWin.h"
#include "SDKErrors.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

AIReal Clamp01(AIReal v) {
    return (std::max)(static_cast<AIReal>(0), (std::min)(static_cast<AIReal>(1), v));
}

AIReal SoftRamp(AIReal t, AIReal softness) {
    t = Clamp01(t);
    softness = Clamp01(softness);
    const AIReal smooth = t * t * (static_cast<AIReal>(3) - static_cast<AIReal>(2) * t);
    return t + (smooth - t) * softness;
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

} // namespace

Plugin* AllocatePlugin(SPPluginRef pluginRef) {
    return new VectorHalftoneEffectPlugin(pluginRef);
}

void FixupReload(Plugin* plugin) {
    VectorHalftoneEffectPlugin::FixupVTable(static_cast<VectorHalftoneEffectPlugin*>(plugin));
}

VectorHalftoneEffectPlugin::VectorHalftoneEffectPlugin(SPPluginRef pluginRef)
    : Plugin(pluginRef), fLiveEffect(nullptr) {
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
    AIErr error = kNoErr;

    AILiveEffectData effectData{};
    effectData.self = message->d.self;
    effectData.name = kVectorHalftoneEffectName;

    char effectTitle[128]{};
    ai::UnicodeString(kVectorHalftoneEffectTitle, kAIUTF8CharacterEncoding).as_Platform(effectTitle, 128);
    effectData.title = effectTitle;
    effectData.majorVersion = 3;
    effectData.minorVersion = 4;
    effectData.prefersAsInput = kPathInputArt | kCompoundPathInputArt | kGroupInputArt;
    effectData.styleFilterFlags = kPostEffectFilter;

    error = sAILiveEffect->AddLiveEffect(&effectData, &fLiveEffect);
    if (error != kNoErr) return error;

    AddLiveEffectMenuData menuData{};
    char category[128]{};
    char menuTitle[128]{};
    ai::UnicodeString(kVectorHalftoneEffectCategory, kAIUTF8CharacterEncoding).as_Platform(category, 128);
    ai::UnicodeString(kVectorHalftoneEffectMenuTitle, kAIUTF8CharacterEncoding).as_Platform(menuTitle, 128);
    menuData.category = category;
    menuData.title = menuTitle;
    menuData.options = 0;

    return sAILiveEffect->AddLiveEffectMenuItem(fLiveEffect, effectData.name, &menuData, nullptr, nullptr);
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
    if (!inputArt || p.spacing <= 0.0 || p.maxSize <= 0.0 || p.glowWidth <= 0.0) return kBadParameterErr;

    VHGeometry geometry;
    AIErr error = BuildVHGeometry(inputArt, p.curveSamples, geometry);
    if (error != kNoErr) return error;

    // v0.3.2: register as a post-effect and copy the actual source fill/stroke
    // style. Earlier v0.3 builds were pre-effects, so Illustrator had not yet
    // applied the Fill/Stroke appearance entries when this code ran.
    AIPathStyle markStyle;
    VHGetSourceMarkStyle(inputArt, markStyle);

    AIArtHandle outputGroup = nullptr;
    error = sAIArt->NewArt(kGroupArt, kPlaceBelow, inputArt, &outputGroup);
    if (error != kNoErr) return error;

    AIArtHandle markParent = outputGroup;
    AIArtHandle clipGroup = nullptr;
    AIArtHandle marksGroup = nullptr;

    // Exact clipping is meaningful for inward halftones. Outward mode is the
    // inverse region, so it remains unclipped and is bounded by glowWidth.
    const bool useExactClip = p.clipToSource && p.glowMode == 0;
    if (useExactClip) {
        error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &clipGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        error = sAIGroup->SetGroupClipped(clipGroup, true);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }

        AIArtHandle maskArt = nullptr;
        error = sAIArt->DuplicateArt(inputArt, kPlaceInsideOnTop, clipGroup, &maskArt);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        error = sAIArt->SetArtUserAttr(maskArt, kArtIsClipMask, kArtIsClipMask);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }

        error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnBottom, clipGroup, &marksGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        markParent = marksGroup;
    } else {
        error = sAIArt->NewArt(kGroupArt, kPlaceInsideOnTop, outputGroup, &marksGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
        markParent = marksGroup;
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

    const AIReal width = right - left;
    const AIReal height = top - bottom;
    if (width <= 0 || height <= 0) {
        sAIArt->DisposeArt(outputGroup);
        return kBadParameterErr;
    }

    const AIReal cx = (geometry.bounds.left + geometry.bounds.right) / 2;
    const AIReal cy = (geometry.bounds.top + geometry.bounds.bottom) / 2;
    const AIReal radians = static_cast<AIReal>(p.gridAngle * M_PI / 180.0);
    const AIReal cosA = static_cast<AIReal>(std::cos(static_cast<double>(radians)));
    const AIReal sinA = static_cast<AIReal>(std::sin(static_cast<double>(radians)));

    // Use the rotated rectangle's actual local extents instead of a diagonal
    // square. This removes a large number of candidate marks at non-zero angles.
    LocalBounds local = RotatedLocalBounds(left, top, right, bottom, cx, cy, cosA, sinA);
    const AIReal spacing = static_cast<AIReal>(p.spacing);
    const int rowStart = static_cast<int>(std::floor(static_cast<double>(local.minY / spacing))) - 1;
    const int rowEnd = static_cast<int>(std::ceil(static_cast<double>(local.maxY / spacing))) + 1;
    const int colStart = static_cast<int>(std::floor(static_cast<double>(local.minX / spacing))) - 1;
    const int colEnd = static_cast<int>(std::ceil(static_cast<double>(local.maxX / spacing))) + 1;

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
            } else {
                if (inside) continue;
            }

            const AIReal distance = VHDistanceToEdge(px, py, geometry);
            if (p.glowMode == 1 && distance > static_cast<AIReal>(p.glowWidth)) continue;

            AIReal toneT = 0;
            if (p.glowMode == 0) {
                toneT = Clamp01(distance / static_cast<AIReal>(p.glowWidth));
            } else {
                toneT = static_cast<AIReal>(1) - Clamp01(distance / static_cast<AIReal>(p.glowWidth));
            }
            toneT = SoftRamp(toneT, static_cast<AIReal>(p.edgeSoftness));

            AIReal density = static_cast<AIReal>((1.0 - p.glowStrength) + p.glowStrength * toneT);
            density = static_cast<AIReal>(std::pow(static_cast<double>(Clamp01(density)), p.gamma));
            if (p.invert) density = static_cast<AIReal>(1) - density;

            AIReal markSize = static_cast<AIReal>(p.minSize + (p.maxSize - p.minSize) * density);
            int markShape = p.shape;
            bool suppressStroke = true; // repeated per-mark strokes overlap; fill/stroke colours are inherited as fill.
            AIReal markAngle = p.followCurve
                ? VHNearestEdgeNormalAngle(px, py, geometry)
                : static_cast<AIReal>(p.gridAngle);

            // Solid-centre mode needs to become genuinely continuous. For the
            // core region, greatly overlap the selected mark family and suppress
            // source strokes so outlines do not stack into dark seams.
            if (p.solidCenter && p.glowMode == 0) {
                const AIReal coreStart = (std::max)(spacing * static_cast<AIReal>(0.45),
                    static_cast<AIReal>(p.glowWidth) * static_cast<AIReal>(0.64));
                if (distance >= coreStart) {
                    AIReal multiplier = static_cast<AIReal>(1.90);
                    switch (markShape) {
                    case 0: // circle
                        multiplier = static_cast<AIReal>(1.82);
                        break;
                    case 1: // square
                        multiplier = static_cast<AIReal>(1.72);
                        break;
                    case 2: // diamond
                        multiplier = static_cast<AIReal>(2.10);
                        break;
                    case 3: // triangle
                        multiplier = static_cast<AIReal>(2.45);
                        break;
                    case 4: // hexagon
                        multiplier = static_cast<AIReal>(1.72);
                        break;
                    case 5: // star
                        multiplier = static_cast<AIReal>(2.55);
                        break;
                    case 6: // line
                        multiplier = static_cast<AIReal>(2.35);
                        break;
                    default:
                        break;
                    }
                    markSize = (std::max)(markSize, spacing * multiplier);
                }
            }

            if (p.cullSize > 0.0 && markSize < static_cast<AIReal>(p.cullSize)) continue;

            // Legacy v0.1 edge containment remains active for existing artwork.
            if (p.containDots && p.glowMode == 0 && !useExactClip) {
                const AIReal maxContained = (markShape == 1)
                    ? distance * static_cast<AIReal>(std::sqrt(2.0))
                    : distance * 2;
                markSize = (std::min)(markSize, maxContained);
            }
            if (markSize <= static_cast<AIReal>(0.05)) continue;

            error = VHCreateMark(markParent, markShape, px, py, markSize, spacing,
                                 markAngle, markStyle, suppressStroke);
            if (error != kNoErr) {
                sAIArt->DisposeArt(outputGroup);
                return error;
            }
        }
    }

    if (p.preserveSourceAppearance) {
        error = sAIArt->ReorderArt(inputArt, kPlaceInsideOnBottom, outputGroup);
        if (error != kNoErr) { sAIArt->DisposeArt(outputGroup); return error; }
    }

    outputArt = outputGroup;
    return kNoErr;
}
