// Default Starter Sketch for P5.js Video Effect (FX Mode)
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

  // Sample and draw stylized reactive matrix
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
