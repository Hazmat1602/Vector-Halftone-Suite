#define _USE_MATH_DEFINES
#include "IllustratorSDK.h"
#include "VectorHalftoneGeometry.h"
#include "VectorHalftoneEffectSuites.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

bool NearlyEqual(AIReal a, AIReal b) {
    return std::fabs(static_cast<double>(a - b)) < 0.0001;
}

bool PointEqual(const AIRealPoint& a, const AIRealPoint& b) {
    return NearlyEqual(a.h, b.h) && NearlyEqual(a.v, b.v);
}

VHPoint CubicPoint(const AIRealPoint& p0, const AIRealPoint& p1,
                   const AIRealPoint& p2, const AIRealPoint& p3, AIReal t) {
    const AIReal mt = 1 - t;
    const AIReal mt2 = mt * mt;
    const AIReal t2 = t * t;
    const AIReal a = mt2 * mt;
    const AIReal b = 3 * mt2 * t;
    const AIReal c = 3 * mt * t2;
    const AIReal d = t * t2;
    return {
        a * p0.h + b * p1.h + c * p2.h + d * p3.h,
        a * p0.v + b * p1.v + c * p2.v + d * p3.v
    };
}

AIErr FlattenPath(AIArtHandle path, int samplesPerBezier, std::vector<VHPoint>& out) {
    AIErr error = kNoErr;
    ai::int16 count = 0;
    error = sAIPath->GetPathSegmentCount(path, &count);
    if (error != kNoErr || count < 2) return error;

    AIBoolean closed = false;
    error = sAIPath->GetPathClosed(path, &closed);
    if (error != kNoErr || !closed) return error;

    std::vector<AIPathSegment> points(static_cast<size_t>(count));
    error = sAIPath->GetPathSegments(path, 0, count, points.data());
    if (error != kNoErr) return error;

    samplesPerBezier = (std::max)(2, samplesPerBezier);
    out.clear();
    out.push_back({points[0].p.h, points[0].p.v});

    for (ai::int16 i = 0; i < count; ++i) {
        const ai::int16 next = static_cast<ai::int16>((i + 1) % count);
        const AIPathSegment& a = points[static_cast<size_t>(i)];
        const AIPathSegment& b = points[static_cast<size_t>(next)];
        const bool straight = PointEqual(a.p, a.out) && PointEqual(b.p, b.in);

        if (straight) {
            if (i != count - 1) out.push_back({b.p.h, b.p.v});
        } else {
            for (int s = 1; s <= samplesPerBezier; ++s) {
                if (i == count - 1 && s == samplesPerBezier) continue;
                const AIReal t = static_cast<AIReal>(s) / static_cast<AIReal>(samplesPerBezier);
                out.push_back(CubicPoint(a.p, a.out, b.in, b.p, t));
            }
        }
    }
    return kNoErr;
}

VHRing MakeRing(std::vector<VHPoint>&& points) {
    VHRing ring;
    ring.points = std::move(points);
    if (ring.points.empty()) return ring;
    ring.minX = ring.maxX = ring.points[0].x;
    ring.minY = ring.maxY = ring.points[0].y;
    for (const auto& p : ring.points) {
        ring.minX = (std::min)(ring.minX, p.x);
        ring.minY = (std::min)(ring.minY, p.y);
        ring.maxX = (std::max)(ring.maxX, p.x);
        ring.maxY = (std::max)(ring.maxY, p.y);
    }
    return ring;
}

void AppendRingSegments(const VHRing& ring, std::vector<VHSegment>& segments) {
    if (ring.points.size() < 3) return;
    for (size_t i = 0; i < ring.points.size(); ++i) {
        const size_t j = (i + 1) % ring.points.size();
        const VHPoint& a = ring.points[i];
        const VHPoint& b = ring.points[j];
        segments.push_back({
            a, b,
            (std::min)(a.x, b.x), (std::min)(a.y, b.y),
            (std::max)(a.x, b.x), (std::max)(a.y, b.y)
        });
    }
}

AIErr AppendGeometryFromArt(AIArtHandle art, int samplesPerBezier, std::vector<VHRing>& rings) {
    if (!art) return kBadParameterErr;

    short type = 0;
    AIErr error = sAIArt->GetArtType(art, &type);
    if (error != kNoErr) return error;

    if (type == kPathArt) {
        std::vector<VHPoint> points;
        error = FlattenPath(art, samplesPerBezier, points);
        if (error == kNoErr && points.size() >= 3)
            rings.push_back(MakeRing(std::move(points)));
        return kNoErr;
    }

    if (type == kCompoundPathArt || type == kGroupArt || type == kPluginArt) {
        AIArtHandle child = nullptr;
        error = sAIArt->GetArtFirstChild(art, &child);
        if (error != kNoErr) return error;
        while (child) {
            // Ignore unsupported child art rather than failing the whole effect;
            // this lets post-effect groups and compound shapes still contribute
            // the usable path outlines.
            AppendGeometryFromArt(child, samplesPerBezier, rings);
            AIArtHandle next = nullptr;
            error = sAIArt->GetArtSibling(child, &next);
            if (error != kNoErr) return error;
            child = next;
        }
        return kNoErr;
    }

    return kNoErr;
}

bool PointInRing(AIReal x, AIReal y, const VHRing& ring) {
    if (ring.points.size() < 3) return false;
    if (x < ring.minX || x > ring.maxX || y < ring.minY || y > ring.maxY) return false;

    bool inside = false;
    size_t j = ring.points.size() - 1;
    for (size_t i = 0; i < ring.points.size(); ++i) {
        const AIReal xi = ring.points[i].x, yi = ring.points[i].y;
        const AIReal xj = ring.points[j].x, yj = ring.points[j].y;
        const AIReal denom = (yj - yi == 0) ? static_cast<AIReal>(0.0000001) : (yj - yi);
        const bool crosses = ((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / denom + xi);
        if (crosses) inside = !inside;
        j = i;
    }
    return inside;
}

AIReal BoundingBoxDistanceSquared(AIReal x, AIReal y, const VHSegment& s) {
    AIReal dx = 0;
    AIReal dy = 0;
    if (x < s.minX) dx = s.minX - x;
    else if (x > s.maxX) dx = x - s.maxX;
    if (y < s.minY) dy = s.minY - y;
    else if (y > s.maxY) dy = y - s.maxY;
    return dx * dx + dy * dy;
}

AIReal PointSegmentDistanceSquared(AIReal px, AIReal py, const VHPoint& a, const VHPoint& b) {
    const AIReal vx = b.x - a.x;
    const AIReal vy = b.y - a.y;
    const AIReal wx = px - a.x;
    const AIReal wy = py - a.y;
    const AIReal vv = vx * vx + vy * vy;
    if (vv <= static_cast<AIReal>(0.0000001)) {
        const AIReal dx = px - a.x;
        const AIReal dy = py - a.y;
        return dx * dx + dy * dy;
    }
    AIReal t = (wx * vx + wy * vy) / vv;
    t = (std::max)(static_cast<AIReal>(0), (std::min)(static_cast<AIReal>(1), t));
    const AIReal cx = a.x + t * vx;
    const AIReal cy = a.y + t * vy;
    const AIReal dx = px - cx;
    const AIReal dy = py - cy;
    return dx * dx + dy * dy;
}

AIColor BlackColor() {
    AIColor color{};
    color.kind = kGrayColor;
    // Illustrator gray: 0 = white, 1 = black.
    color.c.g.gray = kAIRealOne;
    return color;
}

AIPathStyle FallbackMarkStyle() {
    AIPathStyle style;
    style.Init();
    style.fillPaint = true;
    style.strokePaint = false;
    style.fill.color = BlackColor();
    return style;
}

bool StyleHasPaint(const AIPathStyle& style) {
    return (style.fillPaint && style.fill.color.kind != kNoneColor) ||
           (style.strokePaint && style.stroke.color.kind != kNoneColor);
}

bool TryReadPathStyle(AIArtHandle art, AIPathStyle& style) {
    if (!art) return false;
    style.Init();
    AIBoolean hasAdvFill = false;
    if (sAIPathStyle->GetPathStyle(art, &style, &hasAdvFill) == kNoErr && StyleHasPaint(style))
        return true;
    return false;
}

AIPathStyle MakeMarkStyle(const AIPathStyle& sourceStyle, AIReal markSize, AIReal spacing, bool suppressStroke) {
    AIPathStyle mark;
    mark.Init();

    const bool hasFill = sourceStyle.fillPaint && sourceStyle.fill.color.kind != kNoneColor;
    const bool sourceHasStroke = sourceStyle.strokePaint && sourceStyle.stroke.color.kind != kNoneColor;

    // Repeating the source stroke on every dot creates ugly overlaps as marks
    // touch or merge. Marks therefore inherit the source fill directly, and
    // stroke-only artwork uses the source stroke colour as the mark fill.
    const bool paintStroke = false;

    // Dots/marks need a fill to read as halftone marks. If the source only has
    // a stroke, use the stroke colour as the mark fill. This keeps stroke-only
    // artwork visible while still allowing solid-core marks to drop outlines.
    mark.fillPaint = hasFill || sourceHasStroke;
    if (hasFill) {
        mark.fill = sourceStyle.fill;
    } else if (sourceHasStroke) {
        mark.fill.color = sourceStyle.stroke.color;
        mark.fill.overprint = sourceStyle.stroke.overprint;
    } else {
        mark.fill.color = BlackColor();
    }

    if (paintStroke) {
        mark.strokePaint = true;
        mark.stroke = sourceStyle.stroke;
        // Keep the source stroke colour/overprint, but turn it into a controlled
        // mark outline. Source artwork can have thick strokes that look correct
        // on one large path but badly overlap when repeated on halftone dots.
        const AIReal safeSpacing = spacing > static_cast<AIReal>(0.0) ? spacing : markSize;
        const AIReal maxBySize = markSize * static_cast<AIReal>(0.075);
        const AIReal maxBySpacing = safeSpacing * static_cast<AIReal>(0.035);
        const AIReal maxUsefulStroke = (std::max)(static_cast<AIReal>(0.08),
            (std::min)(maxBySize, maxBySpacing));
        if (mark.stroke.width <= static_cast<AIReal>(0.0) || mark.stroke.width > maxUsefulStroke)
            mark.stroke.width = maxUsefulStroke;
    } else {
        mark.strokePaint = false;
    }

    mark.evenodd = sourceStyle.evenodd;
    mark.resolution = sourceStyle.resolution;
    return mark;
}

AIErr SetMarkStyle(AIArtHandle art, const AIPathStyle& sourceStyle, AIReal markSize, AIReal spacing, bool suppressStroke) {
    AIPathStyle style = MakeMarkStyle(sourceStyle, markSize, spacing, suppressStroke);
    return sAIPathStyle->SetPathStyle(art, &style);
}

AIErr CreateClosedPath(AIArtHandle parentGroup, const std::vector<AIPathSegment>& segments,
                       const AIPathStyle& sourceStyle, AIReal markSize, AIReal spacing,
                       bool suppressStroke, AIArtHandle& outArt) {
    AIErr error = sAIArt->NewArt(kPathArt, kPlaceInsideOnTop, parentGroup, &outArt);
    if (error != kNoErr) return error;
    error = sAIPath->SetPathSegmentCount(outArt, static_cast<ai::int16>(segments.size()));
    if (error != kNoErr) return error;
    error = sAIPath->SetPathSegments(outArt, 0, static_cast<ai::int16>(segments.size()), segments.data());
    if (error != kNoErr) return error;
    error = sAIPath->SetPathClosed(outArt, true);
    if (error != kNoErr) return error;
    return SetMarkStyle(outArt, sourceStyle, markSize, spacing, suppressStroke);
}

AIPathSegment StraightPoint(AIReal x, AIReal y) {
    AIPathSegment s{};
    s.p.h = x; s.p.v = y;
    s.in = s.p;
    s.out = s.p;
    s.corner = true;
    return s;
}

VHPoint RotateOffset(AIReal x, AIReal y, AIReal cosA, AIReal sinA, AIReal cx, AIReal cy) {
    return {cx + x * cosA - y * sinA, cy + x * sinA + y * cosA};
}

std::vector<AIPathSegment> PolygonSegments(AIReal cx, AIReal cy, AIReal radius,
                                           int sides, AIReal rotationRadians) {
    std::vector<AIPathSegment> s;
    s.reserve(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const AIReal a = rotationRadians + static_cast<AIReal>(i) * static_cast<AIReal>(2.0 * M_PI / sides);
        s.push_back(StraightPoint(
            cx + radius * static_cast<AIReal>(std::cos(static_cast<double>(a))),
            cy + radius * static_cast<AIReal>(std::sin(static_cast<double>(a)))));
    }
    return s;
}

} // namespace

AIErr BuildVHGeometry(AIArtHandle art, int samplesPerBezier, VHGeometry& out) {
    out.rings.clear();
    out.segments.clear();

    AIErr error = sAIArt->GetArtTransformBounds(art, nullptr, kNoStrokeBounds, &out.bounds);
    if (error != kNoErr) return error;

    error = AppendGeometryFromArt(art, samplesPerBezier, out.rings);
    if (error != kNoErr) return error;

    for (const auto& ring : out.rings) AppendRingSegments(ring, out.segments);
    return out.segments.empty() ? kBadParameterErr : kNoErr;
}

bool VHPointInside(AIReal x, AIReal y, const VHGeometry& geometry) {
    // Even/odd across all component rings handles ordinary compound-path holes
    // without depending on winding direction.
    bool inside = false;
    for (const auto& ring : geometry.rings) {
        if (PointInRing(x, y, ring)) inside = !inside;
    }
    return inside;
}

AIReal VHDistanceToEdge(AIReal x, AIReal y, const VHGeometry& geometry) {
    AIReal minD2 = static_cast<AIReal>(1.0e30);
    for (const auto& segment : geometry.segments) {
        // Cheap AABB lower-bound test avoids the projection math for most distant
        // segments on complex paths.
        if (BoundingBoxDistanceSquared(x, y, segment) >= minD2) continue;
        minD2 = (std::min)(minD2, PointSegmentDistanceSquared(x, y, segment.a, segment.b));
    }
    return static_cast<AIReal>(std::sqrt(static_cast<double>(minD2)));
}

AIReal VHNearestEdgeNormalAngle(AIReal x, AIReal y, const VHGeometry& geometry) {
    AIReal minD2 = static_cast<AIReal>(1.0e30);
    const VHSegment* best = nullptr;
    for (const auto& segment : geometry.segments) {
        if (BoundingBoxDistanceSquared(x, y, segment) >= minD2) continue;
        const AIReal d2 = PointSegmentDistanceSquared(x, y, segment.a, segment.b);
        if (d2 < minD2) {
            minD2 = d2;
            best = &segment;
        }
    }

    if (!best) return static_cast<AIReal>(0);
    const AIReal dx = best->b.x - best->a.x;
    const AIReal dy = best->b.y - best->a.y;
    if (std::fabs(static_cast<double>(dx)) < 0.000001 &&
        std::fabs(static_cast<double>(dy)) < 0.000001)
        return static_cast<AIReal>(0);

    // Angle is the outward/inward normal to the closest flattened path segment.
    // This makes line, square, diamond, triangle, hexagon and star marks sit
    // perpendicular to curved source edges when followCurve is enabled.
    return static_cast<AIReal>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 180.0 / M_PI + 90.0);
}

AIErr VHGetSourceMarkStyle(AIArtHandle art, AIPathStyle& style) {
    style = FallbackMarkStyle();
    if (!art) return kBadParameterErr;

    if (TryReadPathStyle(art, style)) return kNoErr;

    // Compound paths and post-effect input groups can report their style on a
    // child path rather than the container. Walk children until a painted path
    // is found.
    short type = 0;
    if (sAIArt->GetArtType(art, &type) == kNoErr &&
        (type == kCompoundPathArt || type == kGroupArt || type == kPluginArt)) {
        AIArtHandle child = nullptr;
        if (sAIArt->GetArtFirstChild(art, &child) == kNoErr) {
            while (child) {
                short childType = 0;
                if (sAIArt->GetArtType(child, &childType) == kNoErr) {
                    if (childType == kPathArt || childType == kCompoundPathArt) {
                        AIPathStyle childStyle;
                        if (TryReadPathStyle(child, childStyle)) {
                            style = childStyle;
                            return kNoErr;
                        }
                    }
                }
                AIArtHandle next = nullptr;
                if (sAIArt->GetArtSibling(child, &next) != kNoErr) break;
                child = next;
            }
        }
    }

    // Fallback black is intentional, so a missing style never kills the effect.
    return kNoErr;
}

AIErr VHCreateMark(AIArtHandle parentGroup, int shape, AIReal cx, AIReal cy,
                   AIReal size, AIReal spacing, AIReal angleDegrees,
                   const AIPathStyle& sourceStyle, bool suppressStroke) {
    if (size <= static_cast<AIReal>(0.01)) return kNoErr;
    const AIReal r = size / 2;
    AIArtHandle art = nullptr;

    if (shape == 0) {
        // Four-point cubic Bezier circle.
        const AIReal k = static_cast<AIReal>(0.5522847498307936);
        const AIReal h = r * k;
        std::vector<AIPathSegment> s(4);

        s[0] = StraightPoint(cx, cy + r);
        s[0].in.h = cx - h; s[0].in.v = cy + r;
        s[0].out.h = cx + h; s[0].out.v = cy + r;
        s[0].corner = false;

        s[1] = StraightPoint(cx + r, cy);
        s[1].in.h = cx + r; s[1].in.v = cy + h;
        s[1].out.h = cx + r; s[1].out.v = cy - h;
        s[1].corner = false;

        s[2] = StraightPoint(cx, cy - r);
        s[2].in.h = cx + h; s[2].in.v = cy - r;
        s[2].out.h = cx - h; s[2].out.v = cy - r;
        s[2].corner = false;

        s[3] = StraightPoint(cx - r, cy);
        s[3].in.h = cx - r; s[3].in.v = cy - h;
        s[3].out.h = cx - r; s[3].out.v = cy + h;
        s[3].corner = false;

        return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
    }

    const AIReal baseRadians = angleDegrees * static_cast<AIReal>(M_PI / 180.0);

    if (shape == 1 || shape == 2) {
        const AIReal cosA = static_cast<AIReal>(std::cos(static_cast<double>(baseRadians)));
        const AIReal sinA = static_cast<AIReal>(std::sin(static_cast<double>(baseRadians)));
        std::vector<VHPoint> pts;
        if (shape == 1) {
            pts = {RotateOffset(-r, -r, cosA, sinA, cx, cy),
                   RotateOffset( r, -r, cosA, sinA, cx, cy),
                   RotateOffset( r,  r, cosA, sinA, cx, cy),
                   RotateOffset(-r,  r, cosA, sinA, cx, cy)};
        } else {
            pts = {RotateOffset(0,  r, cosA, sinA, cx, cy),
                   RotateOffset(r,  0, cosA, sinA, cx, cy),
                   RotateOffset(0, -r, cosA, sinA, cx, cy),
                   RotateOffset(-r, 0, cosA, sinA, cx, cy)};
        }
        std::vector<AIPathSegment> s;
        s.reserve(pts.size());
        for (const auto& pt : pts) s.push_back(StraightPoint(pt.x, pt.y));
        return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
    }

    if (shape == 3) {
        auto s = PolygonSegments(cx, cy, r, 3, static_cast<AIReal>(M_PI / 2.0) + baseRadians);
        return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
    }

    if (shape == 4) {
        auto s = PolygonSegments(cx, cy, r, 6, static_cast<AIReal>(M_PI / 6.0) + baseRadians);
        return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
    }

    if (shape == 5) {
        std::vector<AIPathSegment> s;
        s.reserve(10);
        const AIReal inner = r * static_cast<AIReal>(0.45);
        for (int i = 0; i < 10; ++i) {
            const AIReal rr = (i % 2 == 0) ? r : inner;
            const AIReal a = static_cast<AIReal>(M_PI / 2.0) + baseRadians +
                static_cast<AIReal>(i) * static_cast<AIReal>(M_PI / 5.0);
            s.push_back(StraightPoint(
                cx + rr * static_cast<AIReal>(std::cos(static_cast<double>(a))),
                cy + rr * static_cast<AIReal>(std::sin(static_cast<double>(a)))));
        }
        return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
    }

    // Line halftone: overlapping filled dashes create a continuous-looking line
    // screen while retaining variable thickness from mark to mark.
    const AIReal halfLength = (std::max)(spacing * static_cast<AIReal>(0.62), r);
    const AIReal halfThickness = r;
    const AIReal radians = angleDegrees * static_cast<AIReal>(M_PI / 180.0);
    const AIReal cosA = static_cast<AIReal>(std::cos(static_cast<double>(radians)));
    const AIReal sinA = static_cast<AIReal>(std::sin(static_cast<double>(radians)));
    const VHPoint p0 = RotateOffset(-halfLength, -halfThickness, cosA, sinA, cx, cy);
    const VHPoint p1 = RotateOffset( halfLength, -halfThickness, cosA, sinA, cx, cy);
    const VHPoint p2 = RotateOffset( halfLength,  halfThickness, cosA, sinA, cx, cy);
    const VHPoint p3 = RotateOffset(-halfLength,  halfThickness, cosA, sinA, cx, cy);
    std::vector<AIPathSegment> s = {
        StraightPoint(p0.x, p0.y), StraightPoint(p1.x, p1.y),
        StraightPoint(p2.x, p2.y), StraightPoint(p3.x, p3.y)
    };
    return CreateClosedPath(parentGroup, s, sourceStyle, size, spacing, suppressStroke, art);
}
