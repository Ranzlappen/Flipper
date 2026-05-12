# Momentum JS API - Quick Reference

A curated reference for the most useful JavaScript modules exposed by [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware), cross-checked against the official typings in [`@next-flip/fz-sdk-mntm`](https://www.npmjs.com/package/@next-flip/fz-sdk-mntm) **v1.0**.

> **Authoritative source:** the `.d.ts` files installed under `JS-Apps/node_modules/@next-flip/fz-sdk-mntm/` after `npm install`. When in doubt, read those.

---

## Runtime caveats (read these first)

The JS engine on the Flipper is **mJS** (with Momentum extensions). It is **not** Node, not V8, not even ES5 in full.

| Feature | Status |
|---|---|
| `let` / `const` | works |
| Arrow functions `() => {}` | **not supported** - use `function () {}` |
| Closures (functions referencing outer vars) | **not supported** |
| `import` / `export` | not supported at runtime - use `require()` |
| `Promise` / `async` / `await` | not supported - use the event loop |
| `JSON.parse` / `JSON.stringify` | available |
| `try` / `catch` | works |

**Closure workaround:** state is passed to callbacks via the **extra args** of `eventLoop.subscribe(contract, callback, ...args)`. The callback receives them after `(subscription, item)`. To update state between fires, return a new array of the same length; return `undefined` to keep the previous args.

---

## Globals (no `require` needed)

| Symbol | Signature | Notes |
|---|---|---|
| `print(...args)` | `(...args: any[]) => void` | Console log, visible in CLI / Mobile App. |
| `delay(ms)` | `(ms: number) => void` | Blocks. |
| `require(name)` | `(name: string) => any` | Module loader. |
| `load(path, scope?)` | `(string, object?) => any` | Eval another JS file. |
| `die(msg)` | `(string) => never` | Throws and terminates. |
| `parseInt(text, base?)` | `(string, number?) => number` | Like JS but local. |
| `chr(n)` | `(number) => string \| null` | Codepoint -> 1-char string. |
| `__filename`, `__dirname` | `string` | Script paths. |
| `console` | `{ log, warn, error, debug }` | Thin wrappers around `print`. |
| Typed arrays | `Uint8Array`, `Int16Array`, … | Constructed as `Uint8Array(buf)`, no `new`. |
| `ArrayBuffer` | class | Same - no `new`. |
| `checkSdkCompatibility(major, minor)` | hard-fail if mismatched | Use at top of feature-using scripts. |
| `isSdkCompatible(major, minor)` | returns bool | Soft check. |
| `checkSdkFeatures(features[])` | hard-fail | E.g. `["subghz", "gui-widget"]`. |
| `doesSdkSupport(features[])` | returns bool | Soft check. |

**Not in the runtime** (so don't use): `to_string`, `to_hex_string`, `setTimeout`, `setInterval`, `Math.*` (use the `math` module instead).

---

## `flipper` - device info

```js
let flipper = require("flipper");
print(flipper.getModel());          // "Flipper Zero"
print(flipper.getName());           // user-assigned dolphin name
print(flipper.getBatteryCharge());  // 0..100
print(flipper.firmwareVendor);      // "momentum" or "flipperdevices"
print(flipper.jsSdkVersion);        // [major, minor]
```

---

## `subghz` - radio transceiver

```js
let subghz = require("subghz");

subghz.setup();                                      // optional: explicit init
subghz.transmitFile("/ext/subghz/garage.sub");       // most common use
subghz.transmitFile("/ext/subghz/garage.sub", 3);    // built-in repeat

subghz.setFrequency(433920000);   // returns effective freq (radio may snap to closest)
print(subghz.getFrequency());
print(subghz.getState());         // "RX" | "TX" | "IDLE" | ""
print(subghz.isExternal());

subghz.setRx();                   // listen
print(subghz.getRssi());          // number | undefined

subghz.setIdle();
subghz.end();                     // teardown
```

> There is **no** `setTx()` and **no** `getRegion()` - those exist in the firmware but are not exposed to JS.

---

## `event_loop` - the heartbeat

```js
let eventLoop = require("event_loop");

// timer (one-shot or periodic):
let t = eventLoop.timer("oneshot", 1000);
eventLoop.subscribe(t, function (_sub, _item, loop) {
    print("tick");
    loop.stop();
}, eventLoop);

eventLoop.run();   // blocks until .stop()
```

Periodic counter pattern (state via extra args, no closures):

```js
let t = eventLoop.timer("periodic", 1000);
eventLoop.subscribe(t, function (_sub, _item, count, loop) {
    print("count =", count);
    if (count >= 5) loop.stop();
    return [count + 1, loop];   // updated args for next fire
}, 0, eventLoop);
eventLoop.run();
```

---

## `gui` + view modules - the right pattern

The GUI uses a **ViewDispatcher**. You build views with a `ViewFactory`, register them implicitly via `makeWith`, switch between them, and handle BACK via `gui.viewDispatcher.navigation`.

```js
let eventLoop = require("event_loop");
let gui       = require("gui");
let submenu   = require("gui/submenu");

let view = submenu.makeWith({ header: "Pick one" }, ["A", "B", "C"]);

eventLoop.subscribe(view.chosen, function (_sub, index) {
    print("picked", index);
});

eventLoop.subscribe(gui.viewDispatcher.navigation, function (_sub, _, loop) {
    loop.stop();
}, eventLoop);

gui.viewDispatcher.switchTo(view);
eventLoop.run();
```

### Available views

| Module | Factory | Event(s) | Useful props |
|---|---|---|---|
| `gui/submenu` | `.makeWith({header}, items)` | `view.chosen: number` | `header` |
| `gui/dialog` | `.makeWith({header, text, left, center, right})` | `view.input: "left"\|"center"\|"right"` | text + 3 buttons |
| `gui/text_input` | `.makeWith({header, minLength, maxLength, defaultText, defaultTextClear, illegalSymbols})` | `view.input: string` | on-screen keyboard |
| `gui/byte_input` | `.makeWith({header, length, defaultData})` | `view.input: Uint8Array` | hex editor |
| `gui/number_input` | `.makeWith({header, min, max, defaultValue})` | `view.input: number` | integer picker |
| `gui/menu` | `.makeWith({header}, items)` | `view.chosen: number` | classic menu |
| `gui/button_menu` | `.makeWith({header}, items)` | `view.chosen: number` | grid of big buttons |
| `gui/button_panel` | `.makeWith({})` | custom | freely-placed buttons |
| `gui/text_box` | `.makeWith({text, focus})` | (read-only) | scrollable text |
| `gui/popup` | `.makeWith({header, text, timeout})` | `view.timeout` | timed pop-up |
| `gui/loading` | `.make()` | none | hourglass screen |
| `gui/empty_screen` | `.make()` | none | blank |
| `gui/file_picker` | see typings | `view.chosen` | filesystem browser |
| `gui/widget` | composable | varied | low-level drawing |

### `gui.viewDispatcher`

| Member | Type | Purpose |
|---|---|---|
| `.switchTo(view)` | function | Show a view |
| `.currentView` | View | Read which view is shown |
| `.navigation` | `Contract` | BACK key event |
| `.custom` | `Contract<number>` | App-defined event |
| `.sendCustom(n)` | function | Fire a custom event |
| `.sendTo("front"\|"back")` | function | Z-order |

---

## `notification` - LED + buzzer + vibration

```js
let notify = require("notification");
notify.success();                       // green LED + happy beep + vibrate
notify.error();                         // red LED + sad beep + vibrate
notify.blink("red", "short");           // red | green | blue | yellow | cyan | magenta
notify.blink("blue", "long");           // duration: "short" (10 ms) | "long" (100 ms)
```

> No standalone `vibrate(ms)` is exposed. Use `success()` / `error()` for haptics.

---

## `storage` - filesystem

Quick existence checks:

```js
let storage = require("storage");
storage.fileExists("/ext/subghz/x.sub");
storage.directoryExists("/ext/apps/Scripts");
storage.fileOrDirExists("/ext/apps");
```

Directory listing:

```js
let entries = storage.readDirectory("/ext/apps/Scripts");  // FileInfo[] | undefined
for (let i = 0; i < entries.length; i++) {
    print(entries[i].path, entries[i].size, entries[i].isDirectory);
}
```

Reading and writing files:

```js
let f = storage.openFile("/ext/log.txt", "rw", "open_append");
if (f) {
    f.write("hello\n");
    f.seekAbsolute(0);
    let s = f.read("ascii", 1024);   // or "binary" -> ArrayBuffer
    print(s);
    f.close();
}
```

Other handy ops:

```js
storage.makeDirectory("/ext/apps/MyApp");
storage.remove("/ext/old.sub");
storage.rmrf("/ext/dump_dir");          // recursive
storage.rename("/ext/a.txt", "/ext/b.txt");
storage.copy("/ext/a.txt", "/ext/b.txt");
storage.stat("/ext/foo");               // FileInfo | undefined
storage.fsInfo("/ext");                 // { totalSpace, freeSpace }
storage.nextAvailableFilename("/ext/dump", "capture_", ".bin", 32);
```

---

## `badusb` - HID keyboard

```js
let badusb = require("badusb");
badusb.setup({ vid: 0x046D, pid: 0xC31C, mfrName: "Logi", prodName: "K120" });
while (!badusb.isConnected()) delay(50);

badusb.println("hello world");
badusb.press("F4", "ALT");          // Alt+F4
badusb.press("L", "GUI");           // Win+L (lock)
badusb.hold("SHIFT");
badusb.press("A");
badusb.release("SHIFT");
badusb.altPrintln("emojis via alt-numpad");
badusb.quit();
```

> The API is `press(...keys)` / `hold(...keys)` / `release(...keys)` taking any number of keys with at most one "main" key plus modifiers. There is **no** `altPress` for individual keys (use `press("F4","ALT")`); `altPrint`/`altPrintln` are for typing strings via the Windows Alt+Numpad method.

---

## `gpio`

```js
let gpio = require("gpio");
let pin = gpio.get("PA7");
pin.init({ direction: "out", outMode: "push_pull" });
pin.write(true);
delay(500);
pin.write(false);
```

---

## `usbdisk` (Momentum)

Expose a folder to the host PC as USB Mass Storage.

```js
let usbdisk = require("usbdisk");
usbdisk.start("/ext/usb_share");
delay(60000);
usbdisk.stop();
```

---

## `blebeacon` (Momentum)

```js
let beacon = require("blebeacon");
beacon.setData([0x02, 0x01, 0x06, 0x03, 0x03, 0xAA, 0xFE]);
beacon.start();
delay(10000);
beacon.stop();
```

> The module folder is `blebeacon`, not `ble/beacon`.

---

## `serial` (UART)

```js
let serial = require("serial");
serial.setup("usart", 115200);
serial.write("AT\r\n");
let resp = serial.expect("OK", 1000);
serial.end();
```

---

## `math`, `i2c`, `spi`, `vgm`

Available - see the matching folder under `node_modules/@next-flip/fz-sdk-mntm/` for typings. `math` is required (the standard JS `Math.*` isn't exposed globally).

---

## Tips

- **No closures** - always pass state via `subscribe()` extras.
- **No `arrow fns`** - use `function () { ... }`.
- **Memory is tight** (~32 KB heap). Avoid huge concatenations.
- **Errors bubble up** - wrap risky calls in `try { ... } catch (e) { print(e); }`.
- **Compatibility checks** - call `checkSdkFeatures(["subghz"])` near the top of feature-using scripts so users on stock firmware get a clear message.

---

## Where to go next

- Full module docs: https://developer.flipper.net/flipperzero/doxygen/js.html
- Real-world examples: https://github.com/Next-Flip/Momentum-Apps
- SDK source: https://github.com/Next-Flip/Momentum-Firmware (under `applications/system/js_app/`)
