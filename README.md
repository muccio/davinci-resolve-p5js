# P5.js Canvas Generator for DaVinci Resolve (macOS)

Native OpenFX Generator plugin for DaVinci Resolve on macOS (Apple Silicon / Intel) enabling transparent, zero-friction execution of standard `p5.js` sketches with full external CDN library support, Metal-accelerated WebKit headless rendering, and zero-copy `IOSurface` shared memory buffers.

---

## Key Features

- **Zero-Friction UX**: Drag the generator into your DaVinci Resolve Timeline or Fusion page, paste any standard p5.js sketch into the Inspector, and play or scrub immediately.
- **Offscreen WebKit + IOSurface Architecture**: Zero-copy kernel memory sharing between Apple's WebKit rendering pipeline and DaVinci Resolve's OpenFX render actions.
- **Deterministic Timeline Clock**: Seamless monkey-patching of `frameCount`, `millis()`, `deltaTime`, `width`, and `height` tied directly to DaVinci Resolve's timeline frame index and FPS.
- **External CDN & Custom Script Support**: Multi-line Inspector field to import external JavaScript/p5 libraries (e.g., `matter.js`, `simplex-noise`, `chroma-js`) via web CDN or local scripts.
- **2D & 3D WebGL Support**: Hardware-accelerated graphics with full alpha transparency compositing over underlying timeline video tracks.
- **Graceful Error Handling**: On-screen visual error overlay for JavaScript syntax/runtime exceptions prevents DaVinci Resolve crashes or black screens.

---

## Quick Start & Build

```bash
# Build the OpenFX Bundle and Standalone Test Harness
mkdir -p build && cd build
DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake ..
DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake --build .

# Run automated offline verification
./test_p5_bridge

# Install into DaVinci Resolve standard OpenFX directory
sudo mkdir -p /Library/OFX/Plugins
sudo cp -R P5Generator.ofx.bundle /Library/OFX/Plugins/
```

For full architecture details, frame lifecycle diagrams, and comprehensive examples (2D and 3D WebGL), see the [User Guide](docs/USER_GUIDE.md).
