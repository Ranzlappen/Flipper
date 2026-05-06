// =============================================================================
//  Momentum App Framework  -  subghz-remote-template.js
// -----------------------------------------------------------------------------
//  A reusable multi-button Sub-GHz remote.
//  - Shows a submenu of buttons (e.g. UP / STOP / DOWN).
//  - When a button is pressed, transmits the matching .sub file from /ext/subghz/.
//  - Loops back to the menu after each transmission.
//
//  HOW TO ADAPT
//  ------------
//  1. Record your remote signals on the Flipper:
//        Sub-GHz -> Read -> press your physical remote button -> Save
//     Save them to /ext/subghz/ with descriptive names, e.g.:
//        garage_open.sub, garage_close.sub, garage_stop.sub
//  2. Edit the BUTTONS array below: change `label` and `file` to match.
//  3. Save this file as `<your_remote>.js` in /ext/apps/Scripts/.
//  4. Run from Apps -> Scripts.
//
//  REQUIRES: Momentum firmware (uses the Momentum-enhanced `subghz` module).
// =============================================================================

let subghz   = require("subghz");
let submenu  = require("gui/submenu");
let dialog   = require("gui/dialog");
let notify   = require("notification");
let eventLoop = require("event_loop");

// ---- CONFIG: edit these to match YOUR .sub files ---------------------------
let TITLE    = "My Sub-GHz Remote";
let HEADER   = "Pick a button";

let BUTTONS = [
    { label: "UP",   file: "/ext/subghz/garage_open.sub"  },
    { label: "STOP", file: "/ext/subghz/garage_stop.sub"  },
    { label: "DOWN", file: "/ext/subghz/garage_close.sub" },
];
// ---------------------------------------------------------------------------

// Build the submenu view.
let view = submenu.makeView({
    header: HEADER,
    items:  BUTTONS.map(function (b) { return b.label; }),
});

// Transmit one .sub file and notify on success/failure.
function sendFile(path) {
    print("TX:", path);
    try {
        subghz.transmitFile(path);   // Momentum: transmits the recorded file
        notify.success();
        showInfo("Sent", path);
    } catch (e) {
        notify.error();
        showInfo("Failed", String(e));
    }
}

// Tiny modal info dialog.
function showInfo(title, text) {
    dialog.message({
        header:  title,
        text:    text,
        center:  "OK",
    });
}

// Wire up: when an item is selected, send the matching file.
eventLoop.subscribe(view.chosen, function (_sub, index) {
    sendFile(BUTTONS[index].file);
});

// Show the menu, run the loop until BACK is pressed.
eventLoop.subscribe(view.exit, function () {
    eventLoop.stop();
});

submenu.setHeader(view, TITLE);
eventLoop.run(view);

print("Remote exited cleanly.");
