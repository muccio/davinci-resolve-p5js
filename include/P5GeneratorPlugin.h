#pragma once

#include "openfx/ofxCore.h"
#include "openfx/ofxImageEffect.h"
#include "openfx/ofxParam.h"
#include "openfx/ofxProperty.h"

#include "P5WebKitBridge.h"

#include <string>
#include <memory>

/**
 * P5PluginInstance
 * 
 * Manages the lifetime, parameters, and render state
 * for a single instance of the P5 generator on a timeline clip.
 */
class P5PluginInstance {
public:
    P5PluginInstance(OfxImageEffectHandle handle);
    ~P5PluginInstance();

    // Parameter handles
    OfxParamHandle paramCode = nullptr;
    OfxParamHandle paramCdn = nullptr;
    OfxParamHandle paramReload = nullptr;
    OfxParamHandle paramSimMode = nullptr;
    OfxParamHandle paramStatus = nullptr;

    // Output clip handle
    OfxImageClipHandle clipOutput = nullptr;

    // Instance property set
    OfxPropertySetHandle effectProps = nullptr;

    // WebKit Bridge
    std::unique_ptr<P5WebKitBridge> bridge;

    // Cached values
    std::string currentCode;
    std::string currentCdn;
    int currentSimMode = 0;
    std::string resourcesPath;

    // Methods
    bool initializeBridge(int width, int height);
    void reloadSketchFromParams();
    OfxStatus render(OfxTime time, OfxRectI renderWindow, OfxPropertySetHandle outArgs);

private:
    OfxImageEffectHandle m_effectHandle = nullptr;
};

// OpenFX Global host suites
extern OfxHost* gHost;
extern OfxPropertySuiteV1* gPropSuite;
extern OfxImageEffectSuiteV1* gImageEffectSuite;
extern OfxParameterSuiteV1* gParamSuite;
extern OfxMemorySuiteV1* gMemorySuite;
