#include "P5GeneratorPlugin.h"

#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cmath>

// Global OpenFX host and suites
OfxHost* gHost = nullptr;
OfxPropertySuiteV1* gPropSuite = nullptr;
OfxImageEffectSuiteV1* gImageEffectSuite = nullptr;
OfxParameterSuiteV1* gParamSuite = nullptr;
OfxMemorySuiteV1* gMemorySuite = nullptr;

// Default Starter Sketch Code
static const char* kDefaultSketchCode = R"JS(// Default Deterministic Starter Sketch for DaVinci Resolve
// Paste or write any standard p5.js sketch here!

function setup() {
  createCanvas(width, height);
  colorMode(HSB, 360, 100, 100, 1.0);
  noStroke();
}

function draw() {
  // Semi-transparent background for subtle motion trails
  background(0, 0, 10, 0.95);

  let cx = width / 2;
  let cy = height / 2;
  let t = millis() * 0.0015; // Deterministic time in seconds

  let rings = 8;
  let pointsPerRing = 48;

  for (let r = 1; r <= rings; r++) {
    let radius = (r / rings) * (min(width, height) * 0.38);
    let hueBase = (r * 35 + t * 40) % 360;

    for (let i = 0; i < pointsPerRing; i++) {
      let angle = (TWO_PI / pointsPerRing) * i + t * (r % 2 === 0 ? 0.6 : -0.6);
      let offset = sin(angle * 4 + t * 3 + r) * 25;
      let finalRadius = radius + offset;

      let x = cx + cos(angle) * finalRadius;
      let y = cy + sin(angle) * finalRadius;

      let dotSize = 4 + sin(angle * 2 + t * 2) * 3;
      let hue = (hueBase + i * 2) % 360;

      fill(hue, 85, 95, 0.85);
      circle(x, y, dotSize);
    }
  }
}
)JS";

static const char* kDefaultCdnUrls = R"TXT(# Enter external CDN libraries or scripts here (one URL per line)
# Example: https://cdnjs.cloudflare.com/ajax/libs/simplex-noise/2.4.0/simplex-noise.min.js
# Example: https://cdnjs.cloudflare.com/ajax/libs/matter-js/0.19.0/matter.min.js
)TXT";

// Helper: Discover bundle's Contents/Resources path dynamically
static std::string getBundleResourcesPath() {
    Dl_info info;
    if (dladdr((const void*)getBundleResourcesPath, &info) && info.dli_fname) {
        std::string dylibPath = info.dli_fname;
        size_t pos = dylibPath.rfind("/Contents/MacOS/");
        if (pos != std::string::npos) {
            return dylibPath.substr(0, pos) + "/Contents/Resources";
        }
    }
    return "./resources";
}

// ============================================================================
// P5PluginInstance Implementation
// ============================================================================

P5PluginInstance::P5PluginInstance(OfxImageEffectHandle handle)
    : m_effectHandle(handle) {
    resourcesPath = getBundleResourcesPath();
    gImageEffectSuite->getPropertySet(m_effectHandle, &effectProps);

    bridge = std::make_unique<P5WebKitBridge>();
}

P5PluginInstance::~P5PluginInstance() {
    if (bridge) {
        bridge->shutdown();
        bridge.reset();
    }
}

bool P5PluginInstance::initializeBridge(int width, int height) {
    if (!bridge) {
        bridge = std::make_unique<P5WebKitBridge>();
    }
    return bridge->init(width, height, resourcesPath);
}

void P5PluginInstance::reloadSketchFromParams() {
    if (!gParamSuite || !bridge) return;

    // Retrieve code parameter
    char* codeStr = nullptr;
    if (paramCode) {
        gParamSuite->paramGetValue(paramCode, &codeStr);
        if (codeStr) {
            currentCode = codeStr;
        }
    }

    // Retrieve CDN parameter
    char* cdnStr = nullptr;
    if (paramCdn) {
        gParamSuite->paramGetValue(paramCdn, &cdnStr);
        if (cdnStr) {
            currentCdn = cdnStr;
        }
    }

    // Retrieve simulation mode
    int simMode = 0;
    if (paramSimMode) {
        gParamSuite->paramGetValue(paramSimMode, &simMode);
        currentSimMode = simMode;
    }

    bridge->updateSketch(currentCode, currentCdn, currentSimMode);

    // Update status display
    if (paramStatus) {
        std::string status = bridge->getLastLogMessage();
        if (status.empty()) status = "Sketch recompiled.";
        gParamSuite->paramSetValue(paramStatus, status.c_str());
    }
}

OfxStatus P5PluginInstance::render(OfxTime time, OfxRectI renderWindow, OfxPropertySetHandle outArgs) {
    if (!clipOutput || !gImageEffectSuite) {
        return kOfxStatFailed;
    }

    int width = renderWindow.x2 - renderWindow.x1;
    int height = renderWindow.y2 - renderWindow.y1;

    if (width <= 0 || height <= 0) {
        return kOfxStatOK;
    }

    // Get Output Image buffer from Resolve
    OfxPropertySetHandle outputImg = nullptr;
    OfxStatus stat = gImageEffectSuite->clipGetImage(clipOutput, time, nullptr, &outputImg);
    if (stat != kOfxStatOK || !outputImg) {
        return stat;
    }

    // Query destination image properties
    void* dstData = nullptr;
    int dstRowBytes = 0;
    char* depthStr = nullptr;

    gPropSuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstData);
    gPropSuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropSuite->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &depthStr);

    bool isFloat = (depthStr && strcmp(depthStr, kOfxBitDepthFloat) == 0);

    // Query project frame rate
    double fps = 24.0;
    gPropSuite->propGetDouble(effectProps, kOfxImageEffectPropFrameRate, 0, &fps);
    if (fps <= 0.0) fps = 24.0;

    double timelineTime = time / fps;

    // Render frame through WebKit bridge to IOSurface, then blit to Resolve buffer
    bool ok = bridge->renderFrame(time, timelineTime, fps, width, height, dstData, dstRowBytes, isFloat);

    gImageEffectSuite->clipReleaseImage(outputImg);

    return ok ? kOfxStatOK : kOfxStatFailed;
}

// ============================================================================
// OpenFX Plugin Actions
// ============================================================================

static OfxStatus onLoadAction(void) {
    return kOfxStatOK;
}

static OfxStatus onUnloadAction(void) {
    return kOfxStatOK;
}

static OfxStatus onDescribeAction(OfxImageEffectHandle descriptor) {
    OfxPropertySetHandle effectProps = nullptr;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // Metadata
    gPropSuite->propSetString(effectProps, kOfxPropLabel, 0, "P5.js Canvas Generator");
    gPropSuite->propSetString(effectProps, kOfxPropShortLabel, 0, "P5 Generator");
    gPropSuite->propSetString(effectProps, kOfxPropLongLabel, 0, "P5.js Canvas Generator");
    gPropSuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Generators");
    gPropSuite->propSetString(effectProps, kOfxPropPluginDescription, 0,
                              "Native p5.js offscreen generator with deterministic timeline synchronization and external CDN library support.");

    // Supported Contexts: Generator, Filter, General
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextGenerator);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 2, kOfxImageEffectContextGeneral);

    // Supported Pixel Depths: 8-bit Byte & 32-bit Float
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthFloat);

    // Rendering features
    gPropSuite->propSetInt(effectProps, kOfxImageEffectPropSupportsMultiResolution, 0, 1);
    gPropSuite->propSetInt(effectProps, kOfxImageEffectPluginPropSingleInstance, 0, 0);
    gPropSuite->propSetInt(effectProps, kOfxImageEffectPluginPropHostFrameThreading, 0, 0);

    return kOfxStatOK;
}

static OfxStatus onDescribeInContextAction(OfxImageEffectHandle descriptor, OfxPropertySetHandle inArgs) {
    char* context = nullptr;
    gPropSuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

    // If instantiated as a Filter or General context, define an optional source clip
    if (context && (strcmp(context, kOfxImageEffectContextFilter) == 0 || strcmp(context, kOfxImageEffectContextGeneral) == 0)) {
        OfxPropertySetHandle srcClipProps = nullptr;
        gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectSimpleSourceClipName, &srcClipProps);
        gPropSuite->propSetString(srcClipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
        gPropSuite->propSetString(srcClipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldBoth);
        gPropSuite->propSetInt(srcClipProps, kOfxImageClipPropOptional, 0, 1);
    }

    // Define Output Clip
    OfxPropertySetHandle outClipProps = nullptr;
    gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectOutputClipName, &outClipProps);
    gPropSuite->propSetString(outClipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(outClipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldBoth);

    // Parameter Set
    OfxParamSetHandle paramSet = nullptr;
    gImageEffectSuite->getParamSet(descriptor, &paramSet);

    // 1. Sketch Code Parameter (Multiline Text)
    OfxPropertySetHandle paramCodeProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "p5_code", &paramCodeProps);
    gPropSuite->propSetString(paramCodeProps, kOfxPropLabel, 0, "p5.js Sketch Code");
    gPropSuite->propSetString(paramCodeProps, kOfxParamPropStringMode, 0, kOfxParamStringIsMultiLine);
    gPropSuite->propSetString(paramCodeProps, kOfxParamPropDefault, 0, kDefaultSketchCode);
    gPropSuite->propSetInt(paramCodeProps, kOfxParamPropAnimates, 0, 0);

    // 2. External CDN Libraries Parameter (Multiline Text)
    OfxPropertySetHandle paramCdnProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "cdn_libraries", &paramCdnProps);
    gPropSuite->propSetString(paramCdnProps, kOfxPropLabel, 0, "External CDN Libraries");
    gPropSuite->propSetString(paramCdnProps, kOfxParamPropStringMode, 0, kOfxParamStringIsMultiLine);
    gPropSuite->propSetString(paramCdnProps, kOfxParamPropDefault, 0, kDefaultCdnUrls);
    gPropSuite->propSetInt(paramCdnProps, kOfxParamPropAnimates, 0, 0);

    // 3. Reload / Compile Push Button
    OfxPropertySetHandle paramReloadProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypePushButton, "reload_btn", &paramReloadProps);
    gPropSuite->propSetString(paramReloadProps, kOfxPropLabel, 0, "Reload / Recompile");
    gPropSuite->propSetString(paramReloadProps, kOfxParamPropHint, 0, "Recompile sketch and reload CDN libraries");

    // 4. Scrubbing / Simulation Mode Choice
    OfxPropertySetHandle paramSimProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "sim_mode", &paramSimProps);
    gPropSuite->propSetString(paramSimProps, kOfxPropLabel, 0, "Scrubbing Mode");
    gPropSuite->propSetString(paramSimProps, kOfxParamPropChoiceOption, 0, "Deterministic Time (Stateless)");
    gPropSuite->propSetString(paramSimProps, kOfxParamPropChoiceOption, 1, "Cumulative Step Simulation");
    gPropSuite->propSetInt(paramSimProps, kOfxParamPropDefault, 0, 0);

    // 5. Status / Console Log Display
    OfxPropertySetHandle paramStatusProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "status_display", &paramStatusProps);
    gPropSuite->propSetString(paramStatusProps, kOfxPropLabel, 0, "Engine Status");
    gPropSuite->propSetString(paramStatusProps, kOfxParamPropStringMode, 0, kOfxParamStringIsSingleLine);
    gPropSuite->propSetString(paramStatusProps, kOfxParamPropDefault, 0, "Ready");
    gPropSuite->propSetInt(paramStatusProps, kOfxParamPropAnimates, 0, 0);

    return kOfxStatOK;
}

static OfxStatus onCreateInstanceAction(OfxImageEffectHandle instance) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    auto* plugin = new P5PluginInstance(instance);

    // Obtain clips
    gImageEffectSuite->clipGetHandle(instance, kOfxImageEffectOutputClipName, &plugin->clipOutput, nullptr);

    // Obtain parameters
    OfxParamSetHandle paramSet = nullptr;
    gImageEffectSuite->getParamSet(instance, &paramSet);

    gParamSuite->paramGetHandle(paramSet, "p5_code", &plugin->paramCode, nullptr);
    gParamSuite->paramGetHandle(paramSet, "cdn_libraries", &plugin->paramCdn, nullptr);
    gParamSuite->paramGetHandle(paramSet, "reload_btn", &plugin->paramReload, nullptr);
    gParamSuite->paramGetHandle(paramSet, "sim_mode", &plugin->paramSimMode, nullptr);
    gParamSuite->paramGetHandle(paramSet, "status_display", &plugin->paramStatus, nullptr);

    // Initialize WebKit bridge with default resolution 1920x1080
    plugin->initializeBridge(1920, 1080);
    plugin->reloadSketchFromParams();

    // Store C++ instance in OFX property
    gPropSuite->propSetPointer(instanceProps, kOfxPropInstanceData, 0, plugin);

    return kOfxStatOK;
}

static OfxStatus onDestroyInstanceAction(OfxImageEffectHandle instance) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (data) {
        auto* plugin = static_cast<P5PluginInstance*>(data);
        delete plugin;
        gPropSuite->propSetPointer(instanceProps, kOfxPropInstanceData, 0, nullptr);
    }

    return kOfxStatOK;
}

static OfxStatus onInstanceChangedAction(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (!data) return kOfxStatFailed;

    auto* plugin = static_cast<P5PluginInstance*>(data);

    char* paramName = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &paramName);

    if (paramName) {
        if (strcmp(paramName, "reload_btn") == 0 ||
            strcmp(paramName, "p5_code") == 0 ||
            strcmp(paramName, "cdn_libraries") == 0 ||
            strcmp(paramName, "sim_mode") == 0) {
            plugin->reloadSketchFromParams();
        }
    }

    return kOfxStatOK;
}

static OfxStatus onGetTimeDomainAction(OfxImageEffectHandle instance, OfxPropertySetHandle outArgs) {
    // Generators have continuous time domain
    gPropSuite->propSetDouble(outArgs, kOfxImageEffectPropFrameRange, 0, -1e6);
    gPropSuite->propSetDouble(outArgs, kOfxImageEffectPropFrameRange, 1, 1e6);
    return kOfxStatOK;
}

static OfxStatus onRenderAction(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (!data) return kOfxStatFailed;

    auto* plugin = static_cast<P5PluginInstance*>(data);

    OfxTime time = 0;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    OfxRectI renderWindow;
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 0, &renderWindow.x1);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 1, &renderWindow.y1);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 2, &renderWindow.x2);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 3, &renderWindow.y2);

    return plugin->render(time, renderWindow, outArgs);
}

// ============================================================================
// OpenFX Plugin Main Dispatcher
// ============================================================================

static OfxStatus pluginMainEntry(const char* action,
                                 const void* handle,
                                 OfxPropertySetHandle inArgs,
                                 OfxPropertySetHandle outArgs) {
    if (!action) return kOfxStatErrBadHandle;

    auto effectHandle = (OfxImageEffectHandle)handle;

    if (strcmp(action, kOfxActionLoad) == 0) {
        return onLoadAction();
    } else if (strcmp(action, kOfxActionUnload) == 0) {
        return onUnloadAction();
    } else if (strcmp(action, kOfxActionDescribe) == 0) {
        return onDescribeAction(effectHandle);
    } else if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
        return onDescribeInContextAction(effectHandle, inArgs);
    } else if (strcmp(action, kOfxActionCreateInstance) == 0) {
        return onCreateInstanceAction(effectHandle);
    } else if (strcmp(action, kOfxActionDestroyInstance) == 0) {
        return onDestroyInstanceAction(effectHandle);
    } else if (strcmp(action, kOfxActionInstanceChanged) == 0) {
        return onInstanceChangedAction(effectHandle, inArgs);
    } else if (strcmp(action, kOfxImageEffectActionGetTimeDomain) == 0) {
        return onGetTimeDomainAction(effectHandle, outArgs);
    } else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
        return onRenderAction(effectHandle, inArgs, outArgs);
    }

    return kOfxStatReplyDefault;
}

static void pluginSetHost(OfxHost* host) {
    gHost = host;
    if (gHost) {
        gPropSuite = (OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
        gImageEffectSuite = (OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
        gParamSuite = (OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
        gMemorySuite = (OfxMemorySuiteV1*)gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1);
    }
}

// OpenFX Plugin Descriptor Structure
static OfxPlugin gPluginDefinition = {
    kOfxImageEffectPluginApi,      // pluginApi: "OfxImageEffectPluginAPI"
    1,                             // apiVersion
    "com.antigravity.p5generator", // pluginIdentifier
    1,                             // pluginVersionMajor
    0,                             // pluginVersionMinor
    pluginSetHost,                 // setHost
    pluginMainEntry                // mainEntry
};

// ============================================================================
// OpenFX Exported Entry Points
// ============================================================================

extern "C" {

OfxExport int OfxGetNumberOfPlugins(void) {
    return 1;
}

OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    if (nth == 0) {
        return &gPluginDefinition;
    }
    return nullptr;
}

}
