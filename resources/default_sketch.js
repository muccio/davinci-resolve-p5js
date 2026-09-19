// Default Deterministic Starter Sketch for DaVinci Resolve
// Modify this code directly in the Inspector or paste your own sketch!

function setup() {
  createCanvas(width, height);
  colorMode(HSB, 360, 100, 100, 1.0);
  noStroke();
}

function draw() {
  // Clear with subtle alpha for motion trails or solid black
  background(0, 0, 8, 0.95);

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
