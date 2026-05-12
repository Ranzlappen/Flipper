// =============================================================================
//  Momentum App Framework  -  subghz-remote-template.js
// -----------------------------------------------------------------------------
//  Generic multi-button Sub-GHz remote.
//  - Shows a submenu of buttons.
//  - When a button is pressed, transmits the matching .sub file from /ext/subghz/.
//  - Returns to the menu after each transmission. BACK exits.
//
//  HOW TO ADAPT
//  ------------
//  1. Record your signals: Sub-GHz -> Read -> press your physical remote
//     button -> Save with a memorable name (e.g. garage_open).
//  2. Edit the BUTTONS array below.
//  3. Save as <your_remote>.js into /ext/apps/Scripts/.
//  4. Run from Apps -> Scripts.
//
//  REQUIRES: Momentum firmware (uses the Momentum-enhanced `subghz` module).
//  No closures: the JS engine is mJS - state passes via subscribe() args.
// =============================================================================

let eventLoop = require("event_loop");
let gui       = require("gui");
let submenu   = require("gui/submenu");
let subghz    = require("subghz");
let notify    = require("notification");

// ---- CONFIG: edit these to match YOUR .sub files ----------------------------
let TITLE   = "My Sub-GHz Remote";
let HEADER  = "Pick a button";

let BUTTONS = [
    { label: "UP",   file: "/ext/subghz/garage_open.sub"  },
    { label: "STOP", file: "/ext/subghz/garage_stop.sub"  },
    { label: "DOWN", file: "/ext/subghz/garage_close.sub" },
];

// How many times each press retransmits. Most remotes need 1.
let REPEAT = 1;
// -----------------------------------------------------------------------------

let labels = [];
for (let i = 0; i < BUTTONS.length; i++) labels.push(BUTTONS[i].label);

let view = submenu.makeWith({ header: HEADER }, labels);

eventLoop.subscribe(
    view.chosen,
    function (_sub, index, buttons, repeat) {
        let i = /** @type {number} */ (index);
        let file = buttons[i].file;
        print("TX:", file);
        try {
            subghz.transmitFile(file, repeat);
            notify.success();
        } catch (e) {
            print("TX failed:", e);
            notify.error();
        }
    },
    BUTTONS,
    REPEAT,
);

eventLoop.subscribe(
    gui.viewDispatcher.navigation,
    function (_sub, _item, loop) { loop.stop(); },
    eventLoop,
);

gui.viewDispatcher.switchTo(view);
print(TITLE + " ready.");
eventLoop.run();
print("Remote exited cleanly.");
