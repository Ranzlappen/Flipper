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
//  Loads pre-recorded .sub files from /ext/subghz/ and transmits them when the
//  user picks a button. The menu returns automatically after each press, just
//  like a physical remote. Press BACK to exit.
//
//  ---------------------------------------------------------------------------
//  ONE-TIME SETUP
//  ---------------------------------------------------------------------------
//   1. On your Flipper:  Sub-GHz -> Read
//   2. Press the REIN button on your physical remote -> wait for capture -> Save as
//        Balkon_markise_rein
//   3. Repeat for RAUS  ->  Balkon_markise_raus
//   4. Repeat for STOP  ->  Balkon_markise_stop
//   5. Copy this script to /ext/apps/Scripts/balkon-markise-remote.js
//   6. Run from Apps -> Scripts.
//
//  Tested against @next-flip/fz-sdk-mntm 1.0 on Momentum firmware.
//
//  Notes on the JS engine:
//   - mJS has NO closures, so any state used inside callbacks is passed via
//     `eventLoop.subscribe(contract, cb, ...args)` extra arguments.
//   - GUI uses the ViewDispatcher pattern: build view, switchTo, run loop.
// =============================================================================

let eventLoop = require("event_loop");
let gui = require("gui");
let submenu = require("gui/submenu");
let subghz = require("subghz");
let notify = require("notification");
let storage = require("storage");

// ---------------------------------------------------------------------------
// CONFIG  -  the three buttons and the .sub files they transmit
// ---------------------------------------------------------------------------
let TITLE = "Balkon Markise";

let BUTTONS = [
  { label: "REIN  (in)", file: "/ext/subghz/Balkon_markise_rein.sub" },
  { label: "STOP", file: "/ext/subghz/Balkon_markise_stop.sub" },
  { label: "RAUS  (out)", file: "/ext/subghz/Balkon_markise_raus.sub" },
];

// How many times each press should retransmit the captured signal. Most
// rolling-shutter / awning motors need 1; bump to 2-3 if yours ignores
// single bursts.
let REPEAT = 1;

// ---------------------------------------------------------------------------
// Pre-flight: make sure every .sub file exists, otherwise show an error.
// ---------------------------------------------------------------------------
function findMissingFiles() {
  let missing = [];
  for (let i = 0; i < BUTTONS.length; i++) {
    if (!storage.fileExists(BUTTONS[i].file)) {
      missing.push(BUTTONS[i].file);
    }
  }
  return missing;
}

let missing = findMissingFiles();
if (missing.length > 0) {
  // No SD files yet - print to log so the user sees what to record.
  print("Missing .sub files - please record:");
  for (let i = 0; i < missing.length; i++) print("  -", missing[i]);
  print("Tip: Sub-GHz -> Read -> Save with the expected filename.");
} else {
  // ---------------------------------------------------------------------------
  // Build the submenu view. Items are passed as the second positional arg
  // (they are "children" in the ViewFactory model).
  // ---------------------------------------------------------------------------
  let labels = [];
  for (let i = 0; i < BUTTONS.length; i++) labels.push(BUTTONS[i].label);

  let view = submenu.makeWith({ header: TITLE }, labels);

  // -----------------------------------------------------------------------
  // When an item is chosen, transmit the matching file. State (BUTTONS,
  // REPEAT) is passed as subscribe extras because mJS has no closures.
  // The callback may return a tuple to update those extras between events;
  // returning undefined keeps them unchanged.
  // -----------------------------------------------------------------------
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
      // return nothing -> keep buttons/repeat for next press
    },
    BUTTONS,
    REPEAT,
  );

  // BACK key -> exit the app.
  eventLoop.subscribe(
    gui.viewDispatcher.navigation,
    function (_sub, _item, loop) {
      loop.stop();
    },
    eventLoop,
  );

  // Show the view, then start the event loop. `run()` blocks until stop().
  gui.viewDispatcher.switchTo(view);
  eventLoop.run();

  print("Balkon-Markise remote closed.");
}
