// Universal Remote — Momentum App Framework
//
// All six hardware buttons (UP/DOWN/LEFT/RIGHT/OK/BACK) are mappable to a
// saved Sub-GHz `.sub` file or a named signal from an `.ir` file.
//   Short press  → fire the bound action.
//   Hold OK      → enter the on-device editor.
//   Hold BACK    → exit the app.
//
// Mappings live in /ext/apps_data/universal_remote/config.txt. The editor
// rewrites that file on save (any comments you put in by hand will be lost).

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
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define TAG                  "UniversalRemote"
#define UR_SIGNAL_NAMES_CAP  64
#define UR_LABEL_BUF         48

typedef enum {
    UrStatusIdle = 0,
    UrStatusOk,
    UrStatusFailed,
    UrStatusUnbound,
} UrStatus;

typedef enum {
    UrViewIdMain = 0,
    UrViewIdEditMenu,
    UrViewIdKindMenu,
    UrViewIdSignalMenu,
} UrViewId;

typedef enum {
    // Main view → transmit one of the 6 buttons (offset by UrButton index).
    UrCustomEventDispatch = 100,

    // Main view → enter / exit (offset above to avoid range overlap).
    UrCustomEventEnterEdit = 200,
    UrCustomEventExit,

    // Edit menu item indexes; safe to overlap with custom event ids
    // because they're routed through submenu callback, not view_dispatcher.
} UrCustomEvent;

// Indexes used inside the edit-main submenu callback. First UrButtonCount
// indexes map 1:1 to UrButton; meta-actions come after.
typedef enum {
    UrEditItemSave = UrButtonCount,
    UrEditItemDiscard,
} UrEditItem;

typedef enum {
    UrKindItemSubGhz = 0,
    UrKindItemInfrared,
    UrKindItemClear,
} UrKindItem;

typedef struct {
    // Persistent state
    UrConfig* config; // live config (matches disk)
    UrConfig* edit_config; // pending edits while in editor

    // GUI handles
    Gui* gui;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    Submenu* edit_menu;
    Submenu* kind_menu;
    Submenu* signal_menu;
    UrViewId current_view;

    // Edit-time scratch
    UrButton editing_button;
    FuriString* scratch_path;
    FuriString* scratch_names[UR_SIGNAL_NAMES_CAP];
    size_t scratch_names_count;
    bool scratch_names_truncated;

    // Last-press feedback for the main view
    UrStatus status;
    UrButton last_pressed;
} UrApp;

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

static void switch_view(UrApp* app, UrViewId view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

// --------------------------------------------------------------------------
// Main view: draw + raw input
// --------------------------------------------------------------------------

static void view_draw_callback(Canvas* canvas, void* model) {
    UrApp* app = *(UrApp**)model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Universal Remote");

    canvas_set_font(canvas, FontSecondary);
    int y = 20;
    char line[UR_LABEL_BUF * 2];
    char summary[UR_LABEL_BUF];
    for(size_t i = 0; i < UrButtonCount; i++) {
        binding_summary(&app->config->bindings[i], summary, sizeof(summary));
        snprintf(line, sizeof(line), "%-5s %s", ur_button_to_name((UrButton)i), summary);
        canvas_draw_str(canvas, 0, y, line);
        y += 7;
    }

    canvas_draw_line(canvas, 0, 56, 128, 56);
    char status[48];
    if(app->last_pressed < UrButtonCount && app->status != UrStatusIdle) {
        snprintf(
            status,
            sizeof(status),
            "last: %s %s",
            ur_button_to_name(app->last_pressed),
            status_text(app->status));
    } else {
        snprintf(status, sizeof(status), "hold OK=edit  hold BACK=exit");
    }
    canvas_draw_str(canvas, 0, 64, status);
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

static bool view_input_callback(InputEvent* event, void* context) {
    UrApp* app = context;

    if(event->type == InputTypeLong) {
        if(event->key == InputKeyBack) {
            view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventExit);
            return true;
        }
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, UrCustomEventEnterEdit);
            return true;
        }
        // Any other long-press: ignore.
        return false;
    }

    if(event->type != InputTypeShort) return false;

    UrButton b = input_key_to_button(event->key);
    if(b >= UrButtonCount) return false;

    app->last_pressed = b;
    view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventDispatch + b);
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

static void do_dispatch(UrApp* app, UrButton b) {
    const UrBinding* bind = &app->config->bindings[b];
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        notify_status(app, UrStatusUnbound);
        return;
    }
    // LED-only mid-transmit feedback. The on-screen "TX…" state used to
    // live here, but the canvas can't redraw while the dispatcher thread
    // is blocked in ur_tx_*. LED is queued by the notification service.
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
            // Read one past the cap so we can flag truncation.
            while(flipper_format_read_string(fff, "name", name)) {
                if(app->scratch_names_count < UR_SIGNAL_NAMES_CAP) {
                    app->scratch_names[app->scratch_names_count++] =
                        furi_string_alloc_set(name);
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
        "Only first 64 signals shown.\nEdit config.txt for the rest.",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
}

// --------------------------------------------------------------------------
// Edit menus (forward decls)
// --------------------------------------------------------------------------

static void rebuild_edit_menu(UrApp* app);
static void rebuild_kind_menu(UrApp* app);
static void rebuild_signal_menu(UrApp* app);

static void edit_menu_callback(void* ctx, uint32_t index);
static void kind_menu_callback(void* ctx, uint32_t index);
static void signal_menu_callback(void* ctx, uint32_t index);

// --------------------------------------------------------------------------
// Edit flow
// --------------------------------------------------------------------------

static void enter_edit(UrApp* app) {
    // Snapshot the live config so the user can discard pending edits.
    ur_config_copy(app->edit_config, app->config);
    rebuild_edit_menu(app);
    switch_view(app, UrViewIdEditMenu);
}

static void exit_edit(UrApp* app, bool save) {
    if(save) {
        ur_config_copy(app->config, app->edit_config);
        if(!ur_config_save(app->config)) {
            // Best-effort: stay on the main view; user can re-try by
            // re-entering edit mode.
            notify_status(app, UrStatusFailed);
        }
    }
    switch_view(app, UrViewIdMain);
}

static void rebuild_edit_menu(UrApp* app) {
    submenu_reset(app->edit_menu);
    submenu_set_header(app->edit_menu, "Edit bindings");

    char label[UR_LABEL_BUF * 2];
    char summary[UR_LABEL_BUF];
    for(size_t i = 0; i < UrButtonCount; i++) {
        binding_summary(&app->edit_config->bindings[i], summary, sizeof(summary));
        snprintf(label, sizeof(label), "%s: %s", ur_button_to_name((UrButton)i), summary);
        submenu_add_item(app->edit_menu, label, (uint32_t)i, edit_menu_callback, app);
    }
    submenu_add_item(
        app->edit_menu, "Save & exit", UrEditItemSave, edit_menu_callback, app);
    submenu_add_item(
        app->edit_menu, "Discard & exit", UrEditItemDiscard, edit_menu_callback, app);
}

static void edit_menu_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    if(index < UrButtonCount) {
        app->editing_button = (UrButton)index;
        rebuild_kind_menu(app);
        switch_view(app, UrViewIdKindMenu);
    } else if(index == UrEditItemSave) {
        exit_edit(app, true);
    } else if(index == UrEditItemDiscard) {
        exit_edit(app, false);
    }
}

static void rebuild_kind_menu(UrApp* app) {
    submenu_reset(app->kind_menu);
    char header[UR_LABEL_BUF];
    snprintf(header, sizeof(header), "%s: action kind", ur_button_to_name(app->editing_button));
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
    // Separate preselect buffer so the browser's writes to `out` can't
    // clobber the preselected path mid-call.
    FuriString* preselect = furi_string_alloc_set(base);
    bool picked = dialog_file_browser_show(app->dialogs, out, preselect, &opts);
    furi_string_free(preselect);
    return picked;
}

static void kind_menu_callback(void* ctx, uint32_t index) {
    UrApp* app = ctx;
    UrBinding* bind = &app->edit_config->bindings[app->editing_button];

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
            // Cancelled the file picker.
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

    UrBinding* bind = &app->edit_config->bindings[app->editing_button];
    bind->kind = UrActionInfrared;
    furi_string_set(bind->path, app->scratch_path);
    furi_string_set(bind->name, app->scratch_names[index]);

    scratch_names_clear(app);
    rebuild_edit_menu(app);
    switch_view(app, UrViewIdEditMenu);
}

// --------------------------------------------------------------------------
// ViewDispatcher event wiring
// --------------------------------------------------------------------------

static bool custom_event_callback(void* context, uint32_t event) {
    UrApp* app = context;
    if(event >= UrCustomEventDispatch && event < UrCustomEventDispatch + UrButtonCount) {
        do_dispatch(app, (UrButton)(event - UrCustomEventDispatch));
        return true;
    }
    if(event == UrCustomEventEnterEdit) {
        enter_edit(app);
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
    // Submenus surface short BACK here. Map them back up the view stack.
    switch(app->current_view) {
    case UrViewIdSignalMenu:
        scratch_names_clear(app);
        switch_view(app, UrViewIdEditMenu);
        return true;
    case UrViewIdKindMenu:
        switch_view(app, UrViewIdEditMenu);
        return true;
    case UrViewIdEditMenu:
        // Short BACK from the edit root commits pending changes — matches
        // common Flipper editor UX. Use "Discard & exit" to abandon.
        exit_edit(app, true);
        return true;
    case UrViewIdMain:
    default:
        // Main view consumes BACK itself; getting here means the view
        // returned false unexpectedly. Don't exit.
        return true;
    }
}

// --------------------------------------------------------------------------
// App lifecycle
// --------------------------------------------------------------------------

static UrApp* app_alloc(void) {
    UrApp* app = malloc(sizeof(UrApp));
    app->config = ur_config_load();
    app->edit_config = malloc(sizeof(UrConfig));
    for(size_t i = 0; i < UrButtonCount; i++) {
        app->edit_config->bindings[i].kind = UrActionNone;
        app->edit_config->bindings[i].path = furi_string_alloc();
        app->edit_config->bindings[i].name = furi_string_alloc();
    }
    app->status = UrStatusIdle;
    app->last_pressed = UrButtonCount;
    app->editing_button = UrButtonUp;
    app->scratch_path = furi_string_alloc();
    app->scratch_names_count = 0;
    app->scratch_names_truncated = false;
    for(size_t i = 0; i < UR_SIGNAL_NAMES_CAP; i++) app->scratch_names[i] = NULL;
    app->current_view = UrViewIdMain;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Main view.
    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(UrApp*));
    with_view_model(app->main_view, UrApp ** m, { *m = app; }, true);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, view_draw_callback);
    view_set_input_callback(app->main_view, view_input_callback);

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
    view_dispatcher_add_view(app->view_dispatcher, UrViewIdMain, app->main_view);
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
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdMain);

    submenu_free(app->signal_menu);
    submenu_free(app->kind_menu);
    submenu_free(app->edit_menu);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    scratch_names_clear(app);
    furi_string_free(app->scratch_path);

    for(size_t i = 0; i < UrButtonCount; i++) {
        furi_string_free(app->edit_config->bindings[i].path);
        furi_string_free(app->edit_config->bindings[i].name);
    }
    free(app->edit_config);

    ur_config_free(app->config);
    free(app);
}

int32_t universal_remote_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "starting");

    ur_tx_init();
    UrApp* app = app_alloc();

    switch_view(app, UrViewIdMain);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    ur_tx_deinit();
    FURI_LOG_I(TAG, "exited");
    return 0;
}
