// Universal Remote — Momentum App Framework
//
// Custom view captures raw hardware-button presses and dispatches them to a
// Sub-GHz or Infrared transmitter according to the on-disk button mapping.
// Edit /ext/apps_data/universal_remote/config.txt on the SD card to change
// what each button does. BACK always exits the app.

#include "universal_remote_config.h"
#include "universal_remote_transmit.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "UniversalRemote"

typedef enum {
    UrStatusIdle = 0,
    UrStatusTransmitting,
    UrStatusOk,
    UrStatusFailed,
    UrStatusUnbound,
} UrStatus;

typedef enum {
    UrViewIdMain = 0,
} UrViewId;

typedef enum {
    UrCustomEventStart = 100, // base; offset by UrButton index
} UrCustomEventBase;

typedef struct {
    UrConfig* config;
    ViewDispatcher* view_dispatcher;
    View* view;
    Gui* gui;
    NotificationApp* notifications;
    UrStatus status;
    UrButton last_pressed;
} UrApp;

// --------------------------------------------------------------------------
// View draw / input
// --------------------------------------------------------------------------

static const char* status_text(UrStatus s) {
    switch(s) {
    case UrStatusTransmitting:
        return "TX…";
    case UrStatusOk:
        return "OK";
    case UrStatusFailed:
        return "FAILED";
    case UrStatusUnbound:
        return "unbound";
    case UrStatusIdle:
    default:
        return "ready";
    }
}

static void draw_binding_line(Canvas* canvas, UrButton b, const UrBinding* bind, int y) {
    char line[64];
    const char* name = ur_button_to_name(b);
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        snprintf(line, sizeof(line), "%-5s -", name);
    } else if(bind->kind == UrActionSubGhz) {
        const char* p = furi_string_get_cstr(bind->path);
        const char* slash = strrchr(p, '/');
        snprintf(line, sizeof(line), "%-5s SG %s", name, slash ? slash + 1 : p);
    } else {
        const char* p = furi_string_get_cstr(bind->path);
        const char* slash = strrchr(p, '/');
        snprintf(
            line,
            sizeof(line),
            "%-5s IR %s/%s",
            name,
            slash ? slash + 1 : p,
            furi_string_get_cstr(bind->name));
    }
    canvas_draw_str(canvas, 0, y, line);
}

static void view_draw_callback(Canvas* canvas, void* model) {
    UrApp* app = *(UrApp**)model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Universal Remote");

    canvas_set_font(canvas, FontSecondary);
    int y = 22;
    for(size_t i = 0; i < UrButtonCount; i++) {
        draw_binding_line(canvas, (UrButton)i, &app->config->bindings[i], y);
        y += 8;
    }

    // Status line
    canvas_draw_line(canvas, 0, 56, 128, 56);
    char status[32];
    snprintf(status, sizeof(status), "%s %s", ur_button_to_name(app->last_pressed),
             status_text(app->status));
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
    default:
        return UrButtonCount;
    }
}

static bool view_input_callback(InputEvent* event, void* context) {
    UrApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyBack) return false; // let ViewDispatcher handle exit

    UrButton b = input_key_to_button(event->key);
    if(b >= UrButtonCount) return false;

    // Defer the actual transmit so the redraw happens on the right thread.
    app->last_pressed = b;
    view_dispatcher_send_custom_event(app->view_dispatcher, UrCustomEventStart + b);
    return true;
}

// --------------------------------------------------------------------------
// Transmit + UI feedback (runs on the ViewDispatcher thread)
// --------------------------------------------------------------------------

static void notify(UrApp* app, UrStatus s) {
    app->status = s;
    if(app->view) view_dispatcher_send_custom_event(app->view_dispatcher, 0); // force redraw

    if(s == UrStatusOk) {
        notification_message(app->notifications, &sequence_success);
    } else if(s == UrStatusFailed) {
        notification_message(app->notifications, &sequence_error);
    } else if(s == UrStatusUnbound) {
        notification_message(app->notifications, &sequence_blink_yellow_100);
    } else if(s == UrStatusTransmitting) {
        notification_message(app->notifications, &sequence_blink_blue_100);
    }
}

static void do_dispatch(UrApp* app, UrButton b) {
    const UrBinding* bind = &app->config->bindings[b];
    if(bind->kind == UrActionNone || furi_string_empty(bind->path)) {
        notify(app, UrStatusUnbound);
        return;
    }

    notify(app, UrStatusTransmitting);

    bool ok = false;
    if(bind->kind == UrActionSubGhz) {
        ok = ur_tx_subghz(furi_string_get_cstr(bind->path));
    } else if(bind->kind == UrActionInfrared) {
        ok = ur_tx_infrared(
            furi_string_get_cstr(bind->path), furi_string_get_cstr(bind->name));
    }

    notify(app, ok ? UrStatusOk : UrStatusFailed);
}

static bool custom_event_callback(void* context, uint32_t event) {
    UrApp* app = context;
    if(event >= UrCustomEventStart && event < UrCustomEventStart + UrButtonCount) {
        do_dispatch(app, (UrButton)(event - UrCustomEventStart));
    }
    // Force a redraw whatever happens.
    with_view_model(app->view, UrApp ** m, { *m = app; }, true);
    return true;
}

static bool navigation_event_callback(void* context) {
    (void)context;
    return false; // exit
}

// --------------------------------------------------------------------------
// App entry point
// --------------------------------------------------------------------------

static UrApp* app_alloc(void) {
    UrApp* app = malloc(sizeof(UrApp));
    app->config = ur_config_load();
    app->status = UrStatusIdle;
    app->last_pressed = UrButtonCount;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(UrApp*));
    with_view_model(app->view, UrApp ** m, { *m = app; }, true);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, view_draw_callback);
    view_set_input_callback(app->view, view_input_callback);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, navigation_event_callback);
    view_dispatcher_add_view(app->view_dispatcher, UrViewIdMain, app->view);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void app_free(UrApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, UrViewIdMain);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    app->gui = NULL;
    app->notifications = NULL;

    ur_config_free(app->config);
    free(app);
}

int32_t universal_remote_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "starting");

    ur_tx_init();
    UrApp* app = app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, UrViewIdMain);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    ur_tx_deinit();
    FURI_LOG_I(TAG, "exited");
    return 0;
}
