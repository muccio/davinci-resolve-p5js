/**
 * p5_shim.js
 * 
 * Deterministic timeline clock, dynamic CDN script loader,
 * error boundary, and host communication bridge for DaVinci Resolve.
 */

(function () {
  'use strict';

  // --- Host Logging and Error Propagation ---
  function postToHost(type, payload) {
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.p5Bridge) {
      window.webkit.messageHandlers.p5Bridge.postMessage({
        type: type,
        payload: String(payload)
      });
    }
  }

  // Intercept standard console logging
  const originalLog = console.log;
  const originalWarn = console.warn;
  const originalError = console.error;

  console.log = function (...args) {
    originalLog.apply(console, args);
    postToHost('log', args.join(' '));
  };

  console.warn = function (...args) {
    originalWarn.apply(console, args);
    postToHost('log', '[WARN] ' + args.join(' '));
  };

  console.error = function (...args) {
    originalError.apply(console, args);
    postToHost('error', args.join(' '));
  };

  // Visual error banner handler
  function showError(msg) {
    const banner = document.getElementById('error-banner');
    const msgEl = document.getElementById('error-message');
    if (banner && msgEl) {
      msgEl.textContent = msg;
      banner.style.display = 'block';
    }
    postToHost('error', msg);
  }

  function clearError() {
    const banner = document.getElementById('error-banner');
    if (banner) {
      banner.style.display = 'none';
    }
  }

  window.addEventListener('error', function (event) {
    const errText = `${event.message} (line ${event.lineno}, col ${event.colno})`;
    showError(errText);
  });

  window.addEventListener('unhandledrejection', function (event) {
    showError('Unhandled Promise Rejection: ' + event.reason);
  });

  // --- Deterministic Timeline Variables ---
  window.resolveFrame = 0;
  window.resolveTime = 0.0;
  window.resolveFPS = 24.0;
  window.resolveWidth = 1920;
  window.resolveHeight = 1080;
  window.resolveSimMode = 0; // 0 = Pure Time, 1 = Cumulative Simulation

  let activeP5Instance = null;
  let lastSimulatedFrame = -1;

  // Safe properties with both getter AND setter to prevent strict mode readonly errors
  function defineClockProperty(obj, prop, getFn, setFn) {
    try {
      Object.defineProperty(obj, prop, {
        get: getFn,
        set: setFn || function () {},
        configurable: true,
        enumerable: true
      });
    } catch (e) {
      console.warn('Could not define property ' + prop + ': ' + e.message);
    }
  }

  // Bind p5 clock overrides
  if (typeof p5 !== 'undefined') {
    // Prevent p5 internal _draw from incrementing frameCount
    const origSetProperty = p5.prototype._setProperty;
    p5.prototype._setProperty = function (prop, val) {
      if (prop === 'frameCount' || prop === 'deltaTime') {
        return;
      }
      if (typeof origSetProperty === 'function') {
        origSetProperty.call(this, prop, val);
      }
    };

    // Override millis
    p5.prototype.millis = function () {
      return window.resolveTime * 1000.0;
    };
    window.millis = p5.prototype.millis;

    // frameCount
    defineClockProperty(p5.prototype, 'frameCount',
      () => window.resolveFrame,
      (v) => {}
    );
    defineClockProperty(window, 'frameCount',
      () => window.resolveFrame,
      (v) => {}
    );

    // deltaTime
    defineClockProperty(p5.prototype, 'deltaTime',
      () => 1000.0 / (window.resolveFPS || 24.0),
      (v) => {}
    );
    defineClockProperty(window, 'deltaTime',
      () => 1000.0 / (window.resolveFPS || 24.0),
      (v) => {}
    );

    // width and height
    defineClockProperty(window, 'width',
      () => (window.resolveWidth || 1920),
      (v) => { window.resolveWidth = v; }
    );
    defineClockProperty(window, 'height',
      () => (window.resolveHeight || 1080),
      (v) => { window.resolveHeight = v; }
    );

    // Hook createCanvas to enforce Resolve timeline resolution
    const origCreateCanvas = p5.prototype.createCanvas;
    p5.prototype.createCanvas = function (w, h, renderer) {
      const targetW = window.resolveWidth || w || 1920;
      const targetH = window.resolveHeight || h || 1080;
      const cnv = origCreateCanvas.call(this, targetW, targetH, renderer);
      cnv.parent('canvas-root');
      return cnv;
    };

    // Forward standard p5 functions AND constants to global scope
    for (const key in p5.prototype) {
      if (typeof p5.prototype[key] === 'function') {
        if (!(key in window)) {
          window[key] = function (...args) {
            if (activeP5Instance && typeof activeP5Instance[key] === 'function') {
              return activeP5Instance[key].apply(activeP5Instance, args);
            }
          };
        }
      } else {
        if (!(key in window)) {
          try {
            window[key] = p5.prototype[key];
          } catch (e) {}
        }
      }
    }

    // Explicitly define standard p5 constants on window
    const p5Constants = {
      HSB: 'hsb',
      RGB: 'rgb',
      HSL: 'hsl',
      WEBGL: 'webgl',
      P2D: 'p2d',
      PI: Math.PI,
      TWO_PI: Math.PI * 2,
      HALF_PI: Math.PI / 2,
      QUARTER_PI: Math.PI / 4,
      TAU: Math.PI * 2,
      DEGREES: 'degrees',
      RADIANS: 'radians',
      CENTER: 'center',
      RADIUS: 'radius',
      CORNER: 'corner',
      CORNERS: 'corners',
      CLOSE: 'close',
      OPEN: 'open',
      CHORD: 'chord',
      PIE: 'pie',
      POINTS: 0x0000,
      LINES: 0x0001,
      TRIANGLES: 0x0004,
      TRIANGLE_STRIP: 0x0005,
      TRIANGLE_FAN: 0x0006,
      QUADS: 0x0010,
      QUAD_STRIP: 0x0011,
      TESS: 'tess',
      PROJECT: 'square',
      SQUARE: 'butt',
      ROUND: 'round',
      BEVEL: 'bevel',
      MITER: 'miter',
      AUTO: 'auto',
      BLEND: 'source-over',
      REMOVE: 'destination-out',
      DARKEST: 'darkest',
      LIGHTEST: 'lighten',
      DIFFERENCE: 'difference',
      SUBTRACT: 'subtract',
      EXCLUSION: 'exclusion',
      MULTIPLY: 'multiply',
      SCREEN: 'screen',
      REPLACE: 'copy',
      OVERLAY: 'overlay',
      HARD_LIGHT: 'hard-light',
      SOFT_LIGHT: 'soft-light',
      DODGE: 'color-dodge',
      BURN: 'color-burn',
      THRESHOLD: 'threshold',
      GRAY: 'gray',
      OPAQUE: 'opaque',
      INVERT: 'invert',
      POSTERIZE: 'posterize',
      DILATE: 'dilate',
      ERODE: 'erode',
      BLUR: 'blur',
      NORMAL: 'normal',
      ITALIC: 'italic',
      BOLD: 'bold',
      BOLDITALIC: 'bolditalic'
    };

    for (const c in p5Constants) {
      if (!(c in window)) {
        window[c] = p5Constants[c];
      }
    }
    window.p5Constants = p5Constants;
  }

  // --- Dynamic CDN Loader ---
  function loadScript(url) {
    return new Promise((resolve, reject) => {
      const trimmed = url.trim();
      if (!trimmed || trimmed.startsWith('#')) {
        resolve();
        return;
      }
      const existing = document.querySelector(`script[src="${trimmed}"]`);
      if (existing) {
        resolve();
        return;
      }

      const script = document.createElement('script');
      script.src = trimmed;
      script.crossOrigin = 'anonymous';
      script.onload = () => {
        console.log(`[P5-CDN] Loaded: ${trimmed}`);
        resolve();
      };
      script.onerror = () => {
        const err = `Failed to load external library: ${trimmed}`;
        console.error(err);
        reject(new Error(err));
      };
      document.head.appendChild(script);
    });
  }

  async function loadAllCDNs(cdnListString) {
    const urls = cdnListString
      .split('\n')
      .map(s => s.trim())
      .filter(s => s.length > 0 && !s.startsWith('#'));

    for (const url of urls) {
      try {
        await loadScript(url);
      } catch (err) {
        showError(err.message);
      }
    }
  }

  // --- Sketch Compilation & Execution ---
  window.setSketchSource = function (options) {
    clearError();
    const code = options.code || '';
    const cdns = options.cdn || '';
    window.resolveSimMode = options.simMode || 0;

    // Load CDNs asynchronously in background
    if (cdns.trim().length > 0) {
      loadAllCDNs(cdns);
    }

    // Tear down existing p5 instance
    if (activeP5Instance) {
      try {
        activeP5Instance.remove();
      } catch (e) {}
      activeP5Instance = null;
    }

    const container = document.getElementById('canvas-root');
    if (container) {
      container.innerHTML = '';
    }

    try {
      const sketchWrapper = function (p) {
        activeP5Instance = p;
        window.p5Instance = p;

        // Compile user script without problematic 'with' blocks
        // By evaluating inside a function that returns setup and draw
        const compileFn = new Function('p', `
          ${code}
          const s = (typeof setup === 'function') ? setup : (typeof p.setup === 'function' ? p.setup : null);
          const d = (typeof draw === 'function') ? draw : (typeof p.draw === 'function' ? p.draw : null);
          return { userSetup: s, userDraw: d };
        `);

        const compiled = compileFn.call(p, p);
        defineClockProperty(p, 'frameCount', () => window.resolveFrame);
        defineClockProperty(p, 'deltaTime', () => 1000.0 / (window.resolveFPS || 24.0));

        p.setup = function () {
          if (compiled.userSetup) {
            compiled.userSetup.call(p);
          }
          // Enforce noLoop() to prevent asynchronous requestAnimationFrame
          p.noLoop();
        };

        if (compiled.userDraw) {
          p.draw = function () {
            compiled.userDraw.call(p);
          };
        }
      };

      new p5(sketchWrapper, 'canvas-root');
      lastSimulatedFrame = -1;
      console.log('[P5] Sketch compiled successfully.');
    } catch (e) {
      showError(`Compilation Error: ${e.message}`);
    }

    return true;
  };

  // --- Deterministic Frame Render Action ---
  window.renderResolveFrame = function (targetFrame, targetTime, targetFPS, width, height) {
    window.resolveFrame = Math.round(targetFrame);
    window.resolveTime = targetTime;
    window.resolveFPS = targetFPS > 0 ? targetFPS : 24.0;

    // Handle resolution change
    if (width > 0 && height > 0 && (width !== window.resolveWidth || height !== window.resolveHeight)) {
      window.resolveWidth = width;
      window.resolveHeight = height;
      if (activeP5Instance && typeof activeP5Instance.resizeCanvas === 'function') {
        activeP5Instance.resizeCanvas(width, height);
      }
    }

    if (!activeP5Instance) {
      return true;
    }

    try {
      if (window.resolveSimMode === 0) {
        // Stateless / Deterministic Time Mode (Default)
        activeP5Instance.redraw();
      } else {
        // Cumulative Simulation Mode
        if (targetFrame < lastSimulatedFrame || lastSimulatedFrame < 0) {
          if (typeof activeP5Instance.setup === 'function') {
            activeP5Instance.setup();
          }
          lastSimulatedFrame = 0;
        }

        const startFrame = lastSimulatedFrame + 1;
        for (let f = startFrame; f <= targetFrame; ++f) {
          window.resolveFrame = f;
          window.resolveTime = f / window.resolveFPS;
          activeP5Instance.redraw();
        }
        lastSimulatedFrame = targetFrame;
      }

      // Flush WebGL commands if rendering with WebGL
      const canvas = document.querySelector('canvas');
      if (canvas) {
        const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
        if (gl) {
          gl.finish();
        }
      }
    } catch (err) {
      showError(`Runtime Draw Error: ${err.message}`);
    }

    return true;
  };

  // Notify host that the HTML environment has finished loading
  postToHost('ready', 'Environment initialized');
})();
