#include "P5FilterPlugin.h"
#include "P5GeneratorPlugin.h"

#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// Default Starter Sketch Code for Filter Mode
static const char* kDefaultFilterSketchCode = R"JS(// Default Starter Sketch for P5.js Video Effect (FX Mode)
// ------------------------------------------------------------------
// 'videoIn' gives you direct access to the timeline video clip!
// 'audioIn' gives you audio amplitude, waveforms, and FFT spectrum!

function setup() {
  createCanvas(width, height);
  colorMode(HSB, 360, 100, 100, 1.0);
  noStroke();
}

function draw() {
  // 1. Draw the underlying video clip frame
  if (typeof videoIn !== 'undefined') {
    image(videoIn, 0, 0, width, height);
  } else {
    background(0);
  }

  // 2. Query audio metrics (0.0 to 1.0)
  let level = (typeof audioIn !== 'undefined') ? audioIn.getLevel() : 0.1;
  let bass = (typeof audioIn !== 'undefined') ? audioIn.bass : 0.0;
  let t = millis() * 0.0015;

  // 3. Audio-reactive particle grid sampling video colors
  let gridStep = 32;
  let maxRadius = (gridStep * 0.6) * (1.0 + level * 2.0);

  if (typeof videoIn !== 'undefined' && videoIn.width > 0) {
    let cols = Math.floor(width / gridStep);
    let rows = Math.floor(height / gridStep);

    for (let i = 0; i < cols; i += 2) {
      for (let j = 0; j < rows; j += 2) {
        let x = (i + 0.5) * gridStep;
        let y = (j + 0.5) * gridStep;

        // Sample original video pixel color
        let col = videoIn.get(x, y);
        let brightnessVal = (col[0] + col[1] + col[2]) / (3 * 255.0);

        let waveOffset = sin(t * 3.0 + (x + y) * 0.01) * 0.5 + 0.5;
        let r = maxRadius * brightnessVal * (0.4 + waveOffset * 0.6 + bass * 0.8);

        if (r > 2.0) {
          fill((col[0] + t * 40) % 360, 80, 100, 0.75);
          circle(x, y, r);
        }
      }
    }
  }

  // 4. Audio Waveform Overlay at the bottom
  if (typeof audioIn !== 'undefined' && audioIn.waveform) {
    let wave = audioIn.waveform();
    if (wave && wave.length > 0) {
      stroke(180, 80, 100, 0.8);
      strokeWeight(2);
      noFill();
      beginShape();
      let stepX = width / wave.length;
      for (let k = 0; k < wave.length; k++) {
        let wy = height - 40 + wave[k] * 60.0;
        vertex(k * stepX, wy);
      }
      endShape();
      noStroke();
    }
  }
}
)JS";

// ASCII Art Full Color Preset
static const char* kAsciiColorSketchCode = R"JS(// ==================================================================
//  P5.js Video Effect: ASCII Art (Full Color Matrix)
// ==================================================================
//  Renders incoming video clip as dynamic colored typography
//  with audio-reactive contrast and density modulation.

// --- CONFIGURATION ---
const COLOR_MODE = true;      // true = Full RGB Color, false = Monochrome B&W
const CHAR_SIZE = 12;         // Character grid step in pixels (8 to 20)
const DENSITY = " .:-=+*#%@"; // Characters from dark to bright
const CONTRAST = 1.25;        // Contrast enhancement factor
const AUDIO_REACTIVE = true;  // React to audio beats and amplitude

function setup() {
  createCanvas(width, height);
  textFont('monospace');
  textSize(CHAR_SIZE);
  textAlign(CENTER, CENTER);
  textStyle(BOLD);
  noStroke();
}

function draw() {
  background(0); // Deep black background

  if (typeof videoIn === 'undefined' || videoIn.width === 0) {
    return;
  }

  // Audio metrics
  let audioLevel = (typeof audioIn !== 'undefined') ? audioIn.getLevel() : 0.0;
  let bass = (typeof audioIn !== 'undefined') ? audioIn.bass : 0.0;

  let effectiveSize = CHAR_SIZE;
  let dynamicContrast = CONTRAST + (AUDIO_REACTIVE ? bass * 0.8 : 0);

  // Load raw pixels from videoIn for real-time 60fps sampling
  videoIn.loadPixels();
  let vw = videoIn.width;
  let vh = videoIn.height;
  let pix = videoIn.pixels;

  let cols = floor(width / effectiveSize);
  let rows = floor(height / effectiveSize);
  let stepX = width / cols;
  let stepY = height / rows;

  let dLen = DENSITY.length - 1;

  for (let j = 0; j < rows; j++) {
    let y = (j + 0.5) * stepY;
    let py = floor((y / height) * vh);

    for (let i = 0; i < cols; i++) {
      let x = (i + 0.5) * stepX;
      let px = floor((x / width) * vw);

      let idx = (py * vw + px) * 4;
      let r = pix[idx];
      let g = pix[idx + 1];
      let b = pix[idx + 2];

      // Perceptual luminance calculation (ITU-R BT.601)
      let lum = (r * 0.299 + g * 0.587 + b * 0.114);

      // Contrast adjustment
      lum = constrain(((lum - 128) * dynamicContrast) + 128, 0, 255);

      let charIdx = floor(map(lum, 0, 255, 0, dLen));
      let ch = DENSITY.charAt(charIdx);

      if (ch !== ' ') {
        if (COLOR_MODE) {
          fill(r, g, b);
        } else {
          fill(lum);
        }
        text(ch, x, y);
      }
    }
  }

  // Audio level indicator on the bottom border
  if (AUDIO_REACTIVE && audioLevel > 0.02) {
    fill(0, 255, 180, 180);
    rect(0, height - 3, width * audioLevel, 3);
  }
}
)JS";

// ASCII Art Monochrome (B&W) Preset
static const char* kAsciiBwSketchCode = R"JS(// ==================================================================
//  P5.js Video Effect: ASCII Art (Monochrome Black & White)
// ==================================================================
//  Renders incoming video clip as high-contrast typographic art
//  with customizable phosphor/monochrome tint and audio dynamics.

// --- CONFIGURATION ---
const COLOR_MODE = false;     // false = Black & White, true = Full Color
const CHAR_SIZE = 12;         // Character grid step in pixels (8 to 20)
const DENSITY = " .:-=+*#%@"; // Characters from dark to bright
const CONTRAST = 1.35;        // Contrast enhancement factor
const INVERT = false;         // true = Dark text on White, false = White on Black
const AUDIO_REACTIVE = true;  // React to audio beats and amplitude

function setup() {
  createCanvas(width, height);
  textFont('monospace');
  textSize(CHAR_SIZE);
  textAlign(CENTER, CENTER);
  textStyle(BOLD);
  noStroke();
}

function draw() {
  background(INVERT ? 255 : 0);

  if (typeof videoIn === 'undefined' || videoIn.width === 0) {
    return;
  }

  // Audio metrics
  let audioLevel = (typeof audioIn !== 'undefined') ? audioIn.getLevel() : 0.0;
  let bass = (typeof audioIn !== 'undefined') ? audioIn.bass : 0.0;

  let dynamicContrast = CONTRAST + (AUDIO_REACTIVE ? bass * 0.9 : 0);

  // Load raw pixels from videoIn for real-time 60fps sampling
  videoIn.loadPixels();
  let vw = videoIn.width;
  let vh = videoIn.height;
  let pix = videoIn.pixels;

  let cols = floor(width / CHAR_SIZE);
  let rows = floor(height / CHAR_SIZE);
  let stepX = width / cols;
  let stepY = height / rows;

  let dLen = DENSITY.length - 1;

  for (let j = 0; j < rows; j++) {
    let y = (j + 0.5) * stepY;
    let py = floor((y / height) * vh);

    for (let i = 0; i < cols; i++) {
      let x = (i + 0.5) * stepX;
      let px = floor((x / width) * vw);

      let idx = (py * vw + px) * 4;
      let r = pix[idx];
      let g = pix[idx + 1];
      let b = pix[idx + 2];

      // Perceptual luminance calculation (ITU-R BT.601)
      let lum = (r * 0.299 + g * 0.587 + b * 0.114);

      // Contrast enhancement
      lum = constrain(((lum - 128) * dynamicContrast) + 128, 0, 255);

      let charIdx = floor(map(lum, 0, 255, 0, dLen));
      if (INVERT) {
        charIdx = dLen - charIdx;
      }
      let ch = DENSITY.charAt(charIdx);

      if (ch !== ' ') {
        if (INVERT) {
          fill(255 - lum);
        } else {
          fill(lum);
        }
        text(ch, x, y);
      }
    }
  }

  // Audio waveform overlay line on bottom border
  if (AUDIO_REACTIVE && audioLevel > 0.02) {
    fill(INVERT ? 0 : 255, 180);
    rect(0, height - 3, width * audioLevel, 3);
  }
}
)JS";


static const char* kDefaultFilterCdnUrls = R"TXT(# Enter external CDN libraries or scripts here (one URL per line)
# Example: https://cdnjs.cloudflare.com/ajax/libs/simplex-noise/2.4.0/simplex-noise.min.js
)TXT";

// Helper: Discover bundle's Contents/Resources path dynamically
static std::string getFilterBundleResourcesPath() {
    Dl_info info;
    if (dladdr((const void*)getFilterBundleResourcesPath, &info) && info.dli_fname) {
        std::string dylibPath = info.dli_fname;
        size_t pos = dylibPath.rfind("/Contents/MacOS/");
        if (pos != std::string::npos) {
            return dylibPath.substr(0, pos) + "/Contents/Resources";
        }
    }
    return "./resources";
}

// ============================================================================
// P5FilterPluginInstance Implementation
// ============================================================================

P5FilterPluginInstance::P5FilterPluginInstance(OfxImageEffectHandle handle)
    : m_effectHandle(handle) {
    resourcesPath = getFilterBundleResourcesPath();
    if (gImageEffectSuite) {
        gImageEffectSuite->getPropertySet(m_effectHandle, &effectProps);
    }

    bridge = std::make_unique<P5WebKitBridge>();
    audioEngine = std::make_unique<P5AudioEngine>();
}

P5FilterPluginInstance::~P5FilterPluginInstance() {
    if (bridge) {
        bridge->shutdown();
        bridge.reset();
    }
}

bool P5FilterPluginInstance::initializeBridge(int width, int height) {
    if (!bridge) {
        bridge = std::make_unique<P5WebKitBridge>();
    }
    return bridge->init(width, height, resourcesPath);
}

void P5FilterPluginInstance::updateAudioFileFromParams() {
    if (!gParamSuite || !audioEngine) return;

    char* audioPathStr = nullptr;
    if (paramAudioFile) {
        gParamSuite->paramGetValue(paramAudioFile, &audioPathStr);
        if (audioPathStr) {
            currentAudioFile = audioPathStr;
        }
    }

    int audioMode = 0;
    if (paramAudioMode) {
        gParamSuite->paramGetValue(paramAudioMode, &audioMode);
        currentAudioMode = audioMode;
    }

    if (currentAudioMode == 0 && !currentAudioFile.empty()) {
        bool ok = audioEngine->loadFile(currentAudioFile);
        if (paramStatus) {
            std::string status = ok ? ("Loaded audio: " + currentAudioFile) : ("Failed to load audio: " + currentAudioFile);
            gParamSuite->paramSetValue(paramStatus, status.c_str());
        }
    } else if (currentAudioMode != 0) {
        audioEngine->closeFile();
    }
}

void P5FilterPluginInstance::reloadSketchFromParams() {
    if (!gParamSuite || !bridge) return;

    char* codeStr = nullptr;
    if (paramCode) {
        gParamSuite->paramGetValue(paramCode, &codeStr);
        if (codeStr) {
            currentCode = codeStr;
        }
    }

    char* cdnStr = nullptr;
    if (paramCdn) {
        gParamSuite->paramGetValue(paramCdn, &cdnStr);
        if (cdnStr) {
            currentCdn = cdnStr;
        }
    }

    int simMode = 0;
    if (paramSimMode) {
        gParamSuite->paramGetValue(paramSimMode, &simMode);
        currentSimMode = simMode;
    }

    updateAudioFileFromParams();

    bridge->updateSketch(currentCode, currentCdn, currentSimMode);

    if (paramStatus) {
        std::string status = bridge->getLastLogMessage();
        if (status.empty()) status = "Filter sketch recompiled.";
        gParamSuite->paramSetValue(paramStatus, status.c_str());
    }
}

OfxStatus P5FilterPluginInstance::render(OfxTime time, OfxRectI renderWindow, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    if (!clipOutput || !gImageEffectSuite) {
        return kOfxStatFailed;
    }

    int outWidth = renderWindow.x2 - renderWindow.x1;
    int outHeight = renderWindow.y2 - renderWindow.y1;

    if (outWidth <= 0 || outHeight <= 0) {
        return kOfxStatOK;
    }

    // Get Output Image buffer from Resolve
    OfxPropertySetHandle outputImg = nullptr;
    OfxStatus stat = gImageEffectSuite->clipGetImage(clipOutput, time, nullptr, &outputImg);
    if (stat != kOfxStatOK || !outputImg) {
        return stat;
    }

    void* dstData = nullptr;
    int dstRowBytes = 0;
    char* dstDepthStr = nullptr;
    gPropSuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstData);
    gPropSuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropSuite->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &dstDepthStr);
    bool isDstFloat = (dstDepthStr && strcmp(dstDepthStr, kOfxBitDepthFloat) == 0);

    // Get Source Video Image buffer from Resolve
    OfxPropertySetHandle srcImg = nullptr;
    const void* srcData = nullptr;
    int srcRowBytes = 0;
    int srcWidth = outWidth;
    int srcHeight = outHeight;
    bool isSrcFloat = false;

    if (clipSource) {
        OfxStatus srcStat = gImageEffectSuite->clipGetImage(clipSource, time, nullptr, &srcImg);
        if (srcStat == kOfxStatOK && srcImg) {
            char* srcDepthStr = nullptr;
            gPropSuite->propGetPointer(srcImg, kOfxImagePropData, 0, const_cast<void**>(&srcData));
            gPropSuite->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
            gPropSuite->propGetString(srcImg, kOfxImageEffectPropPixelDepth, 0, &srcDepthStr);
            isSrcFloat = (srcDepthStr && strcmp(srcDepthStr, kOfxBitDepthFloat) == 0);

            OfxRectI srcBounds;
            if (gPropSuite->propGetIntN(srcImg, kOfxImagePropBounds, 4, &srcBounds.x1) == kOfxStatOK) {
                srcWidth = srcBounds.x2 - srcBounds.x1;
                srcHeight = srcBounds.y2 - srcBounds.y1;
            }
        }
    }

    // Query frame rate
    double fps = 24.0;
    if (effectProps) {
        gPropSuite->propGetDouble(effectProps, kOfxImageEffectPropFrameRate, 0, &fps);
    }
    if (fps <= 0.0) fps = 24.0;

    double timelineTime = time / fps;

    // Query audio metrics if file loaded
    P5AudioMetrics audioMetrics;
    if (audioEngine && currentAudioMode == 0) {
        audioMetrics = audioEngine->getMetricsAtTime(timelineTime);
    }

    // Render filter frame via WebKit bridge
    bool ok = bridge->renderFilterFrame(time, timelineTime, fps,
                                        srcData, srcRowBytes, srcWidth, srcHeight, isSrcFloat,
                                        audioMetrics,
                                        outWidth, outHeight,
                                        dstData, dstRowBytes, isDstFloat);

    if (srcImg) {
        gImageEffectSuite->clipReleaseImage(srcImg);
    }
    gImageEffectSuite->clipReleaseImage(outputImg);

    return ok ? kOfxStatOK : kOfxStatFailed;
}

// ============================================================================
// OpenFX Plugin Actions for Filter Mode
// ============================================================================

static OfxStatus onFilterLoadAction(void) {
    return kOfxStatOK;
}

static OfxStatus onFilterUnloadAction(void) {
    return kOfxStatOK;
}

static OfxStatus onFilterDescribeAction(OfxImageEffectHandle descriptor) {
    OfxPropertySetHandle effectProps = nullptr;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    gPropSuite->propSetString(effectProps, kOfxPropLabel, 0, "P5.js Canvas Effect");
    gPropSuite->propSetString(effectProps, kOfxPropShortLabel, 0, "P5 Effect");
    gPropSuite->propSetString(effectProps, kOfxPropLongLabel, 0, "P5.js Canvas Effect");
    gPropSuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Filters");
    gPropSuite->propSetString(effectProps, kOfxPropPluginDescription, 0,
                              "Creative coding video effect powered by p5.js with videoIn source frame processing and audioIn sound synchronization.");

    // Context: Filter only
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

    // Supported Pixel Depths: 8-bit Byte & 32-bit Float
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthFloat);

    gPropSuite->propSetInt(effectProps, kOfxImageEffectPropSupportsMultiResolution, 0, 1);
    gPropSuite->propSetInt(effectProps, kOfxImageEffectPluginPropSingleInstance, 0, 0);
    gPropSuite->propSetInt(effectProps, kOfxImageEffectPluginPropHostFrameThreading, 0, 0);

    return kOfxStatOK;
}

static OfxStatus onFilterDescribeInContextAction(OfxImageEffectHandle descriptor, OfxPropertySetHandle inArgs) {
    // 1. Source Clip (Incoming video from timeline clip)
    OfxPropertySetHandle srcClipProps = nullptr;
    gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectSimpleSourceClipName, &srcClipProps);
    gPropSuite->propSetString(srcClipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(srcClipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldBoth);
    gPropSuite->propSetInt(srcClipProps, kOfxImageClipPropOptional, 0, 0); // Mandatory for a filter

    // 2. Output Clip
    OfxPropertySetHandle outClipProps = nullptr;
    gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectOutputClipName, &outClipProps);
    gPropSuite->propSetString(outClipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(outClipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldBoth);

    // 3. Parameters
    OfxParamSetHandle paramSet = nullptr;
    gImageEffectSuite->getParamSet(descriptor, &paramSet);

    // preset_choice
    OfxPropertySetHandle paramPresetProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "preset_choice", &paramPresetProps);
    gPropSuite->propSetString(paramPresetProps, kOfxPropLabel, 0, "Preset Template");
    gPropSuite->propSetString(paramPresetProps, kOfxParamPropChoiceOption, 0, "Custom Sketch");
    gPropSuite->propSetString(paramPresetProps, kOfxParamPropChoiceOption, 1, "ASCII Art - Full Color");
    gPropSuite->propSetString(paramPresetProps, kOfxParamPropChoiceOption, 2, "ASCII Art - Monochrome (B&W)");
    gPropSuite->propSetString(paramPresetProps, kOfxParamPropChoiceOption, 3, "Particle Video Sampler");
    gPropSuite->propSetInt(paramPresetProps, kOfxParamPropDefault, 0, 1);

    // p5_code
    OfxPropertySetHandle paramCodeProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "p5_code", &paramCodeProps);
    gPropSuite->propSetString(paramCodeProps, kOfxPropLabel, 0, "p5.js Sketch Code");
    gPropSuite->propSetString(paramCodeProps, kOfxParamPropStringMode, 0, kOfxParamStringIsMultiLine);
    gPropSuite->propSetString(paramCodeProps, kOfxParamPropDefault, 0, kAsciiColorSketchCode);
    gPropSuite->propSetInt(paramCodeProps, kOfxParamPropAnimates, 0, 0);

    // cdn_libraries
    OfxPropertySetHandle paramCdnProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "cdn_libraries", &paramCdnProps);
    gPropSuite->propSetString(paramCdnProps, kOfxPropLabel, 0, "External CDN Libraries");
    gPropSuite->propSetString(paramCdnProps, kOfxParamPropStringMode, 0, kOfxParamStringIsMultiLine);
    gPropSuite->propSetString(paramCdnProps, kOfxParamPropDefault, 0, kDefaultFilterCdnUrls);
    gPropSuite->propSetInt(paramCdnProps, kOfxParamPropAnimates, 0, 0);

    // audio_file
    OfxPropertySetHandle paramAudioFileProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "audio_file", &paramAudioFileProps);
    gPropSuite->propSetString(paramAudioFileProps, kOfxPropLabel, 0, "Audio Track / Media File");
    gPropSuite->propSetString(paramAudioFileProps, kOfxParamPropStringMode, 0, kOfxParamStringIsSingleLine);
    gPropSuite->propSetString(paramAudioFileProps, kOfxParamPropDefault, 0, "");
    gPropSuite->propSetString(paramAudioFileProps, kOfxParamPropHint, 0, "Path to audio (.wav, .mp3, .m4a) or video file (.mp4, .mov)");
    gPropSuite->propSetInt(paramAudioFileProps, kOfxParamPropAnimates, 0, 0);

    // audio_mode
    OfxPropertySetHandle paramAudioModeProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "audio_mode", &paramAudioModeProps);
    gPropSuite->propSetString(paramAudioModeProps, kOfxPropLabel, 0, "Audio Input Mode");
    gPropSuite->propSetString(paramAudioModeProps, kOfxParamPropChoiceOption, 0, "Audio File (Deterministic & Accurate)");
    gPropSuite->propSetString(paramAudioModeProps, kOfxParamPropChoiceOption, 1, "Live WebAudio / Microphone");
    gPropSuite->propSetString(paramAudioModeProps, kOfxParamPropChoiceOption, 2, "Disabled");
    gPropSuite->propSetInt(paramAudioModeProps, kOfxParamPropDefault, 0, 0);

    // reload_btn
    OfxPropertySetHandle paramReloadProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypePushButton, "reload_btn", &paramReloadProps);
    gPropSuite->propSetString(paramReloadProps, kOfxPropLabel, 0, "Reload / Recompile");
    gPropSuite->propSetString(paramReloadProps, kOfxParamPropHint, 0, "Recompile sketch and reload audio/video assets");

    // sim_mode
    OfxPropertySetHandle paramSimProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "sim_mode", &paramSimProps);
    gPropSuite->propSetString(paramSimProps, kOfxPropLabel, 0, "Scrubbing Mode");
    gPropSuite->propSetString(paramSimProps, kOfxParamPropChoiceOption, 0, "Deterministic Time (Stateless)");
    gPropSuite->propSetString(paramSimProps, kOfxParamPropChoiceOption, 1, "Cumulative Step Simulation");
    gPropSuite->propSetInt(paramSimProps, kOfxParamPropDefault, 0, 0);

    // status_display
    OfxPropertySetHandle paramStatusProps = nullptr;
    gParamSuite->paramDefine(paramSet, kOfxParamTypeString, "status_display", &paramStatusProps);
    gPropSuite->propSetString(paramStatusProps, kOfxPropLabel, 0, "Engine Status");
    gPropSuite->propSetString(paramStatusProps, kOfxParamPropStringMode, 0, kOfxParamStringIsSingleLine);
    gPropSuite->propSetString(paramStatusProps, kOfxParamPropDefault, 0, "Ready");
    gPropSuite->propSetInt(paramStatusProps, kOfxParamPropAnimates, 0, 0);

    return kOfxStatOK;
}

static OfxStatus onFilterCreateInstanceAction(OfxImageEffectHandle instance) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    auto* plugin = new P5FilterPluginInstance(instance);

    // Obtain clips
    gImageEffectSuite->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName, &plugin->clipSource, nullptr);
    gImageEffectSuite->clipGetHandle(instance, kOfxImageEffectOutputClipName, &plugin->clipOutput, nullptr);

    // Obtain parameters
    OfxParamSetHandle paramSet = nullptr;
    gImageEffectSuite->getParamSet(instance, &paramSet);

    gParamSuite->paramGetHandle(paramSet, "preset_choice", &plugin->paramPreset, nullptr);
    gParamSuite->paramGetHandle(paramSet, "p5_code", &plugin->paramCode, nullptr);
    gParamSuite->paramGetHandle(paramSet, "cdn_libraries", &plugin->paramCdn, nullptr);
    gParamSuite->paramGetHandle(paramSet, "audio_file", &plugin->paramAudioFile, nullptr);
    gParamSuite->paramGetHandle(paramSet, "audio_mode", &plugin->paramAudioMode, nullptr);
    gParamSuite->paramGetHandle(paramSet, "reload_btn", &plugin->paramReload, nullptr);
    gParamSuite->paramGetHandle(paramSet, "sim_mode", &plugin->paramSimMode, nullptr);
    gParamSuite->paramGetHandle(paramSet, "status_display", &plugin->paramStatus, nullptr);

    plugin->initializeBridge(1920, 1080);
    plugin->reloadSketchFromParams();

    gPropSuite->propSetPointer(instanceProps, kOfxPropInstanceData, 0, plugin);

    return kOfxStatOK;
}

static OfxStatus onFilterDestroyInstanceAction(OfxImageEffectHandle instance) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (data) {
        auto* plugin = static_cast<P5FilterPluginInstance*>(data);
        delete plugin;
        gPropSuite->propSetPointer(instanceProps, kOfxPropInstanceData, 0, nullptr);
    }

    return kOfxStatOK;
}

static OfxStatus onFilterInstanceChangedAction(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (!data) return kOfxStatFailed;

    auto* plugin = static_cast<P5FilterPluginInstance*>(data);

    char* paramName = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &paramName);

    if (paramName) {
        if (strcmp(paramName, "preset_choice") == 0) {
            int chosen = 0;
            if (plugin->paramPreset) {
                gParamSuite->paramGetValue(plugin->paramPreset, &chosen);
                if (chosen == 1) {
                    gParamSuite->paramSetValue(plugin->paramCode, kAsciiColorSketchCode);
                } else if (chosen == 2) {
                    gParamSuite->paramSetValue(plugin->paramCode, kAsciiBwSketchCode);
                } else if (chosen == 3) {
                    gParamSuite->paramSetValue(plugin->paramCode, kDefaultFilterSketchCode);
                }
            }
            plugin->reloadSketchFromParams();
        } else if (strcmp(paramName, "p5_code") == 0) {
            if (plugin->paramPreset) {
                int cur = 0;
                gParamSuite->paramGetValue(plugin->paramPreset, &cur);
                if (cur != 0) {
                    gParamSuite->paramSetValue(plugin->paramPreset, 0);
                }
            }
            plugin->reloadSketchFromParams();
        } else if (strcmp(paramName, "reload_btn") == 0 ||
                   strcmp(paramName, "cdn_libraries") == 0 ||
                   strcmp(paramName, "sim_mode") == 0) {
            plugin->reloadSketchFromParams();
        } else if (strcmp(paramName, "audio_file") == 0 ||
                   strcmp(paramName, "audio_mode") == 0) {
            plugin->updateAudioFileFromParams();
        }
    }

    return kOfxStatOK;
}

static OfxStatus onFilterRenderAction(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    OfxPropertySetHandle instanceProps = nullptr;
    gImageEffectSuite->getPropertySet(instance, &instanceProps);

    void* data = nullptr;
    gPropSuite->propGetPointer(instanceProps, kOfxPropInstanceData, 0, &data);
    if (!data) return kOfxStatFailed;

    auto* plugin = static_cast<P5FilterPluginInstance*>(data);

    OfxTime time = 0;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    OfxRectI renderWindow;
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 0, &renderWindow.x1);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 1, &renderWindow.y1);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 2, &renderWindow.x2);
    gPropSuite->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, 3, &renderWindow.y2);

    return plugin->render(time, renderWindow, inArgs, outArgs);
}

// Filter OpenFX Main Entry Point
static OfxStatus filterPluginMainEntry(const char* action,
                                       const void* handle,
                                       OfxPropertySetHandle inArgs,
                                       OfxPropertySetHandle outArgs) {
    if (!action) return kOfxStatErrBadHandle;

    auto effectHandle = (OfxImageEffectHandle)handle;

    if (strcmp(action, kOfxActionLoad) == 0) {
        return onFilterLoadAction();
    } else if (strcmp(action, kOfxActionUnload) == 0) {
        return onFilterUnloadAction();
    } else if (strcmp(action, kOfxActionDescribe) == 0) {
        return onFilterDescribeAction(effectHandle);
    } else if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
        return onFilterDescribeInContextAction(effectHandle, inArgs);
    } else if (strcmp(action, kOfxActionCreateInstance) == 0) {
        return onFilterCreateInstanceAction(effectHandle);
    } else if (strcmp(action, kOfxActionDestroyInstance) == 0) {
        return onFilterDestroyInstanceAction(effectHandle);
    } else if (strcmp(action, kOfxActionInstanceChanged) == 0) {
        return onFilterInstanceChangedAction(effectHandle, inArgs);
    } else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
        return onFilterRenderAction(effectHandle, inArgs, outArgs);
    }

    return kOfxStatReplyDefault;
}

static void filterPluginSetHost(OfxHost* host) {
    if (host && !gHost) {
        gHost = host;
        gPropSuite = (OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
        gImageEffectSuite = (OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
        gParamSuite = (OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
        gMemorySuite = (OfxMemorySuiteV1*)gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1);
    }
}

OfxPlugin gFilterPluginDefinition = {
    kOfxImageEffectPluginApi,   // pluginApi
    1,                          // apiVersion
    "com.antigravity.p5filter", // pluginIdentifier
    1,                          // pluginVersionMajor
    0,                          // pluginVersionMinor
    filterPluginSetHost,        // setHost
    filterPluginMainEntry       // mainEntry
};
