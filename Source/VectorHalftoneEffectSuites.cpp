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
    nullptr, 0, nullptr
};
