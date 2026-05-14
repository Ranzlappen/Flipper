# Universal Remote

Map your Flipper's hardware buttons to saved Sub-GHz `.sub` files or named
Infrared signals from `.ir` files. UP / DOWN / LEFT / RIGHT / OK are each
configurable; BACK exits the app.

## Install

Build with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) (or
grab the `.fap` from a [release](https://github.com/Ranzlappen/Flipper/releases)),
then copy `universal_remote.fap` to your SD card:

```
/ext/apps/Tools/universal_remote.fap
```

## Configure

On first run the app creates a commented default at

```
/ext/apps_data/universal_remote/config.txt
```

Edit it on your phone, computer, or directly through qFlipper / Mobile App.
Format:

```
BUTTON=KIND:PATH[,SIGNAL_NAME]
```

- `BUTTON` — one of `UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`. `BACK` is reserved.
- `KIND` — `subghz` or `ir`.
- `PATH` — absolute SD path to a `.sub` (Sub-GHz) or `.ir` (Infrared) file.
- `SIGNAL_NAME` — required for `ir`; the name of the signal inside the
  `.ir` file (as it appears in the stock Infrared app's remote list).

Lines starting with `#` and blank lines are ignored.

### Example

```
UP=subghz:/ext/subghz/garage_open.sub
DOWN=subghz:/ext/subghz/garage_close.sub
LEFT=ir:/ext/infrared/tv.ir,Vol_dn
RIGHT=ir:/ext/infrared/tv.ir,Vol_up
OK=ir:/ext/infrared/tv.ir,Power
```

## Recording the signals

You need to capture the signals with Flipper's built-in apps first:

- **Sub-GHz** — `Sub-GHz → Read → press your remote → Save`. The file lands
  in `/ext/subghz/<name>.sub`.
- **Infrared** — `Infrared → Learn New Remote → Add Button → press your
  remote → Save`. The `.ir` file lands in `/ext/infrared/<name>.ir`. Each
  button you record becomes a named signal inside that file; the name you
  give it is what goes into `SIGNAL_NAME` above.

## Limits & gotchas

- **No long-press mapping yet** — only short presses fire actions. Add an
  issue if you want LONG\_OK etc.
- **Keeloq protocols** need the firmware-shipped keystore at
  `/ext/subghz/assets/keeloq_mfcodes`. Stock Momentum installs include it;
  the app calls `subghz_environment_load_keystore` so the keys are picked
  up automatically.
- **Frequency legality** depends on your region setting — if a `.sub`
  refuses to transmit, check `Settings → Sub-GHz → Region`.
- **The app blocks during transmission** (typically tens of ms to a couple
  of seconds for long Sub-GHz frames). The status line at the bottom
  shows `TX…` / `OK` / `FAILED` / `unbound`.

## Source layout

```
universal-remote/
  application.fam          FAM manifest (appid=universal_remote)
  universal_remote.c       Entry point + view + input/event wiring
  universal_remote_config.{c,h}    config.txt parser
  universal_remote_transmit.{c,h}  Sub-GHz + IR transmitters
  README.md
```
