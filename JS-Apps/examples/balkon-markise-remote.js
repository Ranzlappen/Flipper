// =============================================================================
//  Momentum App Framework  -  balkon-markise-remote.js   (Awning Remote)
// -----------------------------------------------------------------------------
//  A polished, ready-to-use 3-button Sub-GHz remote for a balcony awning
//  ("Balkon-Markise" in German). Buttons:
//
//    REIN   -> retract / pull in
//    STOP   -> stop the motor
//    RAUS   -> extend / push out
//
//  This example loads pre-recorded .sub files from /ext/subghz/ and transmits
//  them when the user picks a button. After each transmission, the menu re-
//  appears so you can press another button - just like a physical remote.
//
//  ---------------------------------------------------------------------------
//  ONE-TIME SETUP
//  ---------------------------------------------------------------------------
//   1. On your Flipper:  Sub-GHz -> Read
//   2. Press the REIN button on your physical remote -> wait for capture -> Save as
//        Balkon_markise_rein
//   3. Repeat for RAUS  ->  Balkon_markise_raus
//   4. Repeat for STOP  ->  Balkon_markise_stop
//   5. Verify the three .sub files now exist in /ext/subghz/.
//   6. Copy this script to /ext/apps/Scripts/balkon-markise-remote.js
//   7. Run from Apps -> Scripts.
//
//  Tested on Momentum firmware (release channel).
// =============================================================================

let subghz    = require("subghz");
let submenu   = require("gui/submenu");
let dialog    = require("gui/dialog");
let notify    = require("notification");
let eventLoop = require("event_loop");
let storage   = require("storage");

// --- CONFIG -------------------------------------------------------------------
let TITLE = "Balkon Markise";

let BUTTONS = [
    { label: "REIN  (in)",   file: "/ext/subghz/Balkon_markise_rein.sub"  },
    { label: "STOP",         file: "/ext/subghz/Balkon_markise_stop.sub"  },
    { label: "RAUS  (out)",  file: "/ext/subghz/Balkon_markise_raus.sub"  },
];

// Optional: how long to hold the button down (transmit duration in ms). Most
// rolling-shutter / awning remotes need a single quick send. Some need the
// signal repeated - tweak below if your motor ignores short bursts.
let REPEAT_COUNT = 1;       // number of times to send each press
let REPEAT_GAP   = 80;      // ms between repeats

// ------------------------------------------------------------------------------

function fileExists(path) {
    try { return storage.exists(path); }
    catch (_) { return false; }
}

function ensureFiles() {
    let missing = [];
    for (let i = 0; i < BUTTONS.length; i++) {
        if (!fileExists(BUTTONS[i].file)) missing.push(BUTTONS[i].file);
    }
    if (missing.length > 0) {
        dialog.message({
            header: "Missing .sub files",
            text:   "Please record:\n" + missing.join("\n"),
            center: "OK",
        });
        return false;
    }
    return true;
}

function transmit(file) {
    print("TX:", file);
    try {
        for (let i = 0; i < REPEAT_COUNT; i++) {
            subghz.transmitFile(file);
            if (i < REPEAT_COUNT - 1) delay(REPEAT_GAP);
        }
        notify.success();
    } catch (e) {
        notify.error();
        dialog.message({
            header: "TX failed",
            text:   String(e),
            center: "OK",
        });
    }
}

// --- main ---------------------------------------------------------------------
if (!ensureFiles()) {
    print("Aborting: missing files.");
} else {
    let view = submenu.makeView({
        header: TITLE,
        items:  BUTTONS.map(function (b) { return b.label; }),
    });

    eventLoop.subscribe(view.chosen, function (_sub, index) {
        transmit(BUTTONS[index].file);
        // returns to the menu automatically because we don't stop the loop
    });

    eventLoop.subscribe(view.exit, function () { eventLoop.stop(); });

    eventLoop.run(view);
    print("Balkon-Markise remote closed.");
}
