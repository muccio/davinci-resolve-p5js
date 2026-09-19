#import <Cocoa/Cocoa.h>
#include "P5WebKitBridge.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

// Test sketch with distinct color components
static const char* kTestSketch = R"JS(
function setup() {
  createCanvas(width, height);
  noStroke();
}

function draw() {
  console.log("DRAW: frameCount=" + frameCount + " resolveFrame=" + window.resolveFrame);
  if (frameCount === 0) {
    background(200, 100, 50); // Red=200, Green=100, Blue=50 at frame 0
  } else {
    background(10, 20, 220);  // Red=10, Green=20, Blue=220 at frame 10
  }
}
)JS";

int main(int argc, char* argv[]) {
    @autoreleasepool {
        std::cout << "==================================================" << std::endl;
        std::cout << " Running Standalone P5 WebKit <-> IOSurface Test  " << std::endl;
        std::cout << "==================================================" << std::endl;

        // Initialize Cocoa Application environment for headless WebKit
        [NSApplication sharedApplication];

        // Discover resources path
        NSFileManager *fm = [NSFileManager defaultManager];
        NSString *resourcesPath = @"./resources";

        NSArray *candidatePaths = @[
            @"./resources",
            @"../resources",
            @"./P5Generator.ofx.bundle/Contents/Resources",
            @"../P5Generator.ofx.bundle/Contents/Resources"
        ];

        for (NSString *cand in candidatePaths) {
            NSString *indexPath = [cand stringByAppendingPathComponent:@"index.html"];
            if ([fm fileExistsAtPath:indexPath]) {
                resourcesPath = cand;
                break;
            }
        }

        std::cout << "[Test] Using resources path: " << [resourcesPath UTF8String] << std::endl;

        std::cout << "[Test] Initializing P5WebKitBridge..." << std::endl;
        const int testWidth = 640;
        const int testHeight = 480;

        P5WebKitBridge bridge;
        bool ok = bridge.init(testWidth, testHeight, [resourcesPath UTF8String]);
        if (!ok) {
            std::cerr << "[Test Error] Failed to initialize bridge!" << std::endl;
            return 1;
        }

        // Pump runloop until WebKit environment is initialized
        std::cout << "[Test] Waiting for WebKit environment to load..." << std::endl;
        NSDate *giveUpDate = [NSDate dateWithTimeIntervalSinceNow:5.0];
        while (!bridge.isReady() && [giveUpDate timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }

        std::cout << "[Test] WebKit ready state: " << (bridge.isReady() ? "YES" : "NO") << std::endl;

        // Update sketch code
        std::cout << "[Test] Uploading deterministic test sketch..." << std::endl;
        bridge.updateSketch(kTestSketch, "", 0);

        // Wait a brief moment for compilation
        giveUpDate = [NSDate dateWithTimeIntervalSinceNow:1.0];
        while ([giveUpDate timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }

        // Output buffer allocation (640x480 RGBA 8-bit)
        std::vector<uint8_t> pixelBuffer(testWidth * testHeight * 4, 0);
        int rowBytes = testWidth * 4;

        // --- TEST 1: Render Frame 0 (Expect RED) ---
        std::cout << "[Test 1] Rendering Frame 0 (Expected: RED)..." << std::endl;
        bool f0Rendered = bridge.renderFrame(0.0, 0.0, 24.0, testWidth, testHeight, pixelBuffer.data(), rowBytes, false);
        if (!f0Rendered) {
            std::cerr << "[Test Error] Frame 0 render failed!" << std::endl;
            return 1;
        }

        // Read center pixel
        int centerX = testWidth / 2;
        int centerY = testHeight / 2;
        int centerIdx = (centerY * rowBytes) + (centerX * 4);
        int r0 = pixelBuffer[centerIdx + 0];
        int g0 = pixelBuffer[centerIdx + 1];
        int b0 = pixelBuffer[centerIdx + 2];
        int a0 = pixelBuffer[centerIdx + 3];

        std::cout << "[Test 1 Result] Frame 0 Center Pixel: RGBA("
                  << r0 << ", " << g0 << ", " << b0 << ", " << a0 << ")" << std::endl;

        assert(r0 == 200 && g0 == 100 && b0 == 50 && a0 == 255 && "Frame 0 center pixel must match exact RGBA(200, 100, 50, 255)!");

        // --- TEST 2: Render Frame 10 ---
        std::cout << "[Test 2] Rendering Frame 10..." << std::endl;
        bool f10Rendered = bridge.renderFrame(10.0, 10.0 / 24.0, 24.0, testWidth, testHeight, pixelBuffer.data(), rowBytes, false);
        if (!f10Rendered) {
            std::cerr << "[Test Error] Frame 10 render failed!" << std::endl;
            return 1;
        }

        int r10 = pixelBuffer[centerIdx + 0];
        int g10 = pixelBuffer[centerIdx + 1];
        int b10 = pixelBuffer[centerIdx + 2];
        int a10 = pixelBuffer[centerIdx + 3];

        std::cout << "[Test 2 Result] Frame 10 Center Pixel: RGBA("
                  << r10 << ", " << g10 << ", " << b10 << ", " << a10 << ")" << std::endl;

        assert(r10 == 10 && g10 == 20 && b10 == 220 && a10 == 255 && "Frame 10 center pixel must match exact RGBA(10, 20, 220, 255)!");

        assert(b10 > 200 && r10 < 50 && "Frame 10 should be predominantly BLUE!");

        std::cout << "==================================================" << std::endl;
        std::cout << " SUCCESS! All WebKit & IOSurface tests passed!    " << std::endl;
        std::cout << " Deterministic frame clock verified accurately.   " << std::endl;
        std::cout << "==================================================" << std::endl;

        bridge.shutdown();
    }
    return 0;
}
