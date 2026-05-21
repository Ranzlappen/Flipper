#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

// On-disk layout:
//   /ext/apps_data/universal_remote/
//     remotes/<name>.urcfg         <- one file per remote (current)
//     config.txt                   <- legacy single-config (migrated on first run)
//     config.txt.bak               <- post-migration backup
#define UNIVERSAL_REMOTE_DATA_DIR    EXT_PATH("apps_data/universal_remote")
#define UNIVERSAL_REMOTE_LEGACY_PATH UNIVERSAL_REMOTE_DATA_DIR "/config.txt"
#define UNIVERSAL_REMOTE_LEGACY_BAK  UNIVERSAL_REMOTE_DATA_DIR "/config.txt.bak"
#define UNIVERSAL_REMOTE_REMOTES_DIR UNIVERSAL_REMOTE_DATA_DIR "/remotes"
#define UR_REMOTE_EXT                ".urcfg"
#define UR_REMOTES_MAX               32
#define UR_NAME_MAX                  32

typedef enum {
    UrButtonUp = 0,
    UrButtonDown,
    UrButtonLeft,
    UrButtonRight,
    UrButtonOk,
    UrButtonBack,
    UrButtonCount,
} UrButton;

typedef enum {
    UrGestureShort = 0,
    UrGestureLong,
    UrGestureCount,
} UrGesture;

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

// One remote. UP/DOWN/LEFT/RIGHT use both gestures; OK/BACK use only [UrGestureShort].
typedef struct {
    FuriString* display_name; // human-readable name (NAME= in the file)
    FuriString* file_path; // absolute path to the .urcfg file backing this remote
    UrBinding bindings[UrButtonCount][UrGestureCount];
} UrConfig;

// Light-weight index of available remotes, populated by scanning remotes/.
typedef struct {
    FuriString* names[UR_REMOTES_MAX];
    FuriString* paths[UR_REMOTES_MAX];
    size_t count;
} UrRemoteIndex;

// -------- Per-remote config -----------------------------------------------

// Allocate an empty config (all bindings unset, name/path empty).
UrConfig* ur_config_alloc(void);

// Allocate + load from `path`. Returns a config with whatever could be parsed;
// missing/unreadable files yield an empty config with file_path set. NAME= in
// the file overrides the basename-derived display name.
UrConfig* ur_config_load(const char* path);
void ur_config_free(UrConfig* cfg);

// Serialize to `cfg->file_path`. Overwrites the file. Returns true on success.
bool ur_config_save(const UrConfig* cfg);

// Replace `dst` bindings + name with a deep copy of `src`. `file_path` is NOT
// copied — the editor uses this to snapshot live config while keeping its own
// disk binding.
void ur_config_copy(UrConfig* dst, const UrConfig* src);

// "UP" → UrButtonUp, etc. Returns -1 on miss.
int ur_button_from_name(const char* name);
const char* ur_button_to_name(UrButton b);

// Does this button support a long-press binding?
bool ur_button_has_long(UrButton b);

// -------- Remote index ----------------------------------------------------

// One-shot migration: ensures data dir + remotes/ exist; if remotes/ is empty
// and a legacy config.txt is present, parses it as a "Default" remote and
// renames the legacy file to config.txt.bak. Safe to call repeatedly.
void ur_remotes_migrate_if_needed(void);

// Walk remotes/ and return a sorted index of name/path pairs. Caller frees.
UrRemoteIndex* ur_remotes_scan(void);
void ur_remotes_free(UrRemoteIndex* idx);

// Sanitize `display_name` into a filesystem-safe basename and create an empty
// .urcfg in remotes/. Writes the full path into `out_path`. Returns false if
// the slot is already taken or I/O failed.
bool ur_remote_create(const char* display_name, FuriString* out_path);

// Delete the .urcfg at `path`.
bool ur_remote_delete(const char* path);

// Rename: write the existing bindings out under a new name (display + filename).
// `cfg` is updated in-place (display_name + file_path point to the new file).
// The old file is removed only if the new write succeeded.
bool ur_remote_rename(UrConfig* cfg, const char* new_display_name);

#ifdef __cplusplus
}
#endif
