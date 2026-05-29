// Universal Remote — Momentum App Framework
//
// Multi-remote app: maintains a list of remote profiles on the SD card and
// lets the user fire Sub-GHz / Infrared bindings from a polished D-pad view.
//
//   Remote list (default view):
//     - "[+ New remote]" creates a fresh profile via text input.
//     - Picking a remote opens its D-pad view.
//     - BACK exits the app.
//
//   Remote view (per-remote): a two-column map of all six buttons. Each row
//   carries a button glyph (↑↓←→◉↻) with its SHORT-press binding on the left
//   and LONG-press binding on the right; the title and every label side-scroll.
//     - Short-press UP/DOWN/LEFT/RIGHT   → fire that direction's SHORT binding.
//     - Long-press  UP/DOWN/LEFT/RIGHT   → fire that direction's LONG binding.
//     - Short-press OK / BACK            → fire OK / BACK binding (short-only).
//     - Hold OK                          → enter the per-remote editor.
//     - Hold BACK or short-BACK          → return to the remote list.
//
// Profiles live as one file per remote under
// /ext/apps_data/universal_remote/remotes/<basename>.urcfg. The format is
// documented in README.md.

#include "universal_remote_config.h"
#include "universal_remote_transmit.h"

#include <furi.h>
#include <dialogs/dialogs.h>
#include <flipper_format/flipper_format.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define TAG                  "UniversalRemote"
#define UR_SIGNAL_NAMES_CAP  64
#define UR_LABEL_BUF         48
#define UR_NAME_INPUT_BUF    (UR_NAME_MAX + 1)

typedef enum {
    UrStatusIdle = 0,
    UrStatusOk,
    UrStatusFailed,
    UrStatusUnbound,
} UrStatus;

typedef enum {
    UrViewIdRemoteList = 0,
    UrViewIdMain,
    UrViewIdNameInput,
    UrViewIdEditMenu,
    UrViewIdKindMenu,
    UrViewIdSignalMenu,
} UrViewId;

typedef enum {
    UrNameModeCreate = 0,
    UrNameModeRename,
} UrNameMode;

// Custom event ids dispatched through the view_dispatcher.
//   Dispatch range:  100..100 + UrButtonCount*UrGestureCount-1
//   OpenRemote rng:  200..200 + UR_REMOTES_MAX-1
//   Singletons after.
typedef enum {
    UrCustomEventDispatch = 100,
    UrCustomEventOpenRemote = 200,
    UrCustomEventNewRemote = 300,
    UrCustomEventEnterEdit,
    UrCustomEventBackToList,
    UrCustomEventExit,
} UrCustomEvent;

// Edit-menu item ordering. 10 binding slots + Rename + Delete + Save + Discard.
typedef enum {
    UrEditItemBindingsBase = 0,
    UrEditItemBindingsCount = 10, // 4 dirs × 2 + OK + BACK
    UrEditItemRename = 10,
    UrEditItemDelete,
    UrEditItemSave,
    UrEditItemDiscard,
} UrEditItem;

typedef enum {
    UrKindItemSubGhz = 0,
    UrKindItemInfrared,
    UrKindItemClear,
} UrKindItem;

typedef struct {
    // Persistent state
    UrConfig* config; // currently loaded remote (NULL until one is picked)
    UrConfig* edit_config; // scratch buffer for editor
    UrRemoteIndex* remotes; // cached list of remotes

    // GUI handles
    Gui* gui;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;

    Submenu* remote_list_menu;
    View* main_view;
    TextInput* name_input;
    Submenu* edit_menu;
    Submenu* kind_menu;
    Submenu* signal_menu;
    UrViewId current_view;

    // Edit-time scratch
    UrButton editing_button;
    UrGesture editing_gesture;
    FuriString* scratch_path;
    FuriString* scratch_names[UR_SIGNAL_NAMES_CAP];
    size_t scratch_names_count;
    bool scratch_names_truncated;

    // Text input scratch
    char name_buffer[UR_NAME_INPUT_BUF];
    UrNameMode name_mode;

    // Last-press feedback for the main view
    UrStatus status;
    UrButton last_pressed;
    UrGesture last_gesture;

    // Main-view side-scroll animation (drives elements_scrollable_text_line).
    FuriTimer* scroll_timer;
    uint32_t scroll_tick;
} UrApp;

// 10 editor slots, in display order. Indexed by UrEditItemBindingsBase..+9.
static const UrButton edit_slot_button[UrEditItemBindingsCount] = {
    UrButtonUp,
    UrButtonUp,
    UrButtonDown,
    UrButtonDown,
    UrButtonLeft,
    UrButtonLeft,
    UrButtonRight,
    UrButtonRight,
    UrButtonOk,
    UrButtonBack,
};
static const UrGesture edit_slot_gesture[UrEditItemBindingsCount] = {
    UrGestureShort,
    UrGestureLong,
    UrGestureShort,
    UrGestureLong,
    UrGestureShort,
    UrGestureLong,
    UrGestureShort,
    UrGestureLong,
    UrGestureShort,
    UrGestureShort,
};

// --------------------------------------------------------------------------
// Forward declarations
// --------------------------------------------------------------------------

static void switch_view(UrApp* app, UrViewId view);
static void refresh_remote_list(UrApp* app);
static void open_remote(UrApp* app, size_t index);
static void rebuild_edit_menu(UrApp* app);
static void rebuild_kind_menu(UrApp* app);
static void rebuild_signal_menu(UrApp* app);

static void remote_list_callback(void* ctx, uint32_t index);
static void edit_menu_callback(void* ctx, uint32_t index);
static void kind_menu_callback(void* ctx, uint32_t index);
static void signal_menu_callback(void* ctx, uint32_t index);
static void name_input_callback(void* ctx);

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static const char* status_text(UrStatus s) {
    switch(s) {
    case UrStatusOk:
        return "OK";
    case UrStatusFailed:
        return "FAIL";
    case UrStatusUnbound:
        return "unbound";
    case UrStatusIdle:
    default:
        return "";
    }
}

static void binding_summary(const UrBinding* bind, char* out, size_t cap) {
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        snprintf(out, cap, "-");
        return;
    }
    const char* p = furi_string_get_cstr(bind->path);
    const char* slash = strrchr(p, '/');
    const char* base = slash ? slash + 1 : p;
    if(bind->kind == UrActionSubGhz) {
        snprintf(out, cap, "SG %s", base);
    } else {
        snprintf(out, cap, "IR %s/%s", base, furi_string_get_cstr(bind->name));
    }
}

// Compact form: filename without extension, truncated, for the D-pad view.
static void binding_short_label(const UrBinding* bind, char* out, size_t cap) {
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        snprintf(out, cap, "-");
        return;
    }
    const char* p = furi_string_get_cstr(bind->path);
    const char* slash = strrchr(p, '/');
    const char* base = slash ? slash + 1 : p;
    // Drop extension.
    const char* dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if(bind->kind == UrActionInfrared && !furi_string_empty(bind->name)) {
        // IR: show just the signal name (more useful than the file).
        snprintf(out, cap, "%s", furi_string_get_cstr(bind->name));
    } else {
        size_t copy = len < (cap - 1) ? len : (cap - 1);
        memcpy(out, base, copy);
        out[copy] = '\0';
    }
}

static void switch_view(UrApp* app, UrViewId view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

// --------------------------------------------------------------------------
// Remote list view
// --------------------------------------------------------------------------

static void refresh_remote_list(UrApp* app) {
    if(app->remotes) ur_remotes_free(app->remotes);
    app->remotes = ur_remotes_scan();

    submenu_reset(app->remote_list_menu);
    submenu_set_header(app->remote_list_menu, "Universal Remote");

    submenu_add_item(
        app->remote_list_menu,
        "[+ New remote]",
        UR_REMOTES_MAX, // sentinel index meaning "create"
        remote_list_callback,
        app);

    for(size_t i = 0; i < app->remotes->count; i++) {
        submenu_add_item(
            app->remote_list_menu,
            furi_string_get_cstr(app->remotes->names[i]),
            (uint32_t)i,
            remote_list_callback,
            app);
    }
}

static void remote_list_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    if(index == UR_REMOTES_MAX) {
        // "[+ New remote]" — show the text input.
        app->name_mode = UrNameModeCreate;
        app->name_buffer[0] = '\0';
        text_input_set_header_text(app->name_input, "Name the new remote");
        text_input_set_result_callback(
            app->name_input,
            name_input_callback,
            app,
            app->name_buffer,
            UR_NAME_INPUT_BUF,
            true);
        switch_view(app, UrViewIdNameInput);
        return;
    }
    if(index < app->remotes->count) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, UrCustomEventOpenRemote + (uint32_t)index);
    }
}

static void open_remote(UrApp* app, size_t index) {
    if(index >= app->remotes->count) return;
    if(app->config) {
        ur_config_free(app->config);
        app->config = NULL;
    }
    app->config = ur_config_load(furi_string_get_cstr(app->remotes->paths[index]));
    app->status = UrStatusIdle;
    app->last_pressed = UrButtonCount;
    app->last_gesture = UrGestureShort;
    switch_view(app, UrViewIdMain);
}

// --------------------------------------------------------------------------
// Main view (custom canvas)
// --------------------------------------------------------------------------

// Per-row button glyph column (↑ ↓ ← → ◉ ↻), 67×47 at (1,12). This is the
// free-draw layer exported verbatim from Flipper GUI Studio — drawn with the
// same canvas_draw_xbm() call the tool emits, so it's pixel-identical to the
// editor preview. The six glyphs line up top→bottom with main_row_btn below.
#define UR_GLYPHS_W 67
#define UR_GLYPHS_H 47
static const uint8_t remote_glyph_column[] = {
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// The six rows, top→bottom, matching the glyph column above.
static const UrButton main_row_btn[6] = {
    UrButtonUp,
    UrButtonDown,
    UrButtonLeft,
    UrButtonRight,
    UrButtonOk,
    UrButtonBack,
};
// FontKeyboard baselines for each row (8px pitch), per the Studio layout.
static const uint8_t main_row_y[6] = {19, 27, 35, 43, 51, 59};

static void view_draw_callback(Canvas* canvas, void* model) {
    UrApp* app = *(UrApp**)model;
    const size_t scroll = app->scroll_tick;
    canvas_clear(canvas);

    // Reused across every label so we don't churn allocations each frame.
    FuriString* s = furi_string_alloc();

    // ----- Top row: title (left) + status (right), both side-scroll -----
    const char* title = app->config ? furi_string_get_cstr(app->config->display_name) :
                                      "Universal Remote";
    canvas_set_font(canvas, FontPrimary);
    furi_string_set_str(s, title);
    elements_scrollable_text_line(canvas, 2, 9, 63, s, scroll, false);

    char status[40];
    if(app->config && app->last_pressed < UrButtonCount && app->status != UrStatusIdle) {
        const char* gname = (app->last_gesture == UrGestureLong) ? "long" : "short";
        snprintf(
            status,
            sizeof(status),
            "last: %s %s %s",
            ur_button_to_name(app->last_pressed),
            gname,
            status_text(app->status));
    } else {
        snprintf(status, sizeof(status), "hold OK=edit  hold BACK=list");
    }
    canvas_set_font(canvas, FontSecondary);
    furi_string_set_str(s, status);
    elements_scrollable_text_line(canvas, 70, 9, 57, s, scroll, false);

    canvas_draw_line(canvas, 0, 10, 127, 10);

    if(!app->config) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No remote loaded.");
        canvas_draw_str(canvas, 2, 40, "Press BACK to pick one.");
        furi_string_free(s);
        return;
    }

    // ----- Glyph column (verbatim Studio free-draw) + column divider -----
    canvas_draw_xbm(canvas, 1, 12, UR_GLYPHS_W, UR_GLYPHS_H, remote_glyph_column);
    canvas_draw_line(canvas, 67, 11, 67, 63);

    // ----- 6 rows × 2 columns: left = SHORT label, right = LONG label -----
    // OK/BACK have no long binding, so their right cell shows "-".
    canvas_set_font(canvas, FontKeyboard);
    char label[UR_LABEL_BUF];
    for(size_t r = 0; r < 6; r++) {
        UrButton b = main_row_btn[r];
        uint8_t y = main_row_y[r];

        binding_short_label(&app->config->bindings[b][UrGestureShort], label, sizeof(label));
        furi_string_set_str(s, label);
        elements_scrollable_text_line(canvas, 9, y, 57, s, scroll, false);

        binding_short_label(&app->config->bindings[b][UrGestureLong], label, sizeof(label));
        furi_string_set_str(s, label);
        elements_scrollable_text_line(canvas, 69, y, 58, s, scroll, false);
    }

    furi_string_free(s);
}

static UrButton input_key_to_button(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return UrButtonUp;
    case InputKeyDown:
        return UrButtonDown;
    case InputKeyLeft:
        return UrButtonLeft;
    case InputKeyRight:
        return UrButtonRight;
    case InputKeyOk:
        return UrButtonOk;
    case InputKeyBack:
        return UrButtonBack;
    default:
        return UrButtonCount;
    }
}

static uint32_t dispatch_event(UrButton b, UrGesture g) {
    return UrCustomEventDispatch + ((uint32_t)b * UrGestureCount) + (uint32_t)g;
}

static bool view_input_callback(InputEvent* event, void* context) {
    UrApp* app = context;

    if(event->type == InputTypeLong) {
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventEnterEdit);
            return true;
        }
        if(event->key == InputKeyBack) {
            // Hold-BACK is the dedicated "return to remote list" gesture.
            view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventBackToList);
            return true;
        }
        UrButton b = input_key_to_button(event->key);
        if(b == UrButtonUp || b == UrButtonDown || b == UrButtonLeft || b == UrButtonRight) {
            app->last_pressed = b;
            app->last_gesture = UrGestureLong;
            view_dispatcher_send_custom_event(app->view_dispatcher, dispatch_event(b, UrGestureLong));
            return true;
        }
        return false;
    }

    if(event->type != InputTypeShort) return false;

    UrButton b = input_key_to_button(event->key);
    if(b >= UrButtonCount) return false;

    if(b == UrButtonBack) {
        // Short-BACK fires the BACK binding (if any); hold-BACK is the dedicated
        // "return to list" gesture. When BACK is unbound we keep short-BACK as
        // navigation so the user is never stuck on the remote view.
        const UrBinding* bind =
            app->config ? &app->config->bindings[UrButtonBack][UrGestureShort] : NULL;
        bool bound = bind && bind->kind != UrActionNone && !furi_string_empty(bind->path);
        if(bound) {
            app->last_pressed = b;
            app->last_gesture = UrGestureShort;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, dispatch_event(b, UrGestureShort));
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventBackToList);
        }
        return true;
    }

    app->last_pressed = b;
    app->last_gesture = UrGestureShort;
    view_dispatcher_send_custom_event(app->view_dispatcher, dispatch_event(b, UrGestureShort));
    return true;
}

// --------------------------------------------------------------------------
// Transmit dispatch
// --------------------------------------------------------------------------

static void notify_status(UrApp* app, UrStatus s) {
    app->status = s;
    with_view_model(app->main_view, UrApp ** m, { *m = app; }, true);
    if(s == UrStatusOk) {
        notification_message(app->notifications, &sequence_success);
    } else if(s == UrStatusFailed) {
        notification_message(app->notifications, &sequence_error);
    } else if(s == UrStatusUnbound) {
        notification_message(app->notifications, &sequence_blink_yellow_100);
    }
}

static void do_dispatch(UrApp* app, UrButton b, UrGesture g) {
    if(!app->config) return;
    const UrBinding* bind = &app->config->bindings[b][g];
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        notify_status(app, UrStatusUnbound);
        return;
    }
    notification_message(app->notifications, &sequence_blink_blue_100);

    bool ok = false;
    if(bind->kind == UrActionSubGhz) {
        ok = ur_tx_subghz(furi_string_get_cstr(bind->path));
    } else if(bind->kind == UrActionInfrared) {
        ok = ur_tx_infrared(
            furi_string_get_cstr(bind->path), furi_string_get_cstr(bind->name));
    }
    notify_status(app, ok ? UrStatusOk : UrStatusFailed);
}

// --------------------------------------------------------------------------
// IR signal-name reader (used by the edit flow)
// --------------------------------------------------------------------------

static void scratch_names_clear(UrApp* app) {
    for(size_t i = 0; i < app->scratch_names_count; i++) {
        furi_string_free(app->scratch_names[i]);
        app->scratch_names[i] = NULL;
    }
    app->scratch_names_count = 0;
}

static void read_ir_signal_names(UrApp* app, const char* path) {
    scratch_names_clear(app);
    app->scratch_names_truncated = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();
    FuriString* name = furi_string_alloc();

    if(flipper_format_buffered_file_open_existing(fff, path)) {
        uint32_t version = 0;
        if(flipper_format_read_header(fff, header, &version)) {
            while(flipper_format_read_string(fff, "name", name)) {
                if(app->scratch_names_count < UR_SIGNAL_NAMES_CAP) {
                    app->scratch_names[app->scratch_names_count++] = furi_string_alloc_set(name);
                } else {
                    app->scratch_names_truncated = true;
                    break;
                }
            }
        }
    } else {
        FURI_LOG_E(TAG, "ir: read signal names: cannot open %s", path);
    }

    furi_string_free(name);
    furi_string_free(header);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

static void show_truncation_popup(UrApp* app) {
    DialogMessage* msg = dialog_message_alloc();
    dialog_message_set_header(msg, "Too many signals", 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(
        msg,
        "Only first 64 signals shown.\nEdit the .urcfg for the rest.",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
}

static bool confirm_delete(UrApp* app, const char* name) {
    DialogMessage* msg = dialog_message_alloc();
    char header[40];
    snprintf(header, sizeof(header), "Delete %s?", name);
    dialog_message_set_header(msg, header, 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(
        msg, "This removes the .urcfg file.\nCannot be undone.", 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(msg, "Cancel", NULL, "Delete");
    DialogMessageButton r = dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
    return r == DialogMessageButtonRight;
}

// --------------------------------------------------------------------------
// Edit flow
// --------------------------------------------------------------------------

static void enter_edit(UrApp* app) {
    if(!app->config) return;
    ur_config_copy(app->edit_config, app->config);
    furi_string_set(app->edit_config->file_path, app->config->file_path);
    rebuild_edit_menu(app);
    switch_view(app, UrViewIdEditMenu);
}

static void exit_edit(UrApp* app, bool save) {
    if(save) {
        ur_config_copy(app->config, app->edit_config);
        if(!ur_config_save(app->config)) {
            notify_status(app, UrStatusFailed);
        }
        // Names may have changed — refresh the list so it's up to date.
        refresh_remote_list(app);
    }
    switch_view(app, UrViewIdMain);
}

static void rebuild_edit_menu(UrApp* app) {
    submenu_reset(app->edit_menu);
    char header[40];
    snprintf(
        header, sizeof(header), "Edit: %s", furi_string_get_cstr(app->edit_config->display_name));
    submenu_set_header(app->edit_menu, header);

    char label[UR_LABEL_BUF * 2];
    char summary[UR_LABEL_BUF];
    for(size_t i = 0; i < UrEditItemBindingsCount; i++) {
        UrButton b = edit_slot_button[i];
        UrGesture g = edit_slot_gesture[i];
        binding_summary(&app->edit_config->bindings[b][g], summary, sizeof(summary));
        if(ur_button_has_long(b)) {
            snprintf(
                label,
                sizeof(label),
                "%s %s: %s",
                ur_button_to_name(b),
                (g == UrGestureLong) ? "long " : "short",
                summary);
        } else {
            snprintf(label, sizeof(label), "%s: %s", ur_button_to_name(b), summary);
        }
        submenu_add_item(app->edit_menu, label, (uint32_t)i, edit_menu_callback, app);
    }
    submenu_add_item(app->edit_menu, "Rename remote", UrEditItemRename, edit_menu_callback, app);
    submenu_add_item(app->edit_menu, "Delete remote", UrEditItemDelete, edit_menu_callback, app);
    submenu_add_item(app->edit_menu, "Save & exit", UrEditItemSave, edit_menu_callback, app);
    submenu_add_item(
        app->edit_menu, "Discard & exit", UrEditItemDiscard, edit_menu_callback, app);
}

static void edit_menu_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    if(index < UrEditItemBindingsCount) {
        app->editing_button = edit_slot_button[index];
        app->editing_gesture = edit_slot_gesture[index];
        rebuild_kind_menu(app);
        switch_view(app, UrViewIdKindMenu);
        return;
    }
    if(index == UrEditItemRename) {
        app->name_mode = UrNameModeRename;
        // Pre-fill with the current display name.
        const char* cur = furi_string_get_cstr(app->edit_config->display_name);
        size_t n = strlen(cur);
        if(n >= UR_NAME_INPUT_BUF) n = UR_NAME_INPUT_BUF - 1;
        memcpy(app->name_buffer, cur, n);
        app->name_buffer[n] = '\0';
        text_input_set_header_text(app->name_input, "Rename remote");
        text_input_set_result_callback(
            app->name_input,
            name_input_callback,
            app,
            app->name_buffer,
            UR_NAME_INPUT_BUF,
            false);
        switch_view(app, UrViewIdNameInput);
        return;
    }
    if(index == UrEditItemDelete) {
        const char* name = furi_string_get_cstr(app->edit_config->display_name);
        if(!confirm_delete(app, name)) {
            // Bounce back to the edit menu.
            switch_view(app, UrViewIdEditMenu);
            return;
        }
        // Delete the file backing this remote, then return to the list.
        ur_remote_delete(furi_string_get_cstr(app->edit_config->file_path));
        ur_config_free(app->config);
        app->config = NULL;
        refresh_remote_list(app);
        switch_view(app, UrViewIdRemoteList);
        return;
    }
    if(index == UrEditItemSave) {
        exit_edit(app, true);
        return;
    }
    if(index == UrEditItemDiscard) {
        exit_edit(app, false);
        return;
    }
}

static void rebuild_kind_menu(UrApp* app) {
    submenu_reset(app->kind_menu);
    char header[UR_LABEL_BUF];
    if(ur_button_has_long(app->editing_button)) {
        snprintf(
            header,
            sizeof(header),
            "%s %s: action",
            ur_button_to_name(app->editing_button),
            (app->editing_gesture == UrGestureLong) ? "long" : "short");
    } else {
        snprintf(header, sizeof(header), "%s: action", ur_button_to_name(app->editing_button));
    }
    submenu_set_header(app->kind_menu, header);
    submenu_add_item(
        app->kind_menu, "Sub-GHz file (.sub)", UrKindItemSubGhz, kind_menu_callback, app);
    submenu_add_item(
        app->kind_menu, "Infrared signal (.ir)", UrKindItemInfrared, kind_menu_callback, app);
    submenu_add_item(app->kind_menu, "Clear binding", UrKindItemClear, kind_menu_callback, app);
}

static bool pick_file(UrApp* app, const char* base, const char* ext, FuriString* out) {
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ext, NULL);
    opts.base_path = base;
    opts.hide_dot_files = true;
    opts.hide_ext = false;
    FuriString* preselect = furi_string_alloc_set(base);
    bool picked = dialog_file_browser_show(app->dialogs, out, preselect, &opts);
    furi_string_free(preselect);
    return picked;
}

static void kind_menu_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    UrBinding* bind = &app->edit_config->bindings[app->editing_button][app->editing_gesture];

    if(index == UrKindItemClear) {
        bind->kind = UrActionNone;
        furi_string_reset(bind->path);
        furi_string_reset(bind->name);
        rebuild_edit_menu(app);
        switch_view(app, UrViewIdEditMenu);
        return;
    }

    if(index == UrKindItemSubGhz) {
        if(pick_file(app, "/ext/subghz", ".sub", app->scratch_path)) {
            bind->kind = UrActionSubGhz;
            furi_string_set(bind->path, app->scratch_path);
            furi_string_reset(bind->name);
        }
        rebuild_edit_menu(app);
        switch_view(app, UrViewIdEditMenu);
        return;
    }

    if(index == UrKindItemInfrared) {
        if(!pick_file(app, "/ext/infrared", ".ir", app->scratch_path)) {
            switch_view(app, UrViewIdEditMenu);
            return;
        }
        read_ir_signal_names(app, furi_string_get_cstr(app->scratch_path));
        if(app->scratch_names_count == 0) {
            FURI_LOG_W(TAG, "ir: no signals found in %s", furi_string_get_cstr(app->scratch_path));
            switch_view(app, UrViewIdEditMenu);
            return;
        }
        if(app->scratch_names_truncated) {
            show_truncation_popup(app);
        }
        rebuild_signal_menu(app);
        switch_view(app, UrViewIdSignalMenu);
        return;
    }
}

static void rebuild_signal_menu(UrApp* app) {
    submenu_reset(app->signal_menu);
    char header[UR_LABEL_BUF];
    const char* p = furi_string_get_cstr(app->scratch_path);
    const char* slash = strrchr(p, '/');
    snprintf(header, sizeof(header), "Signal in %s", slash ? slash + 1 : p);
    submenu_set_header(app->signal_menu, header);
    for(size_t i = 0; i < app->scratch_names_count; i++) {
        submenu_add_item(
            app->signal_menu,
            furi_string_get_cstr(app->scratch_names[i]),
            (uint32_t)i,
            signal_menu_callback,
            app);
    }
}

static void signal_menu_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    if(index >= app->scratch_names_count) return;

    UrBinding* bind = &app->edit_config->bindings[app->editing_button][app->editing_gesture];
    bind->kind = UrActionInfrared;
    furi_string_set(bind->path, app->scratch_path);
    furi_string_set(bind->name, app->scratch_names[index]);

    scratch_names_clear(app);
    rebuild_edit_menu(app);
    switch_view(app, UrViewIdEditMenu);
}

// --------------------------------------------------------------------------
// Name input
// --------------------------------------------------------------------------

static void name_input_callback(void* ctx) {
    UrApp* app = ctx;
    const char* entered = app->name_buffer;
    if(entered[0] == '\0') {
        // Empty name — bounce.
        switch_view(
            app, app->name_mode == UrNameModeCreate ? UrViewIdRemoteList : UrViewIdEditMenu);
        return;
    }

    if(app->name_mode == UrNameModeCreate) {
        FuriString* new_path = furi_string_alloc();
        bool ok = ur_remote_create(entered, new_path);
        if(ok) {
            refresh_remote_list(app);
            // Open the newly created remote immediately.
            if(app->config) {
                ur_config_free(app->config);
                app->config = NULL;
            }
            app->config = ur_config_load(furi_string_get_cstr(new_path));
            app->status = UrStatusIdle;
            app->last_pressed = UrButtonCount;
            switch_view(app, UrViewIdMain);
        } else {
            notify_status(app, UrStatusFailed);
            switch_view(app, UrViewIdRemoteList);
        }
        furi_string_free(new_path);
        return;
    }

    // Rename: pivot edit_config to the new name (which also renames the file
    // on save). We do the rename eagerly so the on-screen edit header updates.
    ur_remote_rename(app->edit_config, entered);
    // Mirror the rename in the live config so the user can see it after Save.
    if(app->config) {
        furi_string_set(app->config->display_name, app->edit_config->display_name);
        furi_string_set(app->config->file_path, app->edit_config->file_path);
    }
    rebuild_edit_menu(app);
    switch_view(app, UrViewIdEditMenu);
}

// --------------------------------------------------------------------------
// ViewDispatcher event wiring
// --------------------------------------------------------------------------

static bool custom_event_callback(void* context, uint32_t event) {
    UrApp* app = context;

    if(event >= UrCustomEventDispatch &&
       event < UrCustomEventDispatch + (UrButtonCount * UrGestureCount)) {
        uint32_t rel = event - UrCustomEventDispatch;
        UrButton b = (UrButton)(rel / UrGestureCount);
        UrGesture g = (UrGesture)(rel % UrGestureCount);
        do_dispatch(app, b, g);
        return true;
    }
    if(event >= UrCustomEventOpenRemote && event < UrCustomEventOpenRemote + UR_REMOTES_MAX) {
        open_remote(app, event - UrCustomEventOpenRemote);
        return true;
    }
    if(event == UrCustomEventNewRemote) {
        // Reached via short-OK on "[+ New remote]"; same flow as the list callback.
        return true;
    }
    if(event == UrCustomEventEnterEdit) {
        enter_edit(app);
        return true;
    }
    if(event == UrCustomEventBackToList) {
        switch_view(app, UrViewIdRemoteList);
        return true;
    }
    if(event == UrCustomEventExit) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

static bool navigation_event_callback(void* context) {
    UrApp* app = context;
    // BACK from submenus / text input climbs the view stack.
    switch(app->current_view) {
    case UrViewIdSignalMenu:
        scratch_names_clear(app);
        switch_view(app, UrViewIdEditMenu);
        return true;
    case UrViewIdKindMenu:
        switch_view(app, UrViewIdEditMenu);
        return true;
    case UrViewIdEditMenu:
        // Short-BACK from the edit root commits pending edits, matching the
        // previous behaviour. Use "Discard & exit" to abandon.
        exit_edit(app, true);
        return true;
    case UrViewIdNameInput:
        // Cancel name entry.
        switch_view(
            app, app->name_mode == UrNameModeCreate ? UrViewIdRemoteList : UrViewIdEditMenu);
        return true;
    case UrViewIdMain:
        // Main view consumes BACK itself; if we get here, fall through to list.
        switch_view(app, UrViewIdRemoteList);
        return true;
    case UrViewIdRemoteList:
    default:
        // BACK from the root list exits.
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

// --------------------------------------------------------------------------
// App lifecycle
// --------------------------------------------------------------------------

// Periodic tick that advances the side-scroll offset and repaints the remote
// view. Runs on the timer thread; bumping the counter and committing the
// locking view model (the `true` flag) to request a redraw is safe from there.
// Only runs while the main view is on screen (see enter/exit below).
static void scroll_timer_cb(void* ctx) {
    UrApp* app = ctx;
    app->scroll_tick++;
    with_view_model(app->main_view, UrApp ** m, { *m = app; }, true);
}

static void main_view_enter(void* ctx) {
    UrApp* app = ctx;
    app->scroll_tick = 0;
    furi_timer_start(app->scroll_timer, furi_ms_to_ticks(120));
}

static void main_view_exit(void* ctx) {
    UrApp* app = ctx;
    furi_timer_stop(app->scroll_timer);
}

static UrApp* app_alloc(void) {
    UrApp* app = malloc(sizeof(UrApp));
    app->config = NULL;
    app->edit_config = ur_config_alloc();
    app->remotes = NULL;

    app->status = UrStatusIdle;
    app->last_pressed = UrButtonCount;
    app->last_gesture = UrGestureShort;
    app->editing_button = UrButtonUp;
    app->editing_gesture = UrGestureShort;
    app->scratch_path = furi_string_alloc();
    app->scratch_names_count = 0;
    app->scratch_names_truncated = false;
    for(size_t i = 0; i < UR_SIGNAL_NAMES_CAP; i++) app->scratch_names[i] = NULL;
    app->name_buffer[0] = '\0';
    app->name_mode = UrNameModeCreate;
    app->current_view = UrViewIdRemoteList;
    app->scroll_tick = 0;
    app->scroll_timer = furi_timer_alloc(scroll_timer_cb, FuriTimerTypePeriodic, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Remote list (submenu).
    app->remote_list_menu = submenu_alloc();

    // Main view (custom).
    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(UrApp*));
    with_view_model(app->main_view, UrApp ** m, { *m = app; }, true);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, view_draw_callback);
    view_set_input_callback(app->main_view, view_input_callback);
    view_set_enter_callback(app->main_view, main_view_enter);
    view_set_exit_callback(app->main_view, main_view_exit);

    // Text input.
    app->name_input = text_input_alloc();

    // Submenus.
    app->edit_menu = submenu_alloc();
    app->kind_menu = submenu_alloc();
    app->signal_menu = submenu_alloc();

    // Dispatcher.
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, navigation_event_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, UrViewIdRemoteList, submenu_get_view(app->remote_list_menu));
    view_dispatcher_add_view(app->view_dispatcher, UrViewIdMain, app->main_view);
    view_dispatcher_add_view(
        app->view_dispatcher, UrViewIdNameInput, text_input_get_view(app->name_input));
    view_dispatcher_add_view(
        app->view_dispatcher, UrViewIdEditMenu, submenu_get_view(app->edit_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, UrViewIdKindMenu, submenu_get_view(app->kind_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, UrViewIdSignalMenu, submenu_get_view(app->signal_menu));
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void app_free(UrApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdSignalMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdKindMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdEditMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdNameInput);
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdMain);
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdRemoteList);

    furi_timer_stop(app->scroll_timer);
    furi_timer_free(app->scroll_timer);

    submenu_free(app->signal_menu);
    submenu_free(app->kind_menu);
    submenu_free(app->edit_menu);
    text_input_free(app->name_input);
    view_free(app->main_view);
    submenu_free(app->remote_list_menu);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    scratch_names_clear(app);
    furi_string_free(app->scratch_path);

    if(app->remotes) ur_remotes_free(app->remotes);
    if(app->config) ur_config_free(app->config);
    ur_config_free(app->edit_config);
    free(app);
}

int32_t universal_remote_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "starting");

    ur_tx_init();

    // One-shot migration: legacy config.txt → remotes/default.urcfg.
    ur_remotes_migrate_if_needed();

    UrApp* app = app_alloc();
    refresh_remote_list(app);
    switch_view(app, UrViewIdRemoteList);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    ur_tx_deinit();
    FURI_LOG_I(TAG, "exited");
    return 0;
}
