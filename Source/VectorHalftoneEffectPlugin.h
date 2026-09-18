#pragma once

#include "Plugin.hpp"
#include "VectorHalftoneEffectID.h"
#include "VectorHalftoneParams.h"
#include "VectorHalftoneExtraParams.h"

Plugin* AllocatePlugin(SPPluginRef pluginRef);
void FixupReload(Plugin* plugin);

class VectorHalftoneEffectPlugin : public Plugin {
public:
    explicit VectorHalftoneEffectPlugin(SPPluginRef pluginRef);
    virtual ~VectorHalftoneEffectPlugin();

    FIXUP_VTABLE_EX(VectorHalftoneEffectPlugin, Plugin);

    ASErr StartupPlugin(SPInterfaceMessage* message);
    ASErr ShutdownPlugin(SPInterfaceMessage* message);
    ASErr GoLiveEffect(AILiveEffectGoMessage* message);
    ASErr EditLiveEffectParameters(AILiveEffectEditParamMessage* message);

private:
    AILiveEffectHandle fLiveEffect;
    AILiveEffectHandle fGradientEffect; // hidden legacy effect
    AILiveEffectHandle fSimpleColourHalftoneEffect;
    AILiveEffectHandle fPhotoshopHalftoneEffect;

    ASErr AddLiveEffect(SPInterfaceMessage* message);
    ASErr AddExtraLiveEffects(SPInterfaceMessage* message);
    ASErr ReadParameters(const AILiveEffectParameters& dict, VectorHalftoneParams& p) const;
    ASErr WriteParameters(const AILiveEffectParameters& dict, const VectorHalftoneParams& p) const;
    ASErr BuildHalftone(AIArtHandle inputArt, AIArtHandle& outputArt, const VectorHalftoneParams& p) const;

    ASErr ReadGradientParameters(const AILiveEffectParameters& dict, VectorHalftoneGradientParams& p) const;
    ASErr WriteGradientParameters(const AILiveEffectParameters& dict, const VectorHalftoneGradientParams& p) const;
    ASErr ReadSimpleColourParameters(const AILiveEffectParameters& dict, VectorSimpleColourHalftoneParams& p) const;
    ASErr WriteSimpleColourParameters(const AILiveEffectParameters& dict, const VectorSimpleColourHalftoneParams& p) const;
    ASErr ReadPhotoshopParameters(const AILiveEffectParameters& dict, VectorPhotoshopHalftoneParams& p) const;
    ASErr WritePhotoshopParameters(const AILiveEffectParameters& dict, const VectorPhotoshopHalftoneParams& p) const;
    ASErr EditGradientParameters(AILiveEffectEditParamMessage* message);
    ASErr EditSimpleColourParameters(AILiveEffectEditParamMessage* message);
    ASErr EditPhotoshopParameters(AILiveEffectEditParamMessage* message);
    ASErr BuildGradientHalftone(AIArtHandle inputArt, AIArtHandle& outputArt, const VectorHalftoneGradientParams& p) const;
    ASErr BuildSimpleColourHalftone(AIArtHandle inputArt, AIArtHandle& outputArt, const VectorSimpleColourHalftoneParams& p) const;
    ASErr BuildPhotoshopHalftone(AIArtHandle inputArt, AIArtHandle& outputArt, const VectorPhotoshopHalftoneParams& p) const;
};
