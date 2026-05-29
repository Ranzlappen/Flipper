/* XBM packing utilities.
 *
 * Flipper's canvas_draw_xbm() consumes XBM-format byte arrays: each
 * row is packed LSB-first into bytes, padded to a whole byte at the
 * row boundary. A 1 bit = pixel ON (black on Flipper LCD).
 *
 * Our packed form is stored as base64 in state.icons[i].bits to keep
 * the hash compact and the JSON spec readable.
 */

const bytesPerRow = (w) => Math.ceil(w / 8);

/* Pack an array of 0/1 pixels (row-major, length w*h) into XBM bytes. */
export function packXbm(pixels, w, h) {
  const stride = bytesPerRow(w);
  const out = new Uint8Array(stride * h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const v = pixels[y * w + x] ? 1 : 0;
      if (v) {
        const byteIdx = y * stride + (x >> 3);
        const bit = x & 7;
        out[byteIdx] |= 1 << bit; // LSB-first
      }
    }
  }
  return out;
}

/* Unpack XBM bytes back into a 0/1 row-major pixel array. */
export function unpackXbm(bytes, w, h) {
  const stride = bytesPerRow(w);
  const out = new Uint8Array(w * h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const byteIdx = y * stride + (x >> 3);
      const bit = x & 7;
      out[y * w + x] = (bytes[byteIdx] >> bit) & 1;
    }
  }
  return out;
}

/* base64 helpers — use Uint8Array → base64 string (no URL-safe needed
 * since these go into JSON, not URL params; the hash uses the JSON
 * codec from tool.js). */
export function bytesToB64(bytes) {
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
  return btoa(s);
}

export function b64ToBytes(b64) {
  const s = atob(b64);
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
  return out;
}

/* Convert ImageData → 1bpp pixels with threshold or Floyd–Steinberg.
 * Returns Uint8Array(w*h) of 0/1. */
export function imageDataToBits(imgData, w, h, { threshold = 128, dither = false } = {}) {
  const src = imgData.data; // RGBA
  // Convert to grayscale buffer (Float for dithering).
  const gray = new Float32Array(w * h);
  for (let i = 0; i < w * h; i++) {
    const r = src[i * 4], g = src[i * 4 + 1], b = src[i * 4 + 2], a = src[i * 4 + 3];
    // Luminance; alpha-blend against white background so transparent → off.
    const lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    const blended = lum * (a / 255) + 255 * (1 - a / 255);
    gray[i] = blended;
  }
  const out = new Uint8Array(w * h);
  if (!dither) {
    for (let i = 0; i < w * h; i++) {
      out[i] = gray[i] < threshold ? 1 : 0;
    }
    return out;
  }
  // Floyd–Steinberg in-place on gray copy.
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      const old = gray[i];
      const newPx = old < threshold ? 0 : 255;
      out[i] = old < threshold ? 1 : 0;
      const err = old - newPx;
      if (x + 1 < w)              gray[i + 1]         += err * 7 / 16;
      if (x - 1 >= 0 && y + 1 < h) gray[i + w - 1]     += err * 3 / 16;
      if (y + 1 < h)              gray[i + w]         += err * 5 / 16;
      if (x + 1 < w && y + 1 < h) gray[i + w + 1]     += err * 1 / 16;
    }
  }
  return out;
}

/* Render a packed XBM byte array onto a 2D context at the given scale.
 * Used by editor preview and icon thumbnails. */
export function renderXbm(ctx, x, y, w, h, bytes, scale = 1) {
  const stride = bytesPerRow(w);
  for (let py = 0; py < h; py++) {
    for (let px = 0; px < w; px++) {
      const bit = (bytes[py * stride + (px >> 3)] >> (px & 7)) & 1;
      if (bit) {
        ctx.fillRect(x + px * scale, y + py * scale, scale, scale);
      }
    }
  }
}

/* Format bytes as a C array literal (with line breaks). */
export function bytesToCArray(bytes, perLine = 12) {
  const parts = [];
  for (let i = 0; i < bytes.length; i++) {
    parts.push("0x" + bytes[i].toString(16).padStart(2, "0"));
  }
  const lines = [];
  for (let i = 0; i < parts.length; i += perLine) {
    lines.push("    " + parts.slice(i, i + perLine).join(", "));
  }
  return lines.join(",\n");
}
