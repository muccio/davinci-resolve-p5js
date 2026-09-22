#pragma once

#include "openfx/ofxCore.h"
#include "openfx/ofxImageEffect.h"
#include "openfx/ofxParam.h"
#include "openfx/ofxProperty.h"

#include "P5WebKitBridge.h"
#include "P5AudioEngine.h"

#include <string>
#include <memory>

/**
 * P5FilterPluginInstance
 * 
 * Manages the lifetime, parameters, source clip input,
 * audio analysis, and render state for an OpenFX Filter effect instance.
 */
class P5FilterPluginInstance {
public:
    P5FilterPluginInstance(OfxImageEffectHandle handle);
    ~P5FilterPluginInstance();

    // Parameter handles
    OfxParamHandle paramCode = nullptr;
    OfxParamHandle paramCdn = nullptr;
    OfxParamHandle paramAudioFile = nullptr;
    OfxParamHandle paramAudioMode = nullptr;
    OfxParamHandle paramReload = nullptr;
    OfxParamHandle paramSimMode = nullptr;
    OfxParamHandle paramStatus = nullptr;

    // Clip handles
    OfxImageClipHandle clipSource = nullptr;
    OfxImageClipHandle clipOutput = nullptr;

    // Instance property set
    OfxPropertySetHandle effectProps = nullptr;

    // WebKit Bridge & Audio Engine
    std::unique_ptr<P5WebKitBridge> bridge;
    std::unique_ptr<P5AudioEngine> audioEngine;

    // Cached values
    std::string currentCode;
    std::string currentCdn;
    std::string currentAudioFile;
    int currentAudioMode = 0; // 0 = File (Deterministic), 1 = Live WebAudio, 2 = Disabled
    int currentSimMode = 0;
    std::string resourcesPath;

    // Methods
    bool initializeBridge(int width, int height);
    void reloadSketchFromParams();
    void updateAudioFileFromParams();
    OfxStatus render(OfxTime time, OfxRectI renderWindow, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);

private:
    OfxImageEffectHandle m_effectHandle = nullptr;
};

// OpenFX Plugin Descriptor for the Filter Plugin
extern OfxPlugin gFilterPluginDefinition;
