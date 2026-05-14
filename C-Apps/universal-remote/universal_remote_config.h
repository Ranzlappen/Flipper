#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UNIVERSAL_REMOTE_CONFIG_DIR  EXT_PATH("apps_data/universal_remote")
#define UNIVERSAL_REMOTE_CONFIG_PATH UNIVERSAL_REMOTE_CONFIG_DIR "/config.txt"

typedef enum {
    UrButtonUp = 0,
    UrButtonDown,
    UrButtonLeft,
    UrButtonRight,
    UrButtonOk,
    UrButtonCount,
} UrButton;

typedef enum {
    UrActionNone = 0,
    UrActionSubGhz,
    UrActionInfrared,
} UrActionKind;

typedef struct {
    UrActionKind kind;
    FuriString* path; // file path on SD ("" if unset)
    FuriString* name; // signal name within file; only used for IR
} UrBinding;

typedef struct {
    UrBinding bindings[UrButtonCount];
} UrConfig;

// Returns a heap-allocated config. Caller owns and must call ur_config_free.
// If the on-disk config doesn't exist it writes a commented default and
// returns it parsed.
UrConfig* ur_config_load(void);
void ur_config_free(UrConfig* cfg);

// "UP" → UrButtonUp, etc. Returns -1 on miss.
int ur_button_from_name(const char* name);
const char* ur_button_to_name(UrButton b);

#ifdef __cplusplus
}
#endif
