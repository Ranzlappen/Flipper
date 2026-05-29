# Universal Remote

Multi-profile remote control for the Flipper Zero. Each *remote* is a saved
profile that maps the six hardware buttons (UP / DOWN / LEFT / RIGHT / OK /
BACK) to a saved Sub-GHz `.sub` file or a named signal from an Infrared `.ir`
file. The four directional buttons additionally support a separately-mappable
**long-press** binding.

You can keep as many remotes as you like and switch between them from a
single menu — e.g. one for the garage, one for the awning, one for the TV.

## Gestures

| Gesture | Effect |
|---|---|
| Short press UP / DOWN / LEFT / RIGHT | Fire that direction's SHORT binding |
| **Long press UP / DOWN / LEFT / RIGHT** | Fire that direction's LONG binding |
| Short press OK | Fire OK's binding |
| Short press BACK | Fire BACK's binding (returns to the remote list if BACK is unbound) |
| **Hold OK** | Open the editor for the current remote |
| **Hold BACK** | Return to the remote list |

From the remote list, short-BACK exits the app. The list also contains a
`[+ New remote]` entry; selecting it opens a name prompt and creates an empty
profile.

## Remote view

The active remote's view shows the four D-pad bindings (both short and long)
in a compact two-column layout, the OK and BACK bindings underneath, and a
status line at the bottom that reports the last press:

```
┌──────────────────────────────────────────┐
│ Garage Door                              │
├──────────────────────────────────────────┤
│  ▲  S: open        L: full               │
│  ▼  S: close       L: -                  │
│  ◀  S: -           L: light_toggle       │
│  ▶  S: tv_vol      L: -                  │
├──────────────────────────────────────────┤
│ OK: stop          BACK: -                │
│ last: UP long  OK                        │
└──────────────────────────────────────────┘
```

## On-device editor

Hold OK from a remote view. The editor lists every mappable slot:

```
UP short:   SG garage_open.sub
UP long:    SG garage_full.sub
DOWN short: SG garage_close.sub
DOWN long:  -
LEFT short: …
…
OK:         IR tv.ir/Power
BACK:       -
Rename remote
Delete remote
Save & exit
Discard & exit
```

Selecting a binding slot asks for the action kind:

- **Sub-GHz file (.sub)** — opens the Sub-GHz file browser at `/ext/subghz`.
  Pick a file; the binding is updated.
- **Infrared signal (.ir)** — opens the Infrared file browser at
  `/ext/infrared`, then shows a submenu of every signal name inside the
  chosen file. Pick a signal; the binding is updated.
- **Clear binding** — removes the binding entirely.

**Rename** and **Delete** operate on the on-disk file; deleting prompts for
confirmation. Short-BACK from the edit root saves changes and exits. Use
**Discard & exit** if you want to abandon the session.

## Install

Build with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) (or
grab the `.fap` from a [release](https://github.com/Ranzlappen/Flipper/releases)),
then copy `universal_remote.fap` to your SD card:

```
/ext/apps/Tools/universal_remote.fap
```

## Storage layout

```
/ext/apps_data/universal_remote/
  remotes/
    Default.urcfg
    Garage.urcfg
    …
  config.txt.bak     (only present if the pre-0.2 single-config was migrated)
```

On first run after upgrading from 0.1, the app auto-migrates the old
single-profile `config.txt` into `remotes/Default.urcfg` and renames the
original to `config.txt.bak` so you can recover from it if needed.

## File format (`<name>.urcfg`)

```
NAME=Garage Door
UP_SHORT=subghz:/ext/subghz/garage_open.sub
UP_LONG=subghz:/ext/subghz/garage_full.sub
DOWN_SHORT=subghz:/ext/subghz/garage_close.sub
DOWN_LONG=
LEFT_SHORT=ir:/ext/infrared/tv.ir,Vol_dn
LEFT_LONG=
RIGHT_SHORT=ir:/ext/infrared/tv.ir,Vol_up
RIGHT_LONG=
OK=ir:/ext/infrared/tv.ir,Power
BACK=
```

- `NAME` — the display name shown in the remote list and titlebar (defaults
  to the filename without the `.urcfg` extension if missing).
- `<BUTTON>_SHORT` / `<BUTTON>_LONG` — D-pad bindings. For backwards
  compatibility, a bare `UP=…` (no suffix) is read as `UP_SHORT=…`.
- `OK` / `BACK` — short-press only. Short-OK / short-BACK fire their bindings;
  hold-OK opens the editor and hold-BACK returns to the list. (When `BACK` is
  empty, short-BACK falls back to returning to the list.)
- `KIND` is either `subghz` or `ir`.
- For `ir`, the value is `PATH,SIGNAL_NAME` — both required.
- Empty value (`UP_LONG=`) means "unbound."
- Lines starting with `#` are ignored. Comments are *not* preserved across
  on-device edits.

## Recording the signals

Capture the signals with Flipper's built-in apps first:

- **Sub-GHz** — `Sub-GHz → Read → press your remote → Save`. The file lands
  in `/ext/subghz/<name>.sub`.
- **Infrared** — `Infrared → Learn New Remote → Add Button → press your
  remote → Save`. The `.ir` file lands in `/ext/infrared/<name>.ir`. Each
  button you record becomes a named signal inside that file.

## Limits & gotchas

- **The first 64 signals** in an `.ir` file are shown in the signal picker.
  Files with more signals fall back to manual config editing.
- **Keeloq protocols** need the firmware-shipped keystore at
  `/ext/subghz/assets/keeloq_mfcodes`. Stock Momentum installs include it;
  the app loads it automatically.
- **Frequency legality** depends on your region setting — if a `.sub`
  refuses to transmit, check `Settings → Sub-GHz → Region`.
- **The app blocks during transmission** (typically tens of ms to a couple
  of seconds for long Sub-GHz frames). The status line at the bottom of
  the remote view shows the last press and its outcome.

## Source layout

```
universal-remote/
  application.fam                  FAM manifest (appid=universal_remote)
  icon.png                         10×10 1-bit launcher icon
  universal_remote.c               Entry point + remote list + D-pad view +
                                   edit submenus
  universal_remote_config.{c,h}    .urcfg parser, multi-remote index,
                                   migration of pre-0.2 config.txt
  universal_remote_transmit.{c,h}  Sub-GHz + IR transmitters
  README.md
```
