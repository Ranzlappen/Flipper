# Universal Remote

Map every Flipper hardware button to a saved Sub-GHz `.sub` file or a named
signal from an Infrared `.ir` file. All six buttons (UP / DOWN / LEFT / RIGHT
/ OK / BACK) are mappable.

## Gestures

| Gesture | Effect |
|---|---|
| Short press (any button) | Fire the binding for that button |
| **Hold OK** | Open the on-device editor |
| **Hold BACK** | Exit the app |

Short-press BACK fires its binding just like the other buttons — only the
long-press exits.

> **Note:** hold-BACK only exits from the **main view**. Inside the editor
> (any submenu), short-BACK saves and returns you to the main view; from
> there, hold-BACK exits the app. This is a Flipper submenu limitation —
> long-press doesn't propagate to the navigation handler — not a bug.

## On-device editor

Hold OK from the main view. You'll see a submenu of all six buttons with
their current bindings:

```
UP:   SG garage_open.sub
DOWN: IR tv.ir/Vol_dn
LEFT: -
…
[Save & exit]
[Discard & exit]
```

Selecting a button asks for the action kind:

- **Sub-GHz file (.sub)** — opens the Sub-GHz file browser at `/ext/subghz`.
  Pick a file; the binding is updated.
- **Infrared signal (.ir)** — opens the Infrared file browser at
  `/ext/infrared`, then shows a submenu of every signal name inside the
  chosen file. Pick a signal; the binding is updated.
- **Clear binding** — removes the binding entirely.

Short-press BACK from the edit root menu saves changes and exits the
editor. Use **Discard & exit** if you want to abandon the session.

Saving rewrites `/ext/apps_data/universal_remote/config.txt`. Hand-written
comments in that file are not preserved across saves.

## Install

Build with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) (or
grab the `.fap` from a [release](https://github.com/Ranzlappen/Flipper/releases)),
then copy `universal_remote.fap` to your SD card:

```
/ext/apps/Tools/universal_remote.fap
```

## Manual config (optional)

The editor covers the common case. If you'd rather edit the file directly,
the format is:

```
BUTTON=KIND:PATH[,SIGNAL_NAME]
```

- `BUTTON` — `UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, or `BACK`
- `KIND` — `subghz` or `ir`
- `PATH` — absolute SD path to a `.sub` or `.ir` file
- `SIGNAL_NAME` — required for `ir`; the name of the signal inside the
  `.ir` file (as it appears in the stock Infrared app's remote)

Empty value (`UP=`) means "unbound." Lines starting with `#` are ignored.

### Example

```
UP=subghz:/ext/subghz/garage_open.sub
DOWN=subghz:/ext/subghz/garage_close.sub
LEFT=ir:/ext/infrared/tv.ir,Vol_dn
RIGHT=ir:/ext/infrared/tv.ir,Vol_up
OK=ir:/ext/infrared/tv.ir,Power
BACK=subghz:/ext/subghz/garage_stop.sub
```

## Recording the signals

You need to capture the signals with Flipper's built-in apps first:

- **Sub-GHz** — `Sub-GHz → Read → press your remote → Save`. The file lands
  in `/ext/subghz/<name>.sub`.
- **Infrared** — `Infrared → Learn New Remote → Add Button → press your
  remote → Save`. The `.ir` file lands in `/ext/infrared/<name>.ir`. Each
  button you record becomes a named signal inside that file.

## Limits & gotchas

- **Short-press only.** Long-press is reserved for OK (edit) and BACK
  (exit). Long-press dispatch on other buttons is a future enhancement.
- **The first 32 signals** in an `.ir` file are shown in the signal
  picker. Files with more signals fall back to manual config editing.
- **Keeloq protocols** need the firmware-shipped keystore at
  `/ext/subghz/assets/keeloq_mfcodes`. Stock Momentum installs include it;
  the app loads it automatically.
- **Frequency legality** depends on your region setting — if a `.sub`
  refuses to transmit, check `Settings → Sub-GHz → Region`.
- **The app blocks during transmission** (typically tens of ms to a couple
  of seconds for long Sub-GHz frames). The status line at the bottom of
  the main view shows `TX…` / `OK` / `FAILED` / `unbound`.

## Source layout

```
universal-remote/
  application.fam                  FAM manifest (appid=universal_remote)
  universal_remote.c               Entry point + main view + edit submenus
  universal_remote_config.{c,h}    config.txt parser + serializer
  universal_remote_transmit.{c,h}  Sub-GHz + IR transmitters
  README.md
```
