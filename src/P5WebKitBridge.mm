#import "P5WebKitBridge.h"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <IOSurface/IOSurface.h>
#import <CoreGraphics/CoreGraphics.h>
#include <mutex>
#include <iostream>
#include <sstream>

// Objective-C interface for message handling and navigation delegate
@interface P5WebScriptHandler : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@property (nonatomic, assign) bool isReady;
@property (nonatomic, strong) NSString *lastLog;
@property (nonatomic, strong) NSString *lastError;
@end

@implementation P5WebScriptHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _isReady = false;
        _lastLog = @"Initializing...";
        _lastError = @"";
    }
    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message {
    if ([message.name isEqualToString:@"p5Bridge"]) {
        NSDictionary *body = message.body;
        if ([body isKindOfClass:[NSDictionary class]]) {
            NSString *type = body[@"type"];
            NSString *payload = body[@"payload"];
            if ([type isEqualToString:@"ready"]) {
                self.isReady = true;
                self.lastLog = @"p5.js environment ready.";
            } else if ([type isEqualToString:@"log"]) {
                self.lastLog = payload ? payload : @"";
            } else if ([type isEqualToString:@"error"]) {
                self.lastError = payload ? payload : @"Unknown error";
                self.lastLog = [NSString stringWithFormat:@"[JS ERROR] %@", self.lastError];
                NSLog(@"[P5WebKitBridge JS ERROR] %@", self.lastError);
            }
        }
    }
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSLog(@"[P5WebKitBridge] Headless HTML navigation completed.");
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    NSLog(@"[P5WebKitBridge] Navigation failed: %@", error.localizedDescription);
    self.lastError = error.localizedDescription;
}

@end

// Internal implementation class
class P5WebKitBridgeImpl {
public:
    NSWindow *offscreenWindow = nil;
    WKWebView *webView = nil;
    P5WebScriptHandler *scriptHandler = nil;
    IOSurfaceRef ioSurface = nil;

    int currentWidth = 0;
    int currentHeight = 0;
    std::string resourcesDir;
    mutable std::mutex logMutex;

    P5WebKitBridgeImpl() = default;

    ~P5WebKitBridgeImpl() {
        cleanup();
    }

    void cleanup() {
        if (ioSurface) {
            CFRelease(ioSurface);
            ioSurface = nil;
        }
        if (webView) {
            [webView.configuration.userContentController removeScriptMessageHandlerForName:@"p5Bridge"];
            [webView removeFromSuperview];
            webView = nil;
        }
        if (offscreenWindow) {
            [offscreenWindow close];
            offscreenWindow = nil;
        }
        scriptHandler = nil;
    }

    bool ensureIOSurface(int width, int height) {
        if (ioSurface && currentWidth == width && currentHeight == height) {
            return true;
        }

        if (ioSurface) {
            CFRelease(ioSurface);
            ioSurface = nil;
        }

        currentWidth = width;
        currentHeight = height;

        // Allocate macOS IOSurface with 32-bit RGBA pixel format
        NSDictionary *surfaceProps = @{
            (id)kIOSurfaceWidth: @(width),
            (id)kIOSurfaceHeight: @(height),
            (id)kIOSurfaceBytesPerElement: @4,
            (id)kIOSurfacePixelFormat: @((uint32_t)'BGRA'),
            (id)kIOSurfaceAllocSize: @(width * height * 4)
        };

        ioSurface = IOSurfaceCreate((CFDictionaryRef)surfaceProps);
        if (!ioSurface) {
            NSLog(@"[P5WebKitBridge] Failed to create IOSurface for %dx%d", width, height);
            return false;
        }

        return true;
    }
};

P5WebKitBridge::P5WebKitBridge()
    : m_impl(std::make_unique<P5WebKitBridgeImpl>()) {
}

P5WebKitBridge::~P5WebKitBridge() {
    shutdown();
}

bool P5WebKitBridge::init(int initialWidth, int initialHeight, const std::string& resourcesPath) {
    m_impl->resourcesDir = resourcesPath;
    m_impl->currentWidth = initialWidth > 0 ? initialWidth : 1920;
    m_impl->currentHeight = initialHeight > 0 ? initialHeight : 1080;

    auto initBlock = ^{
        // 1. Create message handler
        m_impl->scriptHandler = [[P5WebScriptHandler alloc] init];

        // 2. Configure WebKit
        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        [config.preferences setValue:@YES forKey:@"allowFileAccessFromFileURLs"];
        [config.userContentController addScriptMessageHandler:m_impl->scriptHandler name:@"p5Bridge"];

        // 3. Create offscreen host window for complete CoreAnimation & Metal acceleration
        NSRect frame = NSMakeRect(-20000, -20000, m_impl->currentWidth, m_impl->currentHeight);
        m_impl->offscreenWindow = [[NSWindow alloc] initWithContentRect:frame
                                                             styleMask:NSWindowStyleMaskBorderless
                                                               backing:NSBackingStoreBuffered
                                                                 defer:NO];
        m_impl->offscreenWindow.releasedWhenClosed = NO;
        m_impl->offscreenWindow.hasShadow = NO;

        // 4. Create offscreen WKWebView
        m_impl->webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, m_impl->currentWidth, m_impl->currentHeight)
                                             configuration:config];
        m_impl->webView.navigationDelegate = m_impl->scriptHandler;

        // Enable transparent background so p5.js can composite cleanly over DaVinci timeline
        [m_impl->webView setValue:@NO forKey:@"drawsBackground"];

        [m_impl->offscreenWindow.contentView addSubview:m_impl->webView];

        // 5. Load index.html
        NSString *resPath = [NSString stringWithUTF8String:resourcesPath.c_str()];
        if (![resPath isAbsolutePath]) {
            resPath = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:resPath];
        }
        resPath = [resPath stringByStandardizingPath];
        NSString *indexPath = [resPath stringByAppendingPathComponent:@"index.html"];
        NSURL *indexURL = [NSURL fileURLWithPath:indexPath];
        NSURL *readAccessURL = [NSURL fileURLWithPath:resPath];

        NSLog(@"[P5WebKitBridge] Loading HTML template: %@", indexPath);
        [m_impl->webView loadFileURL:indexURL allowingReadAccessToURL:readAccessURL];

        // Pre-allocate initial IOSurface
        m_impl->ensureIOSurface(m_impl->currentWidth, m_impl->currentHeight);
    };

    if ([NSThread isMainThread]) {
        initBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), initBlock);
    }

    return true;
}

bool P5WebKitBridge::updateSketch(const std::string& sketchCode, const std::string& cdnLibraries, int simMode) {
    if (!m_impl->webView) {
        return false;
    }

    // Convert parameters to JSON strings safely
    NSData *codeData = [NSData dataWithBytes:sketchCode.data() length:sketchCode.size()];
    NSString *nsCode = [[NSString alloc] initWithData:codeData encoding:NSUTF8StringEncoding];
    if (!nsCode) nsCode = @"";

    NSData *cdnData = [NSData dataWithBytes:cdnLibraries.data() length:cdnLibraries.size()];
    NSString *nsCdn = [[NSString alloc] initWithData:cdnData encoding:NSUTF8StringEncoding];
    if (!nsCdn) nsCdn = @"";

    NSDictionary *payload = @{
        @"code": nsCode,
        @"cdn": nsCdn,
        @"simMode": @(simMode)
    };

    NSError *jsonErr = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:payload options:0 error:&jsonErr];
    if (jsonErr || !jsonData) {
        return false;
    }

    NSString *jsonStr = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    NSString *jsCall = [NSString stringWithFormat:@"window.setSketchSource(%@);", jsonStr];

    auto updateBlock = ^{
        [m_impl->webView evaluateJavaScript:jsCall completionHandler:^(id result, NSError *error) {
            if (error) {
                NSLog(@"[P5WebKitBridge] updateSketch error: %@", error.localizedDescription);
            }
        }];
    };

    if ([NSThread isMainThread]) {
        updateBlock();
    } else {
        dispatch_async(dispatch_get_main_queue(), updateBlock);
    }

    return true;
}

bool P5WebKitBridge::renderFrame(double frame, double time, double fps,
                                 int width, int height,
                                 void* dstBuffer, int dstRowBytes,
                                 bool isFloatFormat) {
    if (!m_impl->webView || !dstBuffer || width <= 0 || height <= 0) {
        return false;
    }

    __block bool renderSuccess = false;
    __block bool completed = false;

    auto executeRender = ^(void (^onFinish)(bool)) {
        // Ensure webview size matches render resolution
        if (m_impl->currentWidth != width || m_impl->currentHeight != height) {
            [m_impl->webView setFrame:NSMakeRect(0, 0, width, height)];
            [m_impl->offscreenWindow setContentSize:NSMakeSize(width, height)];
            m_impl->ensureIOSurface(width, height);
        }

        // Call JS deterministic frame render function
        NSString *jsCall = [NSString stringWithFormat:@"window.renderResolveFrame(%f, %f, %f, %d, %d);",
                            frame, time, fps, width, height];

        [m_impl->webView evaluateJavaScript:jsCall completionHandler:^(id result, NSError *error) {
            if (error) {
                NSLog(@"[P5WebKitBridge] JS Render error: %@", error.localizedDescription);
                onFinish(false);
                return;
            }

            // Snapshot the rendered WebKit frame into CGImage
            WKSnapshotConfiguration *snapshotConfig = [[WKSnapshotConfiguration alloc] init];
            snapshotConfig.rect = NSMakeRect(0, 0, width, height);
            snapshotConfig.snapshotWidth = @(width);

            [m_impl->webView takeSnapshotWithConfiguration:snapshotConfig completionHandler:^(NSImage *snapshot, NSError *snapErr) {
                if (snapErr || !snapshot) {
                    NSLog(@"[P5WebKitBridge] Snapshot error: %@", snapErr.localizedDescription);
                    onFinish(false);
                    return;
                }

                CGImageRef cgImage = [snapshot CGImageForProposedRect:NULL context:nil hints:nil];
                if (!cgImage) {
                    onFinish(false);
                    return;
                }

                // Lock the IOSurface kernel buffer
                IOSurfaceRef surface = m_impl->ioSurface;
                if (!surface) {
                    onFinish(false);
                    return;
                }

                IOSurfaceLock(surface, 0, NULL);
                void* surfaceBase = IOSurfaceGetBaseAddress(surface);
                size_t surfaceRowBytes = IOSurfaceGetBytesPerRow(surface);

                // Render CGImage directly into IOSurface backing memory
                CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                CGContextRef ctx = CGBitmapContextCreate(
                    surfaceBase,
                    width, height, 8,
                    surfaceRowBytes,
                    colorSpace,
                    kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast
                );

                if (ctx) {
                    // Clear background before drawing snapshot
                    CGContextClearRect(ctx, CGRectMake(0, 0, width, height));
                    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cgImage);
                    CGContextRelease(ctx);
                }
                CGColorSpaceRelease(colorSpace);

                // Transfer from IOSurface memory to OpenFX output buffer
                // OpenFX coordinate convention: bottom-left is (0, 0).
                // HTML5 / WebKit convention: top-left is (0, 0).
                // We perform a vertical flip during copy.
                const uint8_t* srcBytes = (const uint8_t*)surfaceBase;
                uint8_t* dstBytes = (uint8_t*)dstBuffer;

                if (!isFloatFormat) {
                    // 8-bit RGBA
                    for (int y = 0; y < height; ++y) {
                        const uint8_t* srcRow = srcBytes + (y * surfaceRowBytes);
                        uint8_t* dstRow = dstBytes + ((height - 1 - y) * dstRowBytes);
                        memcpy(dstRow, srcRow, width * 4);
                    }
                } else {
                    // 32-bit Float RGBA (0.0f - 1.0f)
                    const float inv255 = 1.0f / 255.0f;
                    for (int y = 0; y < height; ++y) {
                        const uint8_t* srcRow = srcBytes + (y * surfaceRowBytes);
                        float* dstRow = (float*)(dstBytes + ((height - 1 - y) * dstRowBytes));
                        for (int x = 0; x < width; ++x) {
                            dstRow[x * 4 + 0] = srcRow[x * 4 + 0] * inv255;
                            dstRow[x * 4 + 1] = srcRow[x * 4 + 1] * inv255;
                            dstRow[x * 4 + 2] = srcRow[x * 4 + 2] * inv255;
                            dstRow[x * 4 + 3] = srcRow[x * 4 + 3] * inv255;
                        }
                    }
                }

                IOSurfaceUnlock(surface, 0, NULL);
                onFinish(true);
            }];
        }];
    };

    if ([NSThread isMainThread]) {
        executeRender(^(bool ok) {
            renderSuccess = ok;
            completed = true;
        });

        // Pump runloop if running synchronously on main thread until completed
        NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:3.0];
        while (!completed && [timeoutDate timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.005]];
        }
    } else {
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        dispatch_async(dispatch_get_main_queue(), ^{
            executeRender(^(bool ok) {
                renderSuccess = ok;
                completed = true;
                dispatch_semaphore_signal(sema);
            });
        });

        // 3-second watchdog timer to avoid any host UI lockup
        intptr_t waitResult = dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, 3000 * NSEC_PER_MSEC));
        if (waitResult != 0) {
            NSLog(@"[P5WebKitBridge] Render timeout on frame %f", frame);
            return false;
        }
    }

    return renderSuccess;
}

std::string P5WebKitBridge::getLastLogMessage() const {
    if (m_impl->scriptHandler && m_impl->scriptHandler.lastLog) {
        return [m_impl->scriptHandler.lastLog UTF8String];
    }
    return "";
}

bool P5WebKitBridge::isReady() const {
    return m_impl->scriptHandler ? m_impl->scriptHandler.isReady : false;
}

void P5WebKitBridge::shutdown() {
    auto shutdownBlock = ^{
        m_impl->cleanup();
    };

    if ([NSThread isMainThread]) {
        shutdownBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), shutdownBlock);
    }
}
