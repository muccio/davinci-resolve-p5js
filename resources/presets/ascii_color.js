// ==================================================================
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

  // Audio modulation: pulse character size or contrast slightly on bass hits
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
