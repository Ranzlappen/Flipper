# Momentum JS API - Quick Reference

This is a curated reference for the **most useful** JavaScript modules exposed by [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware). Momentum is a superset of the stock Flipper JS API, with many extra modules and methods.

> **Authoritative source:** https://docs.flipper.net/development/js (with Momentum extensions noted in the [Momentum README](https://github.com/Next-Flip/Momentum-Firmware#javascript)).

All modules are loaded with `let mod = require("module_name")`.

---

## Globals

Available everywhere without `require`:

| Function | Description |
|---|---|
| `print(...args)` | Console output (visible in CLI / Mobile App log) |
| `delay(ms)` | Block for `ms` milliseconds |
| `to_string(x)` | Force-convert to string |
| `to_hex_string(num, width)` | Hex-format an integer |
| `__filename`, `__dirname` | Standard path globals |

---

## `flipper`

Device info.

```js
let flipper = require("flipper");
print(flipper.getModel());        // "Flipper Zero"
print(flipper.getName());         // user-assigned device name
print(flipper.getBatteryCharge()); // 0..100
```

---

## `subghz`  (Momentum-enhanced)

Transmit / receive on the Sub-GHz radio.

```js
let subghz = require("subghz");

// Transmit a saved .sub file (most common use):
subghz.transmitFile("/ext/subghz/Balkon_markise_rein.sub");

// Set state explicitly (Momentum):
subghz.setRx();        // listen
subghz.setTx();        // transmit mode
subghz.setIdle();      // power down

// Frequency helpers:
subghz.setFrequency(433920000);
print(subghz.getFrequency());

// Region / regulatory:
print(subghz.getRegion());        // e.g. "EU"
```

> The `transmitFile` call is the easiest way to make a remote: record the signal once with `Sub-GHz -> Read`, then play it back from JS.

---

## `gui/submenu`  (vertical list)

```js
let eventLoop = require("event_loop");
let submenu   = require("gui/submenu");

let view = submenu.makeView({
    header: "Pick one",
    items:  ["Open", "Stop", "Close"],
});

eventLoop.subscribe(view.chosen, function (_sub, idx) {
    print("Picked", idx);
    eventLoop.stop();
});

eventLoop.run(view);
```

---

## `gui/dialog`

```js
let dialog = require("gui/dialog");

let answer = dialog.message({
    header: "Confirm",
    text:   "Are you sure?",
    left:   "No",
    right:  "Yes",
    center: "OK",        // optional middle button
});
// answer is "left" | "right" | "center"
```

---

## `gui/text_input`

```js
let textInput = require("gui/text_input");
let typed = textInput.show({
    header:    "Enter name",
    minLength: 1,
    maxLength: 32,
    default:   "",
});
```

## `gui/byte_input`  (Momentum)

```js
let byteInput = require("gui/byte_input");
let bytes = byteInput.show({
    header: "Edit key",
    bytes:  [0xDE, 0xAD, 0xBE, 0xEF],
});
```

---

## `event_loop`

Used to drive any GUI view.

```js
let eventLoop = require("event_loop");
eventLoop.subscribe(view.chosen, callback);
eventLoop.subscribe(view.exit,   function () { eventLoop.stop(); });
eventLoop.run(view);
```

---

## `storage`

```js
let storage = require("storage");

storage.exists("/ext/subghz/file.sub");          // -> bool
storage.read("/ext/path.txt");                   // -> string
storage.write("/ext/log.txt", "hello\n");
storage.append("/ext/log.txt", "more\n");
storage.list("/ext/apps/Scripts");               // -> [string]
storage.remove("/ext/old.sub");
storage.makeDirectory("/ext/apps/MyApp");
```

---

## `notification`

```js
let notify = require("notification");
notify.success();           // green LED + happy beep
notify.error();             // red LED + error beep
notify.blink("blue", "short");
notify.vibrate(150);        // ms
```

---

## `usbdisk`  (Momentum)

Expose a folder as a USB Mass Storage device to the host PC.

```js
let usbdisk = require("usbdisk");
usbdisk.start("/ext/usb_share");   // read-write share
delay(60000);                      // share for 60 s
usbdisk.stop();
```

---

## `badusb`

```js
let badusb = require("badusb");
badusb.setup({ vid: 0x046D, pid: 0xC31C, mfrName: "Logi", prodName: "K120" });
badusb.println("hello world");
badusb.altPress("F4");
badusb.quit();
```

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

## `ble/beacon`  (Momentum)

```js
let beacon = require("ble/beacon");
beacon.setData([0x02, 0x01, 0x06, 0x03, 0x03, 0xAA, 0xFE]);
beacon.start();
delay(10000);
beacon.stop();
```

---

## `serial`  (UART)

```js
let serial = require("serial");
serial.setup("usart", 115200);
serial.write("AT\r\n");
let resp = serial.expect("OK", 1000);
serial.end();
```

---

## `math`

Standard JS-style: `math.sin`, `math.PI`, `math.random`, etc.

---

## Tips

- **No `import`/`export`** - use `require()`.
- **No async/await** - the runtime is synchronous + event-loop based.
- **Strings, numbers, arrays, objects** all work as expected.
- Errors thrown propagate to the CLI - wrap risky calls in `try { ... } catch (e) { ... }`.
- Memory is tight (~32 KB heap). Avoid huge string concatenations or massive arrays.

---

## Where to go next

- Full module docs: https://docs.flipper.net/development/js
- Momentum-specific extras: see Momentum repo `/applications/system/js_app/modules/`
- Real-world examples: https://github.com/Next-Flip/Momentum-Apps
