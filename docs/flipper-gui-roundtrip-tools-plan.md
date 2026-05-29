# Plan B — `ranzlappen-tools` changes for C-Apps ⇄ Flipper GUI Studio round-trip

> **How to use this file:** open a Claude Code session in the
> `ranzlappen-tools` repo and paste this brief as the task. It is the
> companion to the work already landed in `ranzlappen/flipper` (the
> `JS-Apps/vendor/flipper-gui/` snapshot, `scripts/regen-check.mjs`, and the
> `*.flipper-gui.json` sidecar convention). Nothing here needs the Flipper
> repo checked out.

## Goal

Make Flipper GUI Studio's **C-app export round-trippable** and keep its
exporters **consumable headless** so the Flipper repo can regenerate +
byte-diff committed C apps against their spec.

## The contract (already enforced on the Flipper side)

Each Studio-generated C-app folder carries `<appid>.flipper-gui.json`
(schema `flipper-gui/v1`) as the **source of truth**. From it, the Flipper
repo regenerates and byte-compares:

| File | Generated from spec | Byte-checked |
|------|---------------------|--------------|
| `application.fam` | yes | ✅ |
| `<ns>_scene.c` / `<ns>_scene.h` | yes | ✅ |
| `<ns>.c` (entry + `on_event`) | once, then user-owned | ❌ excluded |
| `icon.png`, `images/*.png` | assets | ❌ excluded |

So: **the C export folder must include the JSON spec**, and the exporters
must produce **byte-identical** output to the snapshot the Flipper repo
vendors (call `preloadFonts()` first — see B4).

---

## B1 — Emit the JSON sidecar in the C bundle (the key fix)

`tools/flipper-gui/exporters/bundle.js`, in the `target === "c"` branch
(today it writes `fam`, `entry`, `scene()`, `icon.png`, optional `images/`,
`README.md` — but **not** the spec). Add the spec next to the rest:

```js
// inside exportBundle(), the `if (target === "c")` block:
root.file(`${m.appid}.flipper-gui.json`, exporters.json().text);
```

Place it before `root.file("README.md", ...)`. This is the single change
that unblocks the reverse direction: the exported folder becomes
self-describing and re-editable.

## B2 — Name the sidecar from `appMeta`

`tools/flipper-gui/exporters/json.js` currently derives the filename from
the raw `state.app.namespace`:

```js
filename: `${state.app.namespace || "flipper_gui"}.flipper-gui.json`,
```

Make it use the same normalized identifier as everything else so the
sidecar name always matches the folder/appid:

```js
import { appMeta } from "./fam.js";
// ...
filename: `${appMeta(state).appid}.flipper-gui.json`,
```

The `text` payload is unchanged (`exportJson` already serializes the whole
`state.app`, so the FAM is fully recoverable from it). Only the filename
derivation changes.

## B3 — Entry-file preservation

The bundle's `<ns>.c` carries the user's `on_event` logic, so re-export
must not be treated as its authority. Two cheap, additive changes:

1. In the C-bundle README generator (`cReadme` in `bundle.js`) and the
   editor's export UI copy, state the rule explicitly: *"On re-export keep
   your existing `<ns>.c`; overwrite only `application.fam`,
   `<ns>_scene.{c,h}`, and `<appid>.flipper-gui.json`."* This matches the
   Flipper repo's regen-diff exclusion list.
2. (Optional) Have `exporters/entry.js` emit a header comment marking the
   file *"generated once — your code lives here; safe to keep across
   re-exports."*

## B4 — Keep exporters headless-importable

The Flipper repo's `regen-check` imports
`exporters/{scene,fam,entry,json}.js` plus `lib/{font-metrics,font-render,
xbm}.js` and `lib/fonts/*.js` in **Node**. Today this already works because:

- `scene.js` uses only `measureText` (pure glyph-advance data + `atob`) for
  button-label centering; the canvas `blitText` is never called by exporters.
- `font-render.js`, `xbm.js`, `font-metrics.js`, `fonts/*.js` have no
  top-level DOM access.

**Keep it that way.** Guard against regressions:

1. Don't introduce top-level `document`/`window`/`canvas` into any of those
   modules or their imports. If editor-only DOM code is needed, isolate it
   in a module the exporters don't import.
2. Add a thin headless entry, `tools/flipper-gui/exporters/index.js`,
   re-exporting the stable surface:

   ```js
   export { exportFam, appMeta, FAP_CATEGORIES, hasReferencedIcons } from "./fam.js";
   export { exportScene, safeNs, pascal, anyScroll, emitWidgetDraw } from "./scene.js";
   export { exportEntry } from "./entry.js";
   export { exportJson } from "./json.js";
   export { preloadFonts } from "../lib/font-render.js";
   ```

   Document at the top: *"Node consumers: `await preloadFonts()` before
   `exportScene` for byte-identical output."* (Without it, FontSecondary
   button widths fall back to the monospaced `charW` estimate and centering
   differs by a pixel — exactly the drift the Flipper repo regenerated away.)
3. (Nice to have) a tiny Node smoke test: import `index.js`, `preloadFonts`,
   run `exportScene` on a sample state, assert it returns two files. Guards
   the headless contract in CI.

## B5 — Versioning contract

The Flipper repo pins a snapshot in
`JS-Apps/vendor/flipper-gui/SOURCE.md` (currently commit-unpinned). To make
bumps deterministic:

- Tag releases of the tool (or publish the exporters + needed `lib/` as
  `@ranzlappen/flipper-gui-exporters` from `tools/flipper-gui/exporters/`).
  When published, the Flipper repo can swap its vendored dir for that
  devDependency without touching `regen-check.mjs`'s import sites.
- **Whenever an emitter changes** (`scene.js`/`fam.js`/`entry.js`/`json.js`
  or a font/`xbm` dep), note it so the Flipper repo re-vendors and
  regenerates committed scenes (`npm run regen-check -- --write`).
- Update the tool's **README**: drop the *"One-way export — importing
  existing `.c` isn't supported"* limitation and replace it with the
  committed-sidecar round-trip: the C bundle now ships
  `<appid>.flipper-gui.json`; re-edit via **Export → Load JSON** on that
  file. (Raw `.c` parsing remains out of scope — round-trip is via JSON.)

## B6 — (Optional) C-app folder importer

Add a "Load C app folder" affordance to `tool.js`: read `application.fam`
+ the `*.flipper-gui.json`. If the JSON is present, load it directly via the
existing `validateState`. If only the FAM exists, recover `app` settings
from its fields (`appid`/`name`/`fap_category`/`stack_size`/`requires`/…)
and start from an empty design. Keep `.c` parsing out of scope.

---

## Verification (in `ranzlappen-tools`)

1. Export any design as a **C app** bundle → the zip now contains
   `<appid>.flipper-gui.json` named to match the folder/appid (B1, B2).
2. `node -e` (or the B4 smoke test): `await preloadFonts()` then
   `exportScene(sampleState)` returns `<ns>_scene.h` + `<ns>_scene.c`
   without any DOM — confirms headless importability.
3. **Load JSON** on a previously-exported sidecar reproduces the design
   (round-trip), and re-exporting yields the same `<ns>_scene.{c,h}` bytes.

## End-to-end (spans both repos)

- Export a C app from the tool → unzip into the Flipper repo's `C-Apps/` →
  `cd JS-Apps && npm run regen-check` passes with zero diff.
- Edit the committed `<appid>.flipper-gui.json` via Load JSON, move a
  widget, re-export over the folder → `regen-check` shows the scene changed
  in lockstep with the spec and `<appid>.c` is untouched.
