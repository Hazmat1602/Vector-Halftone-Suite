#pragma once

#include "IllustratorSDK.h"
#include "Suites.hpp"
#include <AILiveEffect.h>
#include <AIGroup.h>
#include <AIUITheme.h>
#include <AIPathfinder.h>
#include <AIArtSet.h>
#include <AIRasterize.h>
#include <AIRaster.h>
#include <AIMask.h>
#include <AIDocument.h>

extern "C" SPBlocksSuite* sSPBlocks;
extern "C" AIUnicodeStringSuite* sAIUnicodeString;
extern "C" AILiveEffectSuite* sAILiveEffect;
extern "C" AIArtSuite* sAIArt;
extern "C" AIPathSuite* sAIPath;
extern "C" AIPathStyleSuite* sAIPathStyle;
extern "C" AIDictionarySuite* sAIDictionary;
extern "C" AIUndoSuite* sAIUndo;
extern "C" AIGroupSuite* sAIGroup;
extern "C" AIUIThemeSuite* sVectorHalftoneUITheme;
extern "C" AIPathfinderSuite* sVectorHalftonePathfinder;
extern "C" AIArtSetSuite* sVectorHalftoneArtSet;
extern "C" AIRasterizeSuite* sVectorHalftoneRasterize;
extern "C" AIRasterSuite* sVectorHalftoneRaster;
extern "C" AIBlendStyleSuite* sVectorHalftoneBlendStyle;
extern "C" AIMaskSuite* sVectorHalftoneMask;
extern "C" AIDocumentSuite* sVectorHalftoneDocument;
// Adobe's shared samplecode/common/source/Suites.cpp owns this one.
extern "C" AIAppContextSuite* sAIAppContext;
