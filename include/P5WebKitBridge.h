#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declaration of internal Objective-C implementation wrapper
class P5WebKitBridgeImpl;

/**
 * P5WebKitBridge
 * 
 * High-performance offscreen WebKit host on macOS.
 * Renders p5.js sketches deterministically frame-by-frame and transfers
 * pixel data directly via macOS kernel-level IOSurface shared memory.
 */
class P5WebKitBridge {
public:
    P5WebKitBridge();
    ~P5WebKitBridge();

    // Prevent copying
    P5WebKitBridge(const P5WebKitBridge&) = delete;
    P5WebKitBridge& operator=(const P5WebKitBridge&) = delete;

    /**
     * Initialize the offscreen WebKit instance, hidden window context, and script controller.
     * @param initialWidth Initial canvas width in pixels
     * @param initialHeight Initial canvas height in pixels
     * @param resourcesPath Path to directory containing index.html, p5.min.js, p5_shim.js
     * @return true on success
     */
    bool init(int initialWidth, int initialHeight, const std::string& resourcesPath);

    /**
     * Update the active p5 sketch code and external CDN libraries.
     * @param sketchCode The user's JavaScript code (containing setup, draw, etc.)
     * @param cdnLibraries Newline-separated list of CDN URLs (or local script paths)
     * @param simMode 0 = Stateless/Deterministic time (default), 1 = Cumulative simulation
     * @return true if successfully dispatched
     */
    bool updateSketch(const std::string& sketchCode, const std::string& cdnLibraries, int simMode);

    /**
     * Render a deterministic frame at the requested timeline time.
     * Synchronizes WebKit, invokes window.renderResolveFrame(...), captures to IOSurface,
     * and blits to the destination buffer with vertical coordinate alignment.
     * 
     * @param frame Current frame index (e.g. 0, 1, 2...)
     * @param time Current timeline time in seconds (e.g. frame / fps)
     * @param fps Timeline framerate (e.g. 24.0, 30.0, 60.0)
     * @param width Output buffer width in pixels
     * @param height Output buffer height in pixels
     * @param dstBuffer Pointer to destination pixel memory (OpenFX output clip buffer)
     * @param dstRowBytes Row stride (pitch) in bytes of the destination buffer
     * @param isFloatFormat false for 8-bit RGBA (uint8_t), true for 32-bit float RGBA
     * @return true if frame was rendered and transferred successfully
     */
    bool renderFrame(double frame, double time, double fps,
                     int width, int height,
                     void* dstBuffer, int dstRowBytes,
                     bool isFloatFormat);

    /**
     * Retrieve latest console messages / JavaScript error logs.
     */
    std::string getLastLogMessage() const;

    /**
     * Check if WebKit runtime and p5.js have reported ready status.
     */
    bool isReady() const;

    /**
     * Clean up WebKit view, window, and IOSurface resources.
     */
    void shutdown();

private:
    std::unique_ptr<P5WebKitBridgeImpl> m_impl;
};
