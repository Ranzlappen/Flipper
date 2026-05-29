# Vendored: Flipper GUI Studio exporters

This directory is a **pinned, byte-identical snapshot** of the C-app exporters
from the [`ranzlappen-tools`](https://github.com/ranzlappen/ranzlappen-tools)
repo (`tools/flipper-gui/`). The Flipper repo vendors them so
[`../../scripts/regen-check.mjs`](../../scripts/regen-check.mjs) can regenerate
each committed Studio-generated C app from its `*.flipper-gui.json` spec and
byte-diff the result against the committed files — the robust drift guard for
bi-directional `C-Apps ⇄ Flipper GUI Studio` compatibility.

## Pin

| Field | Value |
|-------|-------|
| Source repo | `ranzlappen/ranzlappen-tools` |
| Source path | `tools/flipper-gui/` |
| Schema | `flipper-gui/v1` |
| Snapshot taken | 2026-05-29 (from the repo ingest provided by the maintainer) |
| Upstream commit | _unpinned — record the exact SHA here on the next re-vendor_ |

> **Bump discipline:** whenever the Studio's emitters change (any of
> `exporters/scene.js`, `fam.js`, `entry.js`, `json.js`, or a font/`xbm` dep),
> re-vendor this snapshot, update the commit SHA above, and re-run
> `npm run regen-check` (regenerating committed scenes if the output changed).

## Contents

```
exporters/   scene.js fam.js entry.js json.js   (the emitter table)
lib/         font-metrics.js font-render.js xbm.js
lib/fonts/   primary.js secondary.js keyboard.js big_numbers.js
```

## Why these files (and not the whole tool)

The exporters are DOM-free in Node: `scene.js` only uses `measureText`
(pure glyph-advance data + `atob`) for button-label centering; the canvas
`blitText` is never called. `await preloadFonts()` once and regeneration is
byte-deterministic. The editor UI, icon picker, PNG encoders, etc. are not
needed for regeneration and are intentionally omitted.

## Do not edit

Treat everything under this directory as read-only mirror code. Fixes belong
upstream in `ranzlappen-tools`; re-vendor afterwards. When the exporters are
published as an npm package (`@ranzlappen/flipper-gui-exporters`), this
directory can be replaced by that devDependency without changing
`regen-check.mjs`'s import sites.
