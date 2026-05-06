# Hello World - C `.fap` template

Minimal C app for Flipper Zero (Momentum firmware), built with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt).

## Build

```bash
# from repo root:
cd C-Apps/templates/hello-world
ufbt
```

The compiled artifact is at `dist/hello_world.fap`.

## Deploy

| Method | Steps |
|---|---|
| **uFBT (USB)** | `ufbt launch` - builds, uploads, and runs in one step. |
| **qFlipper** | Drag `dist/hello_world.fap` to `apps/Examples/`. |
| **Mobile App** | Connect via Bluetooth -> Storage -> `apps/Examples/` -> Upload. |

Then on the Flipper: **Apps -> Examples -> Hello World**.

## File guide

- `application.fam` - manifest (name, category, entry point, etc.)
- `hello_world.c` - source. Entry point `hello_world_app`.

## Customising

1. Rename `appid` in `application.fam` (e.g. `my_app`). The output `.fap` is named after this.
2. Rename `entry_point` (e.g. `my_app_main`) and update the C function name to match.
3. Move/rename the `.c` file freely - uFBT picks up all `.c` files in the folder.
4. Add icon assets to an `images/` folder (10x10 PNG, 1-bit) and reference via `fap_icon_assets`.

## Common build problems

| Error | Fix |
|---|---|
| `ufbt: command not found` | `pip install --upgrade ufbt`, restart terminal |
| `SDK not found` | `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release` |
| `furi.h not found` | You did not bootstrap uFBT for Momentum yet; see line above |
| `fap too large` | Increase `stack_size` only if you actually need it; otherwise refactor |
