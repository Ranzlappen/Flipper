// =============================================================================
//  Momentum App Framework  -  subghz-multi-remote-template.js
// -----------------------------------------------------------------------------
//  Multi-profile Sub-GHz remote with short and long-press bindings on the
//  four directional buttons. Each profile maps UP / DOWN / LEFT / RIGHT to a
//  pair of .sub files (one short-press binding, one long-press), plus OK to
//  a single .sub.
//
//  Flow:
//    - Pick a profile from the picker (skipped when only one is configured).
//    - The remote view shows one row per direction + OK with both bindings
//      printed inline.
//    - Navigate with UP / DOWN to highlight a row, then:
//        * Short-press OK  -> fires that row's SHORT binding.
//        * HOLD       OK  -> fires that row's LONG binding (OK row: no-op).
//    - BACK returns to the picker; BACK from the picker exits.
//
//  HOW TO ADAPT
//  ------------
//   1. Record your signals (Sub-GHz -> Read -> press your remote -> Save).
//   2. Edit the REMOTES array below — add one entry per physical remote,
//      then fill in absolute file paths. Leave a slot as "" for "unbound".
//   3. Save as <your_remote>.js into /ext/apps/Scripts/.
//   4. Run from Apps -> Scripts.
//
//  WHY HOLD IS ON OK, NOT THE D-PAD
//  --------------------------------
//  The mJS GUI list views (submenu / button_menu / button_panel) own D-pad
//  navigation internally, so a JS script can't receive a raw "long UP"
//  event the way the C universal-remote app can. Instead, this template
//  uses button_menu's `input` contract — focus a direction with the D-pad
//  and press OK (short or long) to fire its SHORT / LONG binding.
//
//  REQUIRES: Momentum firmware (uses the Momentum-enhanced `subghz` module).
//  No closures: mJS state passes via subscribe() args. ALWAYS QUOTE FILE
//  PATHS in the REMOTES array below — bare identifiers cause
//  `parse error at line N: [> Foo_]` on the Flipper.
// =============================================================================

let eventLoop = require("event_loop");
let gui = require("gui");
let submenu = require("gui/submenu");
let buttonMenu = require("gui/button_menu");
let subghz = require("subghz");
let notify = require("notification");
let storage = require("storage");

// ---- CONFIG: one entry per remote profile -----------------------------------
// Each direction has independent SHORT and LONG slots; "" means unbound.
// `ok` is short-only (long-OK is reserved for the SHORT/LONG selector itself).
let REMOTES = [
  {
    name: "Garage",
    up: {
      short: "/ext/subghz/garage_open.sub",
      long: "/ext/subghz/garage_full.sub",
    },
    down: { short: "/ext/subghz/garage_close.sub", long: "" },
    left: { short: "", long: "" },
    right: { short: "", long: "" },
    ok: "/ext/subghz/garage_stop.sub",
  },
  {
    name: "Awning",
    up: { short: "/ext/subghz/Balkon_markise_rein.sub", long: "" },
    down: { short: "/ext/subghz/Balkon_markise_raus.sub", long: "" },
    left: { short: "", long: "" },
    right: { short: "", long: "" },
    ok: "/ext/subghz/Balkon_markise_stop.sub",
  },
];

// How many times each press retransmits. Most remotes need 1; bump to 2–3
// for receivers that ignore single bursts.
let REPEAT = 1;
// -----------------------------------------------------------------------------

// One row per directional + OK. Index 0..3 are D-pad (have short/long),
// index 4 is OK (short-only).
let ROW_KEYS = ["up", "down", "left", "right", "ok"];
let ROW_LABELS = ["UP", "DOWN", "LEFT", "RIGHT", "OK"];

function basenameNoExt(path) {
  if (!path) return "-";
  let slash = path.lastIndexOf("/");
  let base = slash >= 0 ? path.substring(slash + 1) : path;
  let dot = base.lastIndexOf(".");
  return dot > 0 ? base.substring(0, dot) : base;
}

function pathFor(remote, rowIndex, gesture) {
  let key = ROW_KEYS[rowIndex];
  let slot = remote[key];
  if (!slot) return "";
  // OK is a bare string; the directional slots are objects.
  if (key === "ok") return gesture === "short" ? slot : "";
  return slot[gesture] || "";
}

function rowLabel(remote, rowIndex) {
  let s = basenameNoExt(pathFor(remote, rowIndex, "short"));
  if (ROW_KEYS[rowIndex] === "ok") {
    return "OK   " + s;
  }
  let l = basenameNoExt(pathFor(remote, rowIndex, "long"));
  return ROW_LABELS[rowIndex] + "  S:" + s + "  L:" + l;
}

// ---- Pre-flight: warn about missing files ----------------------------------
function reportMissing() {
  let missing = [];
  for (let i = 0; i < REMOTES.length; i++) {
    let r = REMOTES[i];
    for (let row = 0; row < ROW_KEYS.length; row++) {
      let gestures = row === 4 ? ["short"] : ["short", "long"];
      for (let g = 0; g < gestures.length; g++) {
        let p = pathFor(r, row, gestures[g]);
        if (p && !storage.fileExists(p)) {
          missing.push(
            r.name + " " + ROW_LABELS[row] + " " + gestures[g] + " -> " + p,
          );
        }
      }
    }
  }
  if (missing.length > 0) {
    print("Missing .sub files (these bindings will fail to transmit):");
    for (let i = 0; i < missing.length; i++) print("  -", missing[i]);
  }
}
reportMissing();

// ---- Build the views --------------------------------------------------------
let pickerLabels = [];
for (let i = 0; i < REMOTES.length; i++) pickerLabels.push(REMOTES[i].name);
let picker = submenu.makeWith({ header: "Universal Remote" }, pickerLabels);

// Single reusable button_menu — children are reset whenever a profile is
// chosen. mJS heap is tight; pre-building one view per profile would waste it.
let remoteView = buttonMenu.makeWith({ header: "-" }, []);

// mJS has no closures, so the active remote index is kept in a 1-element
// array passed by reference into every subscribe() call that needs it.
let active = [0];

function loadRemoteIntoView(remoteIndex) {
  let r = REMOTES[remoteIndex];
  let children = [];
  for (let i = 0; i < ROW_KEYS.length; i++) {
    children.push({ type: "common", label: rowLabel(r, i) });
  }
  remoteView.set("header", r.name);
  remoteView.setChildren(/** @type {any} */ (children));
}

// ---- Event wiring -----------------------------------------------------------
eventLoop.subscribe(
  picker.chosen,
  function (_sub, index, remotes, view, activeArr) {
    let i = /** @type {number} */ (index);
    if (!remotes[i]) return;
    activeArr[0] = i;
    loadRemoteIntoView(i);
    gui.viewDispatcher.switchTo(view);
  },
  REMOTES,
  remoteView,
  active,
);

// button_menu.input fires { index, type }. We only care about confirmed
// gestures (short / long); press / release / repeat are ignored.
eventLoop.subscribe(
  remoteView.input,
  function (_sub, item, remotes, activeArr, repeat) {
    let evt = /** @type {{ index: number, type: string }} */ (item);
    if (evt.type !== "short" && evt.type !== "long") return;
    let r = remotes[activeArr[0]];
    if (!r) return;

    let gesture = evt.type; // "short" or "long"
    let path = pathFor(r, evt.index, gesture);
    if (!path) {
      print("unbound:", r.name, ROW_LABELS[evt.index] || "?", gesture);
      notify.blink("yellow", "short");
      return;
    }
    print("TX:", path);
    try {
      subghz.transmitFile(path, repeat);
      notify.success();
    } catch (e) {
      print("TX failed:", e);
      notify.error();
    }
  },
  REMOTES,
  active,
  REPEAT,
);

// BACK: pop to picker, or exit if already on the picker.
eventLoop.subscribe(
  gui.viewDispatcher.navigation,
  function (_sub, _item, loop, pickerView) {
    if (gui.viewDispatcher.currentView === pickerView) {
      loop.stop();
    } else {
      gui.viewDispatcher.switchTo(pickerView);
    }
  },
  eventLoop,
  picker,
);

// ---- Boot -------------------------------------------------------------------
if (REMOTES.length === 1) {
  loadRemoteIntoView(0);
  gui.viewDispatcher.switchTo(remoteView);
} else {
  gui.viewDispatcher.switchTo(picker);
}

print("Multi-remote ready (", REMOTES.length, "profile(s)).");
eventLoop.run();
print("Multi-remote exited.");
