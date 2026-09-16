#pragma once

#include "IllustratorSDK.h"
#include "Suites.hpp"
#include <AILiveEffect.h>
#include <AIGroup.h>
#include <AIUITheme.h>

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
// Adobe's shared samplecode/common/source/Suites.cpp owns this one.
extern "C" AIAppContextSuite* sAIAppContext;
