# Designing C-app GUIs with Flipper GUI Studio

[**Flipper GUI Studio**](https://tools.ranzlappen.com/tools/flipper-gui/)
is a browser-based, zero-install visual editor for the Flipper Zero's
128×64 screen. You drag widgets onto a canvas, wire buttons to screens,
and export a **build-ready C app** that drops straight into this repo's
`C-Apps/` directory and builds with uFBT.

It targets the **C path** only. The Flipper JS runtime has no
pixel-canvas drawing API, so a pixel design can't become a JS script — see
[*Why C only?*](#why-c-only) below.

There's a ready-made reference of the tool's exact output at
[`C-Apps/templates/gui-studio/`](../C-Apps/templates/gui-studio).

---

## Workflow

### 1. Design

Open the tool and build your screens:

- Drag **primitives** (text, box, frame, line, dot, icon) and **widgets**
  (button, progress bar, menu, toggle) onto the canvas.
- Add **screens** with the tab bar; wire a button's *Action* to `goto`
  another screen, or to a `custom_event` (an integer code your app
  reacts to).
- Bind a `progress` / `toggle` / `menu` value to a `var:` name to have it
  read from a generated model field.

### 2. Fill in App settings

In the palette's **App settings** panel set:

| Field | Becomes | Notes |
|---|---|---|
| App name | `name` in `application.fam` | display name in the launcher |
| Namespace | `appid` + C identifier prefix | lowercase snake_case |
| Category | `fap_category` | SD-card folder (`/ext/apps/<Category>/`) |
| Stack size | `stack_size` | KB |
| Requires | `requires=[…]` | SDK modules (`gui` is always included) |
| Launcher icon | `icon.png` | 10×10; a default is provided |

A **validate-ready** badge shows the derived `C-Apps/<folder>/`, `appid`
and entry-point so you can confirm they match this repo's CI rules before
exporting.

### 3. Export the C bundle

Open **Export**, set the bundle target to **C app**, and click
**Download bundle (.zip)**. You get:

```
<your-app>/
├── application.fam            manifest (appid, entry_point, category, icon)
├── <appid>.c                  entry point + <ns>_on_event() override
├── <ns>_scene.c               generated drawing + input handling
├── <ns>_scene.h               screen enum, model struct, public API
├── <appid>.flipper-gui.json   the design spec — source of truth, re-editable
└── icon.png                   10×10 1-bit launcher icon
```

The `*.flipper-gui.json` sidecar is what makes the app **round-trippable**:
it's the canonical `flipper-gui/v1` design, and `application.fam` +
`<ns>_scene.{c,h}` are regenerated from it. Commit it alongside the rest —
[`regen-check`](#editing-later) enforces that the committed C never drifts
from it.

### 4. Drop into the repo and build

Unzip the folder into `C-Apps/` and build with uFBT (see
[`C-Apps/ufbt-config.md`](../C-Apps/ufbt-config.md) for first-time setup):

```bash
unzip <your-app>.zip -d C-Apps/
cd C-Apps/<your-app>
ufbt              # → dist/<appid>.fap
ufbt launch       # build + upload + run over USB
```

Deploy `dist/<appid>.fap` to `/ext/apps/<Category>/` (see
[`docs/deployment.md`](deployment.md) for copy methods).

### 5. Validate before a PR

The generated `application.fam` already satisfies this repo's checks
(`appid` equals the folder name, and `entry_point` is defined in
`<appid>.c`). Confirm with:

```bash
cd JS-Apps
npm run validate      # FAM identifiers + the *.flipper-gui.json sidecar
npm run regen-check   # the committed C matches its flipper-gui spec, byte-for-byte
```

`ci.yml` then runs both checks and builds the app automatically on your
pull request.

---

## Adding app logic

Buttons and menu items can emit **custom events**. The scene calls
`<ns>_on_event(event, state)` whenever one fires; a weak no-op default
lives in `<ns>_scene.c`, and `<appid>.c` overrides it for you to fill in:

```c
void my_app_on_event(int32_t event, MyAppModel* state) {
    switch(event) {
        case 1:
            // e.g. notification_message(notifications, &sequence_success);
            break;
    }
}
```

Read live UI state (a menu cursor, a bound toggle/progress field) from the
model struct via `<ns>_scene_model(scene)`.

## Editing later

The committed **`<appid>.flipper-gui.json`** is the source of truth. To
change the GUI, open it back up in the tool — **Export → Load JSON**, pick
the committed sidecar — edit visually, and re-export over the folder.
`application.fam` and `<ns>_scene.{c,h}` are **generated**, so don't
hand-edit them; the only file meant for hand-editing is `<appid>.c` (your
`<ns>_on_event()` logic), which a re-export leaves untouched.

`npm run regen-check` keeps the two in lockstep: it re-runs the real Studio
exporters on the committed spec and byte-compares the result against the
committed `application.fam` + `<ns>_scene.{c,h}` (the entry `<appid>.c` and
assets are excluded). If they ever diverge — a stray hand-edit, a stale
spec — CI fails. Regenerate locally with:

```bash
cd JS-Apps && npm run regen-check -- --write
```

This is the **bi-directional** contract: a design flows
Studio → `C-Apps/` as the C bundle, and `C-Apps/` → Studio via the
committed JSON. (Re-deriving the editor design from raw `.c` is *not*
supported — always round-trip through the JSON.) The exporters used by
`regen-check` are a pinned snapshot under
[`JS-Apps/vendor/flipper-gui/`](../JS-Apps/vendor/flipper-gui/SOURCE.md);
bump them when the tool's emitters change.

## Architecture notes

The generated app uses a single **`ViewPort`** with an idiomatic blocking
`FuriMessageQueue` input loop — the same pattern as
[`C-Apps/templates/hello-world`](../C-Apps/templates/hello-world). Back on
the root screen exits; Back on any other screen returns to root.

For apps that need multiple `View`s, a `ViewDispatcher`, or a
`SceneManager`, treat the export as a starting point and wrap it by hand —
native ViewDispatcher emission is a planned tool feature.

The tool can optionally emit icons as a Momentum **`images/` asset
folder** (`fap_icon_assets` + `canvas_draw_icon`) instead of inline XBM
arrays; inline XBM is the self-contained default.

## Why C only?

Flipper GUI Studio designs are pixel-precise canvas layouts, which map
directly to the C `Canvas` API (`canvas_draw_str`, `canvas_draw_frame`,
`canvas_draw_icon`, …). The Momentum **JS** runtime exposes higher-level
*view factories* (`submenu`, `dialog`, `text_input`, …) — see
[`docs/JS-API-Reference.md`](JS-API-Reference.md) — and has **no
pixel-canvas API**, so a pixel design cannot be turned into a JS script.

The tool's **Preview** bundle renders your design to an HTML canvas for
sharing/review; it is **not** a deployable Flipper app. For anything that
runs on-device, use the **C app** bundle.
