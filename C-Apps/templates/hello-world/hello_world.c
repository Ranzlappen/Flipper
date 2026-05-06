/*
 * Momentum App Framework  -  hello_world.c
 * -----------------------------------------------------------------------------
 * Minimal C "Hello World" app for Flipper Zero (Momentum firmware).
 * Shows a centered string on the display, waits for the user to press BACK,
 * and exits cleanly.
 *
 * Build with:    cd C-Apps/templates/hello-world && ufbt
 * Output .fap:   dist/hello_world.fap
 * Deploy to:     /ext/apps/Examples/hello_world.fap
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

typedef struct {
    FuriMessageQueue* input_queue;
} HelloWorldApp;

/* Render callback - called by the GUI subsystem when the screen needs to
 * redraw. Keep this fast and side-effect-free. */
static void hello_world_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Hello, Momentum!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press BACK to exit");
}

/* Input callback - just forwards events into our message queue so the main
 * loop can process them without blocking the GUI thread. */
static void hello_world_input_callback(InputEvent* input_event, void* ctx) {
    HelloWorldApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

/* Entry point referenced by application.fam (entry_point="hello_world_app"). */
int32_t hello_world_app(void* p) {
    UNUSED(p);

    HelloWorldApp app;
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, hello_world_draw_callback, &app);
    view_port_input_callback_set(view_port, hello_world_input_callback, &app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app.input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app.input_queue);
    return 0;
}
