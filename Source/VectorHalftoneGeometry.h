#pragma once

#include "IllustratorSDK.h"
#include "VectorHalftoneParams.h"
#include <vector>

struct VHPoint {
    AIReal x = 0;
    AIReal y = 0;
};

struct VHSegment {
    VHPoint a;
    VHPoint b;
    AIReal minX = 0;
    AIReal minY = 0;
    AIReal maxX = 0;
    AIReal maxY = 0;
};

struct VHRing {
    std::vector<VHPoint> points;
    AIReal minX = 0;
    AIReal minY = 0;
    AIReal maxX = 0;
    AIReal maxY = 0;
};

struct VHGeometry {
    std::vector<VHRing> rings;
    std::vector<VHSegment> segments;
    AIRealRect bounds{};
};

AIErr BuildVHGeometry(AIArtHandle art, int samplesPerBezier, VHGeometry& out);
bool VHPointInside(AIReal x, AIReal y, const VHGeometry& geometry);
AIReal VHDistanceToEdge(AIReal x, AIReal y, const VHGeometry& geometry);
AIReal VHNearestEdgeNormalAngle(AIReal x, AIReal y, const VHGeometry& geometry);
AIErr VHGetSourceMarkStyle(AIArtHandle art, AIPathStyle& style);
AIErr VHCreateMark(AIArtHandle parentGroup, int shape, AIReal cx, AIReal cy,
                   AIReal size, AIReal spacing, AIReal angleDegrees,
                   const AIPathStyle& sourceStyle, bool suppressStroke = false);
