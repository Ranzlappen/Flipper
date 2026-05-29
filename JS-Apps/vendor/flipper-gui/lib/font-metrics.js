/* Flipper font metrics.
 *
 * Per-font vertical metrics (cap height, line height, baseline offset,
 * descent) plus the u8g2 font name used in the C export. Text width and
 * pixel rendering now come from lib/font-render.js (real glyph data);
 * `charW` survives only as the monospaced fallback width used when a
 * font's glyph data hasn't loaded or doesn't define a character.
 *
 * If you find drift between editor preview and device output, look here
 * (vertical metrics / baseline) and in font-render.js (glyph data).
 */

export const FONTS = {
  primary: {
    name: "FontPrimary",
    label: "Primary",
    charW: 6,
    lineH: 8,
    cap: 7,
    descent: 1,
    // y in canvas_draw_str is the baseline; this offsets from top-of-bbox.
    baseline: 7,
  },
  secondary: {
    name: "FontSecondary",
    label: "Secondary",
    charW: 5,
    lineH: 7,
    cap: 6,
    descent: 1,
    baseline: 6,
  },
  keyboard: {
    name: "FontKeyboard",
    label: "Keyboard",
    charW: 6,
    lineH: 9,
    cap: 7,
    descent: 2,
    baseline: 7,
  },
  big_numbers: {
    name: "FontBigNumbers",
    label: "BigNumbers",
    charW: 8,
    lineH: 13,
    cap: 12,
    descent: 1,
    baseline: 12,
  },
};

export function getFont(fontKey) {
  return FONTS[fontKey] || FONTS.primary;
}
