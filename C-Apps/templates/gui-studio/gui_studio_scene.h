#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GuiStudioScreenMain = 0,
    GuiStudioScreenMenu,
} GuiStudioScreen;

typedef struct {
    GuiStudioScreen screen;
    int32_t last_event;
    uint8_t menu_cursor;
} GuiStudioModel;

typedef struct GuiStudioScene GuiStudioScene;

GuiStudioScene* gui_studio_scene_alloc(void);
void              gui_studio_scene_free(GuiStudioScene* scene);
void              gui_studio_scene_run(GuiStudioScene* scene);
GuiStudioModel*  gui_studio_scene_model(GuiStudioScene* scene);

/* React to custom events emitted by buttons/menus. A weak no-op default
 * lives in gui_studio_scene.c; override it (e.g. in gui_studio.c) to add app logic. */
void              gui_studio_on_event(int32_t event, GuiStudioModel* state);

#ifdef __cplusplus
}
#endif
