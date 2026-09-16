#pragma once

#include "Plugin.hpp"
#include "VectorHalftoneEffectID.h"
#include "VectorHalftoneParams.h"

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

    ASErr AddLiveEffect(SPInterfaceMessage* message);
    ASErr ReadParameters(const AILiveEffectParameters& dict, VectorHalftoneParams& p) const;
    ASErr WriteParameters(const AILiveEffectParameters& dict, const VectorHalftoneParams& p) const;
    ASErr BuildHalftone(AIArtHandle inputArt, AIArtHandle& outputArt, const VectorHalftoneParams& p) const;
};
