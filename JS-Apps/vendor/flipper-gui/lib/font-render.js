/* Pixel-exact font rendering for the editor preview.
 *
 * Three of Flipper's four fonts ship real bitmap glyph data extracted
 * from their u8g2 BDF sources (see build/extract-fonts.mjs):
 *
 *   primary     → helvB08    (bitmaps + advances)
 *   keyboard    → profont11  (bitmaps + advances)
 *   big_numbers → profont22  (bitmaps + advances, digits/punct only)
 *   secondary   → haxrcorp4089 (advances only — no upstream BDF, so the
 *                 editor falls back to fillText but lays text out using
 *                 the real per-glyph advances)
 *
 * Glyph bitmaps are stored MSB-first, ceil(w/8) bytes per row, base64.
 * Everything renders at native 128×64 scale 1; the canvas is CSS-scaled
 * with image-rendering:pixelated, so 1 glyph pixel = 1 device pixel.
 *
 * Fonts are loaded asynchronously (dynamic import). Call preloadFonts()
 * during boot and await it before the first render; measureText/blitText
 * fall back gracefully to the metric tables until the data arrives.
 */

import { getFont } from "./font-metrics.js";

const LOADERS = {
  primary: () => import("./fonts/primary.js"),
  secondary: () => import("./fonts/secondary.js"),
  keyboard: () => import("./fonts/keyboard.js"),
  big_numbers: () => import("./fonts/big_numbers.js"),
};

const cache = {}; // key → font data (default export) once loaded
const decoded = new WeakMap(); // glyph obj → Uint8Array of its rows

export async function preloadFonts() {
  await Promise.all(
    Object.keys(LOADERS).map(async (key) => {
      try {
        const mod = await LOADERS[key]();
        cache[key] = mod.default;
      } catch {
        cache[key] = null; // fall back to metrics for this font
      }
    })
  );
}

function data(key) {
  return cache[key] ?? cache.primary ?? null;
}

/* Advance width of a single codepoint, or null if unknown in this font. */
function glyphAdvance(font, code) {
  if (!font) return null;
  if (font.widthsOnly) {
    const w = font.widths[code];
    return w == null ? null : w;
  }
  const g = font.glyphs[code];
  return g ? g.dx : null;
}

/* Pixel width of a string in the given font. Falls back to the
 * monospaced charW estimate from font-metrics for characters the font
 * doesn't define (and for fonts that haven't loaded yet). */
export function measureText(text, key) {
  const s = String(text ?? "");
  const font = data(key);
  const m = getFont(key);
  if (!font) return { w: s.length * m.charW, h: m.lineH };
  let w = 0;
  for (const ch of s) {
    const adv = glyphAdvance(font, ch.codePointAt(0));
    w += adv == null ? m.charW : adv;
  }
  return { w, h: font.lineHeight ?? m.lineH };
}

function rowsOf(glyph) {
  let bytes = decoded.get(glyph);
  if (!bytes) {
    const bin = atob(glyph.b);
    bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    decoded.set(glyph, bytes);
  }
  return bytes;
}

/* Draw `text` with its top-left at (x, y) using the current ctx
 * fillStyle as the pixel-on color. Returns the total advance width, or
 * null if this font has no bitmaps (caller should fall back to fillText
 * but may still trust measureText for layout). */
export function blitText(ctx, x, y, text, key) {
  const font = data(key);
  if (!font || font.widthsOnly) return null;
  const s = String(text ?? "");
  const ascent = font.ascent;
  let pen = 0;
  for (const ch of s) {
    const code = ch.codePointAt(0);
    const g = font.glyphs[code];
    if (!g) {
      pen += getFont(key).charW; // undefined glyph: advance, draw nothing
      continue;
    }
    if (g.w > 0 && g.h > 0) {
      const bytes = rowsOf(g);
      const stride = Math.ceil(g.w / 8);
      const top = y + ascent - g.oy - g.h;
      for (let r = 0; r < g.h; r++) {
        const base = r * stride;
        for (let c = 0; c < g.w; c++) {
          if ((bytes[base + (c >> 3)] >> (7 - (c & 7))) & 1) {
            ctx.fillRect(x + pen + g.ox + c, top + r, 1, 1);
          }
        }
      }
    }
    pen += g.dx;
  }
  return pen;
}

/* True if this font draws real bitmaps (vs. width-only fallback). */
export function hasBitmaps(key) {
  const font = data(key);
  return !!(font && !font.widthsOnly);
}
