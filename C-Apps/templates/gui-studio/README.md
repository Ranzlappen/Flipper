# gui-studio — Flipper GUI Studio output skeleton

This template is the **exact output** of [Flipper GUI
Studio](https://tools.ranzlappen.com/tools/flipper-gui/) when you export a
design as a **C app** bundle. It's here so generated files have a
documented home and a reference to diff against. Like `hello-world`, it is
excluded from release builds.

## What the tool generates

```
gui-studio/
├── application.fam      manifest (appid "gui_studio", entry "gui_studio_app")
├── gui_studio.c         entry point + the gui_studio_on_event() override
├── gui_studio_scene.c   generated screens: drawing + input handling
├── gui_studio_scene.h   screen enum, model struct, public API, on_event decl
└── icon.png             10x10 1-bit launcher icon
```

This example has two screens — a **Main** screen with a button that
navigates to a **Menu** screen, whose menu items fire a custom event
(`gui_studio_on_event(1, …)`) and navigate back. It demonstrates screen
navigation, the menu cursor model field, and the custom-event hook.

## Build

```bash
cd C-Apps/gui-studio
ufbt              # → dist/gui_studio.fap
ufbt launch       # build + upload + run over USB
```

Deploy `dist/gui_studio.fap` to `/ext/apps/Examples/` on your Flipper.

## Editing

Don't hand-edit `gui_studio_scene.c` — it's generated. Instead re-import
your design's JSON spec into Flipper GUI Studio, change it there, and
re-export. The one file meant for hand-editing is `gui_studio.c`: put your
app logic inside `gui_studio_on_event()`.

See [`docs/gui-tool-integration.md`](../../../docs/gui-tool-integration.md)
for the full workflow.
