# Momentum App Framework

[![CI](https://github.com/Ranzlappen/Flipper/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Ranzlappen/Flipper/actions/workflows/ci.yml)
[![Release-Please](https://github.com/Ranzlappen/Flipper/actions/workflows/release-please.yml/badge.svg?branch=main)](https://github.com/Ranzlappen/Flipper/actions/workflows/release-please.yml)

A complete, beginner-friendly starter repository for building **JavaScript scripts** and **C-language `.fap` apps** for the Flipper Zero running [**Momentum Firmware**](https://github.com/Next-Flip/Momentum-Firmware).

> **Phone-only friendly!** You can develop, edit, and deploy JavaScript scripts using only your phone + the Flipper Mobile App. C apps need a computer (or GitHub Codespaces).

---

## Table of Contents

1. [Why two paths?](#why-two-paths)
2. [Quick Start - JavaScript (phone-only)](#quick-start--javascript-phone-only)
3. [Quick Start - C `.fap` apps](#quick-start--c-fap-apps)
4. [Folder Structure](#folder-structure)
5. [Deploying to your Flipper](#deploying-to-your-flipper)
6. [Examples](#examples)
7. [Troubleshooting](#troubleshooting)
8. [Useful Links](#useful-links)
9. [License](#license)

---

## Why two paths?

| Feature | **JavaScript** (`/JS-Apps`) | **C `.fap`** (`/C-Apps`) |
|---|---|---|
| Difficulty | Easy | Advanced |
| Needs computer? | No (phone works) | Yes (uFBT) |
| Compile step | None | Required |
| Best for | Sub-GHz remotes, GUIs, automation | Drivers, performance-critical apps |
| Hot reload | Yes (just re-copy file) | Re-compile + copy |
| Recommended for new users | YES | Only if you need it |

**Most custom apps - especially Sub-GHz remotes like the included Balkon-Markise example - should be written in JavaScript.** You only need C for things like new chip drivers or low-level hardware integration.

---

## Quick Start - JavaScript (phone-only)

### What you need
- A Flipper Zero running [Momentum firmware](https://momentum-fw.dev)
- The [Flipper Mobile App](https://flipperzero.one/app) on your phone
- Any text editor app on your phone (e.g. Acode, Quoda, even GitHub's web editor)

### Steps

1. **Pick a template** from `JS-Apps/templates/`:
   - `basic-script.js` - simplest "Hello World" script
   - `subghz-remote-template.js` - multi-button Sub-GHz remote (loads `.sub` files)
   - `gui-example.js` - GUI dialog and submenu demo

2. **Copy the template** into a new file (e.g. `my_remote.js`). On your phone, you can do this from GitHub's web UI:
   - Open the file in this repo
   - Tap the pencil icon to edit
   - Save it to a fork or use "Raw" -> "Download" / copy/paste into a text editor app

3. **Edit it** in any text editor on your phone. Change button labels, file paths, frequencies, etc.

4. **Transfer to your Flipper:**
   - Open the **Flipper Mobile App**
   - Connect to your Flipper via Bluetooth
   - Open **File Manager** (or "Storage" / "Browse")
   - Navigate to: `SD Card -> apps -> Scripts/`
     *(Create the `Scripts` folder if it does not exist)*
   - Upload your `.js` file there

5. **Run it on the Flipper:**
   - On the Flipper, go to: **Apps -> Scripts -> [your_script.js]**
   - Press OK to launch

That's it! No compilation, no toolchain, no computer required.

> **Tip:** Want even faster iteration? Use [qFlipper](https://flipperzero.one/update) on a desktop to drag-and-drop, but the mobile app works perfectly fine.

---

## Quick Start - C `.fap` apps

C apps require [**uFBT**](https://github.com/flipperdevices/flipperzero-ufbt) (Micro Flipper Build Tool). This needs a computer with Python 3.

### One-time setup (computer required)

```bash
# 1. Install uFBT
pip install --upgrade ufbt

# 2. Bootstrap uFBT for Momentum firmware
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
```

> **No computer? Use GitHub Codespaces!** Click the green `Code` button on this repo -> `Codespaces` -> `Create codespace on main`. Everything is preinstalled.

### Build the example app

```bash
cd C-Apps/templates/hello-world
ufbt
```

The compiled `.fap` will be at `dist/hello_world.fap`. Copy it to your Flipper at:
`SD Card -> apps -> Examples/hello_world.fap`

For more details, see [`C-Apps/ufbt-config.md`](C-Apps/ufbt-config.md).

---

## Folder Structure

```
momentum-app-framework/
├── README.md                       <- You are here
├── LICENSE
├── .gitignore
├── JS-Apps/                        <- JavaScript scripts (recommended path)
│   ├── package.json                <- @next-flip/fz-sdk-mntm + dev deps
│   ├── tsconfig.json               <- TS / IntelliSense for the SDK
│   ├── .env.example
│   ├── templates/
│   │   ├── basic-script.js
│   │   ├── subghz-remote-template.js
│   │   └── gui-example.js
│   └── examples/
│       └── balkon-markise-remote.js
├── C-Apps/                         <- Native .fap apps (advanced)
│   ├── ufbt-config.md
│   └── templates/
│       └── hello-world/
│           ├── application.fam
│           ├── hello_world.c
│           └── README.md
├── docs/
│   ├── JS-API-Reference.md
│   └── deployment.md
└── .github/workflows/
    ├── ci.yml                      <- PR sanity gate: prettier, typecheck, validate, build
    ├── release-please.yml          <- Conventional-commit driven version + CHANGELOG PR
    ├── release-artifacts.yml       <- Builds & attaches .fap/.js on release published
    └── momentum-sync.yml           <- Weekly Momentum SDK upgrade PR
```

---

## Deploying to your Flipper

| App type | Destination on SD card |
|---|---|
| JavaScript script (`.js`) | `/ext/apps/Scripts/` |
| C `.fap` app | `/ext/apps/<Category>/` (e.g. `Examples`, `Tools`, `GPIO`) |
| Sub-GHz files (`.sub`) | `/ext/subghz/` |

**Three ways to copy files:**
1. **Mobile App** - Bluetooth, phone-friendly. Best for `.js` scripts.
2. **qFlipper** - USB or Bluetooth, desktop. Best for `.fap` apps.
3. **microSD card reader** - Pull the SD card out, mount on phone/computer. Bulk copies.

See [`docs/deployment.md`](docs/deployment.md) for a deep-dive.

---

## Examples

**`JS-Apps/examples/balkon-markise-remote.js`** - A polished multi-button Sub-GHz remote. Shows a 3-button menu (`REIN`, `RAUS`, `STOP`) and transmits the corresponding `.sub` file from `/ext/subghz/`. Perfect base for any garage door, awning, or rolling shutter remote.

To use it:
1. Record your `.sub` files using `Sub-GHz -> Read` on your Flipper.
2. Save them as `Balkon_markise_rein.sub`, `Balkon_markise_raus.sub`, `Balkon_markise_stop.sub` in `/ext/subghz/`.
3. Copy `balkon-markise-remote.js` to `/ext/apps/Scripts/`.
4. Run from `Apps -> Scripts`.

---

## Troubleshooting

| Problem | Solution |
|---|---|
| `Script not found` on Flipper | Make sure file is in `/ext/apps/Scripts/` and ends in `.js` |
| `module not found: @flipperdevices/...` | You are on stock firmware. This framework needs **Momentum**. |
| Sub-GHz file fails to transmit | Check the file is in `/ext/subghz/` and the `.sub` was recorded on the same frequency/protocol your remote uses |
| `ufbt: command not found` | Run `pip install --upgrade ufbt` and ensure your Python `~/.local/bin` is in `PATH` |
| `.fap` crashes on launch | Make sure it was built with `ufbt` against the **Momentum** SDK (not stock) |
| No `Scripts` folder visible | Create it manually via the Mobile App File Manager: `apps -> [+] -> New folder -> Scripts` |

---

## Useful Links

- **Momentum Firmware:** https://github.com/Next-Flip/Momentum-Firmware
- **Momentum website / OTA:** https://momentum-fw.dev
- **Momentum Discord:** https://discord.gg/momentum
- **Momentum JS docs:** https://docs.flipper.net/development/js (with Momentum extensions)
- **uFBT:** https://github.com/flipperdevices/flipperzero-ufbt
- **JS SDK package:** https://www.npmjs.com/package/@next-flip/fz-sdk-mntm
- **Flipper Mobile App:** https://flipperzero.one/app

---

## License

MIT - see [`LICENSE`](LICENSE). Use freely, contributions welcome.
