#include "gui_studio_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

// ─── Internal scene struct ────────────────────────────────────────────

struct GuiStudioScene {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* events;
    GuiStudioModel model;
};

// ─── Icon data ────────────────────────────────────────────────────────

// (no icons referenced)

// ─── Custom event hook (weak default; override in gui_studio.c) ─────────────

__attribute__((weak)) void gui_studio_on_event(int32_t event, GuiStudioModel* state) {
    UNUSED(event);
    UNUSED(state);
}

// ─── Per-screen draw helpers ──────────────────────────────────────────

static void draw_screen_main(Canvas* canvas, GuiStudioModel* state) {
    (void)state;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 9, "GUI Studio");
    canvas_draw_frame(canvas, 0, 13, 128, 1);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 28, "OK menu  Back exit");
    canvas_draw_frame(canvas, 34, 48, 60, 12);
    canvas_draw_str(canvas, 54, 56, "Menu");
}

static void draw_screen_menu(Canvas* canvas, GuiStudioModel* state) {
    (void)state;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 9, "Settings");
    {
        static const char* const items[] = { "Beep (event 1)", "Back to main" };
        const size_t count = 2;
        for(size_t i = 0; i < count; i++) {
            int iy = 14 + (int)i * 10;
            if(i == state->menu_cursor) {
                canvas_draw_box(canvas, 4, iy, 120, 10);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 6, iy + 8, items[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 6, iy + 8, items[i]);
            }
        }
    }
}

// ─── Master draw callback ─────────────────────────────────────────────

static void gui_studio_draw_callback(Canvas* canvas, void* ctx) {
    GuiStudioModel* state = ctx;
    canvas_clear(canvas);
    switch(state->screen) {
        case GuiStudioScreenMain: draw_screen_main(canvas, state); break;
        case GuiStudioScreenMenu: draw_screen_menu(canvas, state); break;
    }
}

// ─── Input handling ───────────────────────────────────────────────────
// Returns false to exit the app (Back on the root screen).

static bool gui_studio_handle_input(GuiStudioScene* scene, InputEvent* e) {
    GuiStudioModel* state = &scene->model;
    switch(state->screen) {
        case GuiStudioScreenMain:
            if(e->type == InputTypeShort) {
                if(e->key == InputKeyOk) { state->screen = GuiStudioScreenMenu; }
            }
            if(e->type == InputTypeShort && e->key == InputKeyBack) {
                if(state->screen == GuiStudioScreenMain) return false;
                state->screen = GuiStudioScreenMain;
            }
            break;
        case GuiStudioScreenMenu:
            if(e->type == InputTypeShort) {
                if(e->key == InputKeyUp) { if(state->menu_cursor > 0) state->menu_cursor--; }
                if(e->key == InputKeyDown) { if(state->menu_cursor < 1) state->menu_cursor++; }
                if(e->key == InputKeyOk) { switch(state->menu_cursor) { case 0: state->last_event = 1; gui_studio_on_event(1, state); break; case 1: state->screen = GuiStudioScreenMain; break; } }
            }
            if(e->type == InputTypeShort && e->key == InputKeyBack) {
                if(state->screen == GuiStudioScreenMain) return false;
                state->screen = GuiStudioScreenMain;
            }
            break;
    }
    return true;
}

static void gui_studio_input_callback(InputEvent* e, void* ctx) {
    GuiStudioScene* scene = ctx;
    furi_message_queue_put(scene->events, e, FuriWaitForever);
}

// ─── Public API ───────────────────────────────────────────────────────

GuiStudioScene* gui_studio_scene_alloc(void) {
    GuiStudioScene* scene = malloc(sizeof(GuiStudioScene));
    scene->model.screen = GuiStudioScreenMain;
    scene->model.last_event = 0;
    scene->model.menu_cursor = 0;

    scene->gui = furi_record_open(RECORD_GUI);
    scene->events = furi_message_queue_alloc(8, sizeof(InputEvent));
    scene->vp = view_port_alloc();
    view_port_draw_callback_set(scene->vp, gui_studio_draw_callback, &scene->model);
    view_port_input_callback_set(scene->vp, gui_studio_input_callback, scene);
    gui_add_view_port(scene->gui, scene->vp, GuiLayerFullscreen);
    return scene;
}

void gui_studio_scene_free(GuiStudioScene* scene) {
    if(!scene) return;
    view_port_enabled_set(scene->vp, false);
    gui_remove_view_port(scene->gui, scene->vp);
    view_port_free(scene->vp);
    furi_message_queue_free(scene->events);
    furi_record_close(RECORD_GUI);
    free(scene);
}

void gui_studio_scene_run(GuiStudioScene* scene) {
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(scene->events, &event, FuriWaitForever) == FuriStatusOk) {
            running = gui_studio_handle_input(scene, &event);
            view_port_update(scene->vp);
        }
    }
}

GuiStudioModel* gui_studio_scene_model(GuiStudioScene* scene) {
    return &scene->model;
}
