# P5.js for DaVinci Resolve (macOS)
### Native OpenFX Generator & Video Filter Effect

Native OpenFX plugin suite for DaVinci Resolve on macOS (Apple Silicon / Intel) providing seamless execution of standard `p5.js` sketches with full external CDN library support, Metal-accelerated WebKit rendering, kernel-level `IOSurface` shared memory buffers, and dedicated hooks for video input (`videoIn`) and audio synchronization (`audioIn`).

---

## 📦 Two Plugins in One Bundle

The bundle exports two distinct OpenFX plugins side-by-side:

| Plugin | Context | Location in Resolve | Description |
| :--- | :--- | :--- | :--- |
| **P5.js Canvas Generator** | Generator | `Effects -> Generators` | Autonomous motion graphics canvas synchronized with timeline playback. |
| **P5.js Canvas Effect** | Filter | `Effects -> OpenFX -> Filters` | Video filter applied directly onto video clips with `videoIn` and `audioIn` hooks. |

---

## ✨ Key Features

- **Zero-Friction UX**: Drag the Generator or Filter into your DaVinci Resolve Timeline, paste any standard p5.js sketch into the Inspector, and play or scrub immediately.
- **`videoIn` Processing Hook (FX Mode)**: Access the incoming timeline video frame directly in your sketch using Processing / p5.js conventions:
  - `image(videoIn, 0, 0)`: Draw or composite the video clip.
  - `videoIn.loadPixels()`: Direct access to pixel array (`videoIn.pixels`).
  - `videoIn.get(x, y)`: Color sampling of the video at any coordinate.
  - `texture(videoIn)`: Bind the live video as a texture in 3D WebGL mode.
- **`audioIn` Sound Synchronization (FX Mode)**: Frame-accurate audio reactivity:
  - **Audio File (Deterministic)**: Select any audio (`.wav`, `.mp3`, `.m4a`) or video file (`.mov`, `.mp4`) via the Inspector. Native CoreAudio + Apple Accelerate vDSP FFT calculates volume RMS and frequency spectra at 5 microseconds per frame, guaranteeing deterministic scrubbing and export!
  - **Live WebAudio**: Real-time microphone or system audio loopback capture.
  - API: `audioIn.getLevel()`, `audioIn.waveform()`, `audioIn.fft()`, `audioIn.bass`, `audioIn.mid`, `audioIn.treble`.
- **Offscreen WebKit + IOSurface Architecture**: Zero-copy kernel memory sharing between Apple's WebKit rendering pipeline and DaVinci Resolve's OpenFX render engine.
- **In-Memory Frame Transfer**: High-speed custom scheme (`resolve-frame://`) decodes video input frames directly in WebKit RAM without touching the disk or converting to Base64.
- **Deterministic Timeline Clock**: Seamless synchronization of `frameCount`, `millis()`, `deltaTime`, `width`, and `height` tied directly to DaVinci Resolve's timeline frame index and FPS.
- **External CDN & Custom Script Support**: Multi-line Inspector field to import external JavaScript/p5 libraries (e.g., `matter.js`, `simplex-noise`, `chroma-js`, `three.js`) via web CDN or local scripts.
- **2D & 3D WebGL Support**: Hardware-accelerated graphics with full alpha transparency compositing over underlying timeline tracks.
- **Graceful Error Handling**: On-screen visual error overlay for JavaScript syntax/runtime exceptions prevents DaVinci Resolve crashes or black screens.

---

## 🚀 Quick Start & Build

```bash
# 1. Build the OpenFX Bundle and Standalone Test Harness
mkdir -p build && cd build
DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake ..
DEVELOPER_DIR=/Library/Developer/CommandLineTools cmake --build .

# 2. Run automated offline verification test suite
./test_p5_bridge

# 3. Install into DaVinci Resolve standard OpenFX directory
sudo mkdir -p /Library/OFX/Plugins
sudo cp -R P5Generator.ofx.bundle /Library/OFX/Plugins/
```

*(Alternatively, copy to `~/Library/OFX/Plugins/` without `sudo`)*.

---

## 🎨 Example FX Sketch (`videoIn` + `audioIn`)

```javascript
function setup() {
  createCanvas(width, height);
  noStroke();
}

function draw() {
  // 1. Draw the underlying video clip
  image(videoIn, 0, 0, width, height);

  // 2. React to audio amplitude and bass
  let level = audioIn.getLevel();
  let bass = audioIn.bass;

  // 3. Sample video colors and draw dynamic reactive particles
  let sampleCol = videoIn.get(width / 2, height / 2);
  fill(sampleCol[0], sampleCol[1], sampleCol[2], 200);
  circle(width / 2, height / 2, 50 + level * 200);
}
```

For full architecture details, frame lifecycle diagrams, and comprehensive examples, see the [User Guide](docs/USER_GUIDE.md).
