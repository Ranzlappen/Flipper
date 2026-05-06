// =============================================================================
//  Momentum App Framework  -  gui-example.js
// -----------------------------------------------------------------------------
//  Demonstrates the most common GUI primitives available in Momentum's JS API:
//    - dialog       (yes/no, info, custom buttons)
//    - submenu      (vertical list of choices)
//    - text_input   (on-screen keyboard)
//    - byte_input   (hex byte editor)
//
//  This is a learning template. Use it as a reference when building your own
//  GUI scripts.
//
//  Place at /ext/apps/Scripts/gui-example.js  and run from Apps -> Scripts.
// =============================================================================

let eventLoop = require("event_loop");
let submenu   = require("gui/submenu");
let dialog    = require("gui/dialog");
let textInput = require("gui/text_input");

// --- helper: simple modal info ------------------------------------------------
function info(header, text) {
    dialog.message({ header: header, text: text, center: "OK" });
}

// --- 1. Submenu: pick a demo --------------------------------------------------
let menu = submenu.makeView({
    header: "GUI Example",
    items:  [
        "Yes / No dialog",
        "Text input",
        "Show info",
        "Exit",
    ],
});

eventLoop.subscribe(menu.chosen, function (_sub, idx) {
    if (idx === 0) {
        // 2. Yes/No dialog
        let answer = dialog.message({
            header: "Confirm",
            text:   "Do you like Flippers?",
            left:   "No",
            right:  "Yes",
        });
        info("You chose", answer === "right" ? "Yes" : "No");

    } else if (idx === 1) {
        // 3. Text input (on-screen keyboard)
        let typed = textInput.show({
            header:    "Type something",
            minLength: 1,
            maxLength: 32,
            default:   "hello",
        });
        info("You typed", typed);

    } else if (idx === 2) {
        // 4. Static info
        info("Info", "Momentum's GUI is event-loop based.\n" +
                     "Subscribe to view events.");

    } else if (idx === 3) {
        eventLoop.stop();
    }
});

eventLoop.subscribe(menu.exit, function () { eventLoop.stop(); });

eventLoop.run(menu);
print("GUI example exited.");
