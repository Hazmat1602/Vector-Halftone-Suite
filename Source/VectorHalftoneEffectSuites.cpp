#include "IllustratorSDK.h"
#include "VectorHalftoneEffectSuites.h"

extern "C" {
SPBlocksSuite* sSPBlocks = nullptr;
AIUnicodeStringSuite* sAIUnicodeString = nullptr;
AILiveEffectSuite* sAILiveEffect = nullptr;
AIArtSuite* sAIArt = nullptr;
AIPathSuite* sAIPath = nullptr;
AIPathStyleSuite* sAIPathStyle = nullptr;
AIDictionarySuite* sAIDictionary = nullptr;
AIUndoSuite* sAIUndo = nullptr;
AIGroupSuite* sAIGroup = nullptr;
AIUIThemeSuite* sVectorHalftoneUITheme = nullptr;
AIPathfinderSuite* sVectorHalftonePathfinder = nullptr;
AIArtSetSuite* sVectorHalftoneArtSet = nullptr;
AIRasterizeSuite* sVectorHalftoneRasterize = nullptr;
AIRasterSuite* sVectorHalftoneRaster = nullptr;
AIBlendStyleSuite* sVectorHalftoneBlendStyle = nullptr;
AIMaskSuite* sVectorHalftoneMask = nullptr;
AIDocumentSuite* sVectorHalftoneDocument = nullptr;
}

ImportSuite gImportSuites[] = {
    kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
    kAIUnicodeStringSuite, kAIUnicodeStringSuiteVersion, &sAIUnicodeString,
    kAILiveEffectSuite, kAILiveEffectSuiteVersion, &sAILiveEffect,
    kAIArtSuite, kAIArtSuiteVersion, &sAIArt,
    kAIPathSuite, kAIPathSuiteVersion, &sAIPath,
    kAIPathStyleSuite, kAIPathStyleSuiteVersion, &sAIPathStyle,
    kAIDictionarySuite, kAIDictionarySuiteVersion, &sAIDictionary,
    kAIUndoSuite, kAIUndoSuiteVersion, &sAIUndo,
    kAIGroupSuite, kAIGroupSuiteVersion, &sAIGroup,
    kAIUIThemeSuite, kAIUIThemeVersion, &sVectorHalftoneUITheme,
    kAIPathfinderSuite, kAIPathfinderVersion, &sVectorHalftonePathfinder,
    kAIArtSetSuite, kAIArtSetSuiteVersion, &sVectorHalftoneArtSet,
    kAIRasterizeSuite, kAIRasterizeSuiteVersion, &sVectorHalftoneRasterize,
    kAIRasterSuite, kAIRasterSuiteVersion, &sVectorHalftoneRaster,
    kAIBlendStyleSuite, kAIBlendStyleSuiteVersion, &sVectorHalftoneBlendStyle,
    kAIMaskSuite, kAIMaskSuiteVersion, &sVectorHalftoneMask,
    kAIDocumentSuite, kAIDocumentSuiteVersion, &sVectorHalftoneDocument,
    nullptr, 0, nullptr
};
