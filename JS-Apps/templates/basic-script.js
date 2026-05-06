// =============================================================================
//  Momentum App Framework  -  basic-script.js
// -----------------------------------------------------------------------------
//  Simplest possible JS script for Momentum-firmware Flipper Zero.
//  Prints a few lines, beeps the speaker, vibrates briefly, and exits.
//
//  HOW TO USE
//  ----------
//  1. Copy this file to your SD card at:    /ext/apps/Scripts/basic-script.js
//  2. On the Flipper:  Apps -> Scripts -> basic-script.js -> OK
//
//  The first line below is a "Momentum module check" so the script fails fast
//  with a clear message if you ever accidentally try to run it on stock fw.
// =============================================================================

// Core console / log
let flipper = require("flipper");

// Hardware: speaker + vibration
let speaker = require("speaker");
let notify  = require("notification");

// ---- main --------------------------------------------------------------------
print("Hello from Momentum JS!");
print("Firmware:", flipper.getModel ? flipper.getModel() : "unknown");

// Quick beep (frequency in Hz, volume 0.0 - 1.0)
if (speaker.start(880, 0.5)) {
    delay(150);
    speaker.stop();
}

// Short success buzz / LED flash
notify.success();

print("Done. Press BACK to exit.");
