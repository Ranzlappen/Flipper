# Deployment Guide

How to get your scripts and `.fap` apps onto the Flipper Zero.

## SD card layout

```
SD card root (called /ext on the Flipper)
├── apps/
│   ├── Scripts/        <- ALL .js scripts here
│   ├── Examples/       <- .fap apps (any category folder works)
│   ├── Tools/
│   └── ...
├── subghz/             <- recorded .sub files
├── nfc/                <- saved NFC tags
├── badusb/             <- BadUSB scripts
└── ...
```

The Flipper UI groups apps by their **`fap_category`** (set in `application.fam`) - the folder name on the SD card just needs to match. JS scripts always live under `apps/Scripts/`.

---

## Method 1: Flipper Mobile App (phone-only)

**Best for: JS scripts, small files, no computer.**

1. Install the [Flipper Mobile App](https://flipperzero.one/app) (iOS / Android).
2. Pair your Flipper via Bluetooth.
3. In the app, open **File Manager** (sometimes called "Storage" or "Browse").
4. Navigate: `SD Card -> apps -> Scripts/`. If `Scripts` does not exist, tap "+" and create it.
5. Tap the upload button -> choose your `.js` file from your phone's local files / Downloads.
6. On the Flipper: **Apps -> Scripts -> [your file] -> OK**.

> **iOS tip:** download files via Safari first - they show up in the Files app, which the Flipper Mobile App can then access. GitHub's "Raw" -> "Download Linked File" works.

> **Android tip:** any text editor (Acode, Quoda, Markor) can save directly into a folder the Flipper app can see.

---

## Method 2: qFlipper (desktop)

**Best for: `.fap` apps, bulk uploads.**

1. Install [qFlipper](https://flipperzero.one/update).
2. Connect via USB or Bluetooth.
3. Open the **File Manager** tab.
4. Drag and drop your files into the right folder.

---

## Method 3: SD card reader

**Best for: copying many files at once, recovery.**

1. Power off the Flipper. Eject the microSD card.
2. Insert it into a reader connected to your phone (USB-C/Lightning) or computer.
3. Copy files using the OS file manager.
4. Eject safely. Reinsert into Flipper. Power on.

---

## Method 4: uFBT (C apps only)

**Best for: dev loop while writing C apps.**

```bash
cd C-Apps/templates/hello-world
ufbt launch                # build + upload + run in one shot
# or:
ufbt                       # just build (output: dist/<appid>.fap)
```

`ufbt launch` requires the Flipper connected via USB.

---

## Method 5: GitHub Releases (this repo's CI)

Every push to `main` triggers `.github/workflows/build-and-release.yml`, which:
1. Builds all C apps under `C-Apps/templates/` and `C-Apps/` with uFBT against Momentum.
2. Bumps the patch version (e.g. `v0.1.0` -> `v0.1.1`).
3. Creates a GitHub Release.
4. Attaches:
   - all compiled `.fap` files
   - all `.js` scripts (from `JS-Apps/templates/` and `JS-Apps/examples/`)
   - a single zipped bundle for easy phone download

Just download the assets straight from the Releases page on your phone, then upload via the Mobile App. No build tools required on your end.

---

## Verifying a successful deploy

After copying:

| App type | How to confirm |
|---|---|
| `.js` script | Visible at `Apps -> Scripts`. Run it - check log via `Mobile App -> Console` or `qFlipper -> Logs`. |
| `.fap` app | Visible at `Apps -> <Category>`. If missing, the SDK version mismatched - rebuild against Momentum. |
| `.sub` file | `Sub-GHz -> Saved -> [filename]`. |

---

## Common deploy issues

| Symptom | Cause | Fix |
|---|---|---|
| Script not in menu | Wrong folder | Must be in `/ext/apps/Scripts/` (not `/ext/scripts/`) |
| `.fap` shows "Invalid app" | Built for stock fw | Re-bootstrap uFBT for Momentum (see `C-Apps/ufbt-config.md`) |
| `.fap` shows "API mismatch" | Momentum updated | `ufbt update` + rebuild |
| Mobile App won't show folder | Cached | Pull-to-refresh in File Manager |
| BLE upload too slow | BT bandwidth | Use SD card reader for large files |
