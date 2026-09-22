// ==================================================================
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
