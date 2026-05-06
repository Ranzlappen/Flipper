# uFBT Setup for Momentum Firmware

[**uFBT**](https://github.com/flipperdevices/flipperzero-ufbt) (Micro Flipper Build Tool) is the official tool for building Flipper Zero applications outside the main firmware tree.

This document explains exactly how to point uFBT at the **Momentum** SDK.

---

## 1. Install uFBT

Requires **Python 3.8+**.

```bash
pip install --upgrade ufbt
```

Verify:

```bash
ufbt --version
```

If `ufbt: command not found`, add Python's user-script directory to your `PATH`:

| OS | Path |
|---|---|
| Linux/macOS | `~/.local/bin` |
| Windows | `%APPDATA%\Python\Python3xx\Scripts` |

---

## 2. Bootstrap for Momentum

uFBT defaults to the official Flipper Devices SDK. To target Momentum, run:

```bash
ufbt update \
  --index-url=https://up.momentum-fw.dev/firmware/directory.json \
  --channel=release
```

Replace `release` with `dev` if you want bleeding-edge.

This downloads the matching SDK into `~/.ufbt/current/`.

### Persist the channel

To avoid retyping the index URL every time, create a `.ufbt/ufbt_state.json` in your project (uFBT will pick it up automatically). Or set environment variables:

```bash
export UFBT_INDEX_URL=https://up.momentum-fw.dev/firmware/directory.json
export UFBT_CHANNEL=release
```

---

## 3. Common commands

| Command | What it does |
|---|---|
| `ufbt` | Build the current app (uses `application.fam` in cwd). |
| `ufbt launch` | Build, upload to a connected Flipper via USB, and launch. |
| `ufbt cli` | Drop into the Flipper's CLI over USB. |
| `ufbt update` | Re-fetch the SDK (use after Momentum releases). |
| `ufbt vscode_dist` | Generate VSCode IntelliSense config. |
| `ufbt format` | Apply clang-format to all `.c`/`.h`. |

---

## 4. Project layout uFBT expects

A C app folder must contain at least:

```
my_app/
├── application.fam        # manifest
└── my_app.c               # source(s) - all .c files in folder are compiled
```

Optional:

```
├── images/                # 10x10 1-bit PNG icons
├── helpers/               # additional .c/.h
└── README.md
```

---

## 5. Targeting specific hardware revisions

Most Flipper Zeros use the F7 target. uFBT picks this up automatically from the SDK. You normally do **not** need to set anything.

---

## 6. CI / GitHub Actions

This repo's `.github/workflows/build-and-release.yml` already invokes `ufbt` against the Momentum index URL on every push to `main`. See that file for a working CI reference.

---

## 7. Useful links

- uFBT repo: https://github.com/flipperdevices/flipperzero-ufbt
- Momentum directory.json: https://up.momentum-fw.dev/firmware/directory.json
- Momentum source: https://github.com/Next-Flip/Momentum-Firmware
- App manifest reference: https://developer.flipper.net/flipperzero/doxygen/app_manifests.html
