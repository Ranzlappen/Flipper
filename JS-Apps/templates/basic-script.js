// =============================================================================
//  Momentum App Framework  -  basic-script.js
// -----------------------------------------------------------------------------
//  Simplest possible JS script for Momentum-firmware Flipper Zero.
//  Prints info, flashes the LED, and exits.
//
//  HOW TO USE
//  ----------
//  1. Copy this file to your SD card at:    /ext/apps/Scripts/basic-script.js
//  2. On the Flipper:  Apps -> Scripts -> basic-script.js -> OK
//
//  Tested against @next-flip/fz-sdk-mntm 1.0.
// =============================================================================

let flipper = require("flipper");
let notify = require("notification");

print("Hello from Momentum JS!");
print("Model:           ", flipper.getModel());
print("Name:            ", flipper.getName());
print("Battery (%):     ", flipper.getBatteryCharge());
print("Firmware vendor: ", flipper.firmwareVendor); // "momentum" on Momentum

// Quick visual + audible feedback: short blue flash, then the standard
// "success" notification (green LED + happy beep + vibration).
notify.blink("blue", "short");
delay(200);
notify.success();

print("Done. Press BACK to exit.");
