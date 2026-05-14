// =============================================================================
//  Momentum App Framework  -  gui-example.js
// -----------------------------------------------------------------------------
//  Demonstrates the real Momentum/Flipper GUI pattern:
//    - require event_loop + gui  (gui depends on event_loop, in that order)
//    - build views via factory.make() / factory.makeWith(props, children)
//    - subscribe to each view's event Contracts (e.g. submenu.chosen)
//    - gui.viewDispatcher.switchTo(view) to display
//    - eventLoop.run() to start the loop
//    - gui.viewDispatcher.navigation fires on BACK
//
//  IMPORTANT: the runtime is mJS, which has NO closures. Any state you need
//  inside a callback must be passed as extra args to subscribe().
//
//  Place at /ext/apps/Scripts/gui-example.js and run from Apps -> Scripts.
// =============================================================================

let eventLoop = require("event_loop");
let gui = require("gui");
let submenu = require("gui/submenu");
let dialog = require("gui/dialog");
let textInput = require("gui/text_input");
let notify = require("notification");

// ---------------------------------------------------------------------------
// Build all views up-front. The view dispatcher tracks them automatically.
// ---------------------------------------------------------------------------
let views = {
  menu: submenu.makeWith({ header: "GUI Example" }, [
    "Yes / No dialog",
    "Text input",
    "Beep + LED",
    "Exit",
  ]),

  confirm: dialog.makeWith({
    header: "Confirm",
    text: "Do you like Flippers?",
    left: "No",
    center: "",
    right: "Yes",
  }),

  typer: textInput.makeWith({
    header: "Type something",
    minLength: 1,
    maxLength: 32,
    defaultText: "hello",
    defaultTextClear: true,
    illegalSymbols: false,
  }),
};

// ---------------------------------------------------------------------------
// Menu item chosen -> switch to the matching screen (or exit).
// ---------------------------------------------------------------------------
eventLoop.subscribe(
  views.menu.chosen,
  function (_sub, idx, ctx) {
    if (idx === 0) {
      ctx.gui.viewDispatcher.switchTo(ctx.views.confirm);
    } else if (idx === 1) {
      ctx.gui.viewDispatcher.switchTo(ctx.views.typer);
    } else if (idx === 2) {
      ctx.notify.blink("blue", "short");
      ctx.notify.success();
      // stay on the menu
    } else if (idx === 3) {
      ctx.loop.stop();
    }
  },
  { gui: gui, views: views, notify: notify, loop: eventLoop },
);

// ---------------------------------------------------------------------------
// Dialog answered -> log the choice and return to the menu.
// ---------------------------------------------------------------------------
eventLoop.subscribe(
  views.confirm.input,
  function (_sub, button, ctx) {
    print("Dialog button:", button); // "left" | "center" | "right"
    ctx.gui.viewDispatcher.switchTo(ctx.views.menu);
  },
  { gui: gui, views: views },
);

// ---------------------------------------------------------------------------
// Text submitted -> log it and return to the menu.
// ---------------------------------------------------------------------------
eventLoop.subscribe(
  views.typer.input,
  function (_sub, text, ctx) {
    print("You typed:", text);
    ctx.gui.viewDispatcher.switchTo(ctx.views.menu);
  },
  { gui: gui, views: views },
);

// ---------------------------------------------------------------------------
// BACK on any screen -> if we're on the menu, exit; otherwise go to menu.
// ---------------------------------------------------------------------------
eventLoop.subscribe(
  gui.viewDispatcher.navigation,
  function (_sub, _item, ctx) {
    if (ctx.gui.viewDispatcher.currentView === ctx.views.menu) {
      ctx.loop.stop();
    } else {
      ctx.gui.viewDispatcher.switchTo(ctx.views.menu);
    }
  },
  { gui: gui, views: views, loop: eventLoop },
);

// ---------------------------------------------------------------------------
// Show the menu and run.
// ---------------------------------------------------------------------------
gui.viewDispatcher.switchTo(views.menu);
eventLoop.run();
print("GUI example exited.");
