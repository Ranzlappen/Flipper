#include "universal_remote_config.h"

#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <stream/buffered_file_stream.h>

#define TAG "UniversalRemote"

static const char* const button_names[UrButtonCount] = {
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
    "OK",
    "BACK",
};

static const char* const gesture_suffix[UrGestureCount] = {
    "_SHORT",
    "_LONG",
};

int ur_button_from_name(const char* name) {
    for(size_t i = 0; i < UrButtonCount; i++) {
        if(strcasecmp(name, button_names[i]) == 0) return (int)i;
    }
    return -1;
}

const char* ur_button_to_name(UrButton b) {
    if(b >= UrButtonCount) return "?";
    return button_names[b];
}

bool ur_button_has_long(UrButton b) {
    return b == UrButtonUp || b == UrButtonDown || b == UrButtonLeft || b == UrButtonRight;
}

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

static void binding_init(UrBinding* b) {
    b->kind = UrActionNone;
    b->path = furi_string_alloc();
    b->name = furi_string_alloc();
}

static void binding_free(UrBinding* b) {
    furi_string_free(b->path);
    furi_string_free(b->name);
}

static void binding_reset(UrBinding* b) {
    b->kind = UrActionNone;
    furi_string_reset(b->path);
    furi_string_reset(b->name);
}

// Parse "subghz:/path" or "ir:/path,Signal" into `bind`. Returns false on
// syntax error (caller leaves the binding untouched in that case).
static bool parse_value(UrBinding* bind, FuriString* val) {
    size_t colon = furi_string_search_char(val, ':');
    if(colon == FURI_STRING_FAILURE) return false;

    FuriString* kind = furi_string_alloc();
    FuriString* rest = furi_string_alloc();
    furi_string_set_n(kind, val, 0, colon);
    furi_string_set_n(rest, val, colon + 1, furi_string_size(val) - colon - 1);
    furi_string_trim(kind);
    furi_string_trim(rest);

    bool ok = false;
    if(furi_string_cmpi_str(kind, "subghz") == 0) {
        bind->kind = UrActionSubGhz;
        furi_string_set(bind->path, rest);
        furi_string_reset(bind->name);
        ok = true;
    } else if(furi_string_cmpi_str(kind, "ir") == 0) {
        size_t comma = furi_string_search_char(rest, ',');
        if(comma == FURI_STRING_FAILURE) {
            FURI_LOG_W(TAG, "IR binding missing ,signal_name: %s", furi_string_get_cstr(rest));
        } else {
            FuriString* path = furi_string_alloc();
            FuriString* sig = furi_string_alloc();
            furi_string_set_n(path, rest, 0, comma);
            furi_string_set_n(sig, rest, comma + 1, furi_string_size(rest) - comma - 1);
            furi_string_trim(path);
            furi_string_trim(sig);

            bind->kind = UrActionInfrared;
            furi_string_set(bind->path, path);
            furi_string_set(bind->name, sig);
            ok = true;

            furi_string_free(path);
            furi_string_free(sig);
        }
    } else {
        FURI_LOG_W(TAG, "unknown action kind: %s", furi_string_get_cstr(kind));
    }

    furi_string_free(kind);
    furi_string_free(rest);
    return ok;
}

// Parse "BUTTON_SUFFIX" into (button, gesture). Bare "BUTTON" (no suffix) is
// treated as short. Returns false if BUTTON is unknown.
static bool parse_key(const char* key, UrButton* out_b, UrGesture* out_g) {
    // Find optional "_SHORT" / "_LONG" suffix.
    const char* underscore = strrchr(key, '_');
    UrGesture g = UrGestureShort;
    char button_part[16];
    if(underscore && (strcasecmp(underscore, "_SHORT") == 0)) {
        g = UrGestureShort;
        size_t n = (size_t)(underscore - key);
        if(n >= sizeof(button_part)) return false;
        memcpy(button_part, key, n);
        button_part[n] = '\0';
    } else if(underscore && (strcasecmp(underscore, "_LONG") == 0)) {
        g = UrGestureLong;
        size_t n = (size_t)(underscore - key);
        if(n >= sizeof(button_part)) return false;
        memcpy(button_part, key, n);
        button_part[n] = '\0';
    } else {
        // Bare key — interpret as short.
        if(strlen(key) >= sizeof(button_part)) return false;
        strcpy(button_part, key);
    }

    int b = ur_button_from_name(button_part);
    if(b < 0) return false;
    *out_b = (UrButton)b;
    *out_g = g;
    return true;
}

static void parse_line(UrConfig* cfg, FuriString* line) {
    furi_string_trim(line);
    if(furi_string_empty(line)) return;
    if(furi_string_get_char(line, 0) == '#') return;

    size_t eq = furi_string_search_char(line, '=');
    if(eq == FURI_STRING_FAILURE || eq == 0) return;

    FuriString* key = furi_string_alloc();
    FuriString* val = furi_string_alloc();
    furi_string_set_n(key, line, 0, eq);
    furi_string_set_n(val, line, eq + 1, furi_string_size(line) - eq - 1);
    furi_string_trim(key);
    furi_string_trim(val);

    // NAME=... sets the display name.
    if(furi_string_cmpi_str(key, "NAME") == 0) {
        if(!furi_string_empty(val)) furi_string_set(cfg->display_name, val);
        furi_string_free(key);
        furi_string_free(val);
        return;
    }

    UrButton b;
    UrGesture g;
    if(!parse_key(furi_string_get_cstr(key), &b, &g)) {
        furi_string_free(key);
        furi_string_free(val);
        return;
    }

    UrBinding* bind = &cfg->bindings[b][g];
    if(furi_string_empty(val)) {
        // Empty value: explicit unbind.
        binding_reset(bind);
    } else {
        parse_value(bind, val);
    }

    furi_string_free(key);
    furi_string_free(val);
}

// Pull "garage" out of "/ext/apps_data/universal_remote/remotes/garage.urcfg".
static void basename_no_ext(const char* path, FuriString* out) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    const char* dot = strrchr(base, '.');
    if(dot && dot > base) {
        furi_string_set_strn(out, base, (size_t)(dot - base));
    } else {
        furi_string_set(out, base);
    }
}

// --------------------------------------------------------------------------
// Public config API
// --------------------------------------------------------------------------

UrConfig* ur_config_alloc(void) {
    UrConfig* cfg = malloc(sizeof(UrConfig));
    cfg->display_name = furi_string_alloc();
    cfg->file_path = furi_string_alloc();
    for(size_t b = 0; b < UrButtonCount; b++) {
        for(size_t g = 0; g < UrGestureCount; g++) {
            binding_init(&cfg->bindings[b][g]);
        }
    }
    return cfg;
}

void ur_config_free(UrConfig* cfg) {
    if(!cfg) return;
    for(size_t b = 0; b < UrButtonCount; b++) {
        for(size_t g = 0; g < UrGestureCount; g++) {
            binding_free(&cfg->bindings[b][g]);
        }
    }
    furi_string_free(cfg->display_name);
    furi_string_free(cfg->file_path);
    free(cfg);
}

UrConfig* ur_config_load(const char* path) {
    UrConfig* cfg = ur_config_alloc();
    if(path) furi_string_set(cfg->file_path, path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);

    if(buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            parse_line(cfg, line);
        }
        furi_string_free(line);
    } else {
        FURI_LOG_E(TAG, "config file unreadable: %s", path);
    }

    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    // If no NAME= was found in the file, derive one from the filename.
    if(furi_string_empty(cfg->display_name)) {
        basename_no_ext(path, cfg->display_name);
    }

    return cfg;
}

bool ur_config_save(const UrConfig* cfg) {
    if(!cfg || furi_string_empty(cfg->file_path)) return false;
    const char* path = furi_string_get_cstr(cfg->file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, UNIVERSAL_REMOTE_REMOTES_DIR);

    Stream* stream = buffered_file_stream_alloc(storage);
    bool ok = false;
    if(buffered_file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(
            stream,
            "# Universal Remote profile — generated by the on-device editor.\n"
            "# Per-direction short / long bindings; OK and BACK are short-only.\n"
            "# Short-OK / short-BACK fire their bindings; hold-OK opens the editor\n"
            "# and hold-BACK returns to the remote list (short-BACK also returns to\n"
            "# the list when its binding is empty).\n");

        FuriString* line = furi_string_alloc();
        furi_string_printf(line, "NAME=%s\n", furi_string_get_cstr(cfg->display_name));
        stream_write_string(stream, line);

        for(size_t b = 0; b < UrButtonCount; b++) {
            for(size_t g = 0; g < UrGestureCount; g++) {
                if(g == UrGestureLong && !ur_button_has_long((UrButton)b)) continue;

                const UrBinding* bind = &cfg->bindings[b][g];
                const char* btn = ur_button_to_name((UrButton)b);
                const char* suffix = ur_button_has_long((UrButton)b) ? gesture_suffix[g] : "";

                if(bind->kind == UrActionSubGhz && !furi_string_empty(bind->path)) {
                    furi_string_printf(
                        line, "%s%s=subghz:%s\n", btn, suffix, furi_string_get_cstr(bind->path));
                } else if(bind->kind == UrActionInfrared && !furi_string_empty(bind->path)) {
                    furi_string_printf(
                        line,
                        "%s%s=ir:%s,%s\n",
                        btn,
                        suffix,
                        furi_string_get_cstr(bind->path),
                        furi_string_get_cstr(bind->name));
                } else {
                    furi_string_printf(line, "%s%s=\n", btn, suffix);
                }
                stream_write_string(stream, line);
            }
        }
        furi_string_free(line);
        ok = true;
        FURI_LOG_I(TAG, "saved %s", path);
    } else {
        FURI_LOG_E(TAG, "could not write %s", path);
    }

    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void ur_config_copy(UrConfig* dst, const UrConfig* src) {
    if(!dst || !src) return;
    furi_string_set(dst->display_name, src->display_name);
    for(size_t b = 0; b < UrButtonCount; b++) {
        for(size_t g = 0; g < UrGestureCount; g++) {
            dst->bindings[b][g].kind = src->bindings[b][g].kind;
            furi_string_set(dst->bindings[b][g].path, src->bindings[b][g].path);
            furi_string_set(dst->bindings[b][g].name, src->bindings[b][g].name);
        }
    }
}

// --------------------------------------------------------------------------
// Remote index + migration
// --------------------------------------------------------------------------

// Allow [A-Za-z0-9 _-] in filenames; map everything else to '_'. Multiple
// underscores collapse to one. Empty input → "remote".
static void sanitize_basename(const char* in, FuriString* out) {
    furi_string_reset(out);
    bool last_underscore = false;
    for(const char* p = in; *p; p++) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_';
        if(c == ' ') {
            c = '_';
            ok = true;
        }
        if(ok) {
            if(c == '_' && last_underscore) continue;
            furi_string_push_back(out, c);
            last_underscore = (c == '_');
        }
    }
    if(furi_string_empty(out)) furi_string_set(out, "remote");
}

static bool path_exists(Storage* storage, const char* path) {
    return storage_common_stat(storage, path, NULL) == FSE_OK;
}

void ur_remotes_migrate_if_needed(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_mkdir(storage, UNIVERSAL_REMOTE_DATA_DIR);
    storage_common_mkdir(storage, UNIVERSAL_REMOTE_REMOTES_DIR);

    // If remotes/ already has at least one .urcfg, we're done.
    File* dir = storage_file_alloc(storage);
    bool has_existing = false;
    if(storage_dir_open(dir, UNIVERSAL_REMOTE_REMOTES_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!file_info_is_dir(&info) && strstr(name, UR_REMOTE_EXT) != NULL) {
                has_existing = true;
                break;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    if(has_existing) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // No existing remotes — try to migrate legacy config.txt.
    if(!path_exists(storage, UNIVERSAL_REMOTE_LEGACY_PATH)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FURI_LOG_I(TAG, "migrating legacy config.txt → remotes/default.urcfg");

    UrConfig* cfg = ur_config_alloc();
    furi_string_set(cfg->display_name, "Default");
    furi_string_set(
        cfg->file_path, UNIVERSAL_REMOTE_REMOTES_DIR "/default" UR_REMOTE_EXT);

    // Parse the legacy file with the new parser. Bare BUTTON= keys map to
    // _SHORT via parse_key's fallback, so the conversion is automatic.
    Stream* stream = buffered_file_stream_alloc(storage);
    if(buffered_file_stream_open(
           stream, UNIVERSAL_REMOTE_LEGACY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            parse_line(cfg, line);
        }
        furi_string_free(line);
    }
    buffered_file_stream_close(stream);
    stream_free(stream);

    furi_record_close(RECORD_STORAGE);

    if(ur_config_save(cfg)) {
        // Preserve the original under a .bak suffix so the user can recover.
        Storage* s2 = furi_record_open(RECORD_STORAGE);
        // Remove any old .bak first; rename fails otherwise.
        if(path_exists(s2, UNIVERSAL_REMOTE_LEGACY_BAK)) {
            storage_common_remove(s2, UNIVERSAL_REMOTE_LEGACY_BAK);
        }
        FS_Error rename_err = storage_common_rename(
            s2, UNIVERSAL_REMOTE_LEGACY_PATH, UNIVERSAL_REMOTE_LEGACY_BAK);
        if(rename_err != FSE_OK) {
            FURI_LOG_W(TAG, "could not rename legacy config: %d", rename_err);
        }
        furi_record_close(RECORD_STORAGE);
    } else {
        FURI_LOG_E(TAG, "migration save failed; leaving legacy config in place");
    }

    ur_config_free(cfg);
}

UrRemoteIndex* ur_remotes_scan(void) {
    UrRemoteIndex* idx = malloc(sizeof(UrRemoteIndex));
    idx->count = 0;
    for(size_t i = 0; i < UR_REMOTES_MAX; i++) {
        idx->names[i] = NULL;
        idx->paths[i] = NULL;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, UNIVERSAL_REMOTE_REMOTES_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name)) && idx->count < UR_REMOTES_MAX) {
            if(file_info_is_dir(&info)) continue;
            // Only files ending in our extension.
            size_t nlen = strlen(name);
            size_t elen = strlen(UR_REMOTE_EXT);
            if(nlen <= elen || strcmp(name + nlen - elen, UR_REMOTE_EXT) != 0) continue;

            FuriString* path = furi_string_alloc();
            furi_string_printf(path, "%s/%s", UNIVERSAL_REMOTE_REMOTES_DIR, name);

            // Peek at NAME= without doing a full load.
            FuriString* display = furi_string_alloc();
            Stream* peek = buffered_file_stream_alloc(storage);
            if(buffered_file_stream_open(
                   peek, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
                FuriString* line = furi_string_alloc();
                while(stream_read_line(peek, line)) {
                    furi_string_trim(line);
                    if(furi_string_start_with_str(line, "NAME=") ||
                       furi_string_start_with_str(line, "name=") ||
                       furi_string_start_with_str(line, "Name=")) {
                        furi_string_right(line, 5);
                        furi_string_trim(line);
                        furi_string_set(display, line);
                        break;
                    }
                }
                furi_string_free(line);
            }
            buffered_file_stream_close(peek);
            stream_free(peek);

            if(furi_string_empty(display)) {
                basename_no_ext(furi_string_get_cstr(path), display);
            }

            idx->names[idx->count] = display;
            idx->paths[idx->count] = path;
            idx->count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    // Sort by display name (case-insensitive).
    for(size_t i = 0; i + 1 < idx->count; i++) {
        for(size_t j = i + 1; j < idx->count; j++) {
            if(strcasecmp(
                   furi_string_get_cstr(idx->names[i]),
                   furi_string_get_cstr(idx->names[j])) > 0) {
                FuriString* tn = idx->names[i];
                idx->names[i] = idx->names[j];
                idx->names[j] = tn;
                FuriString* tp = idx->paths[i];
                idx->paths[i] = idx->paths[j];
                idx->paths[j] = tp;
            }
        }
    }

    return idx;
}

void ur_remotes_free(UrRemoteIndex* idx) {
    if(!idx) return;
    for(size_t i = 0; i < idx->count; i++) {
        furi_string_free(idx->names[i]);
        furi_string_free(idx->paths[i]);
    }
    free(idx);
}

// Build "/ext/.../remotes/<base>.urcfg" into `out` using `display_name` as
// the source for the basename.
static void path_from_name(const char* display_name, FuriString* out) {
    FuriString* base = furi_string_alloc();
    sanitize_basename(display_name, base);
    furi_string_printf(
        out, "%s/%s%s", UNIVERSAL_REMOTE_REMOTES_DIR, furi_string_get_cstr(base), UR_REMOTE_EXT);
    furi_string_free(base);
}

bool ur_remote_create(const char* display_name, FuriString* out_path) {
    UrConfig* cfg = ur_config_alloc();
    furi_string_set(cfg->display_name, display_name);
    path_from_name(display_name, cfg->file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = path_exists(storage, furi_string_get_cstr(cfg->file_path));
    furi_record_close(RECORD_STORAGE);

    if(exists) {
        FURI_LOG_W(TAG, "remote already exists: %s", furi_string_get_cstr(cfg->file_path));
        ur_config_free(cfg);
        return false;
    }

    bool ok = ur_config_save(cfg);
    if(ok && out_path) furi_string_set(out_path, cfg->file_path);
    ur_config_free(cfg);
    return ok;
}

bool ur_remote_delete(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error err = storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "delete failed (%d): %s", err, path);
        return false;
    }
    return true;
}

bool ur_remote_rename(UrConfig* cfg, const char* new_display_name) {
    if(!cfg) return false;

    FuriString* new_path = furi_string_alloc();
    path_from_name(new_display_name, new_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool collision = path_exists(storage, furi_string_get_cstr(new_path));
    furi_record_close(RECORD_STORAGE);

    if(collision &&
       furi_string_cmp(new_path, cfg->file_path) != 0) {
        // Target filename already taken by a different remote.
        furi_string_free(new_path);
        return false;
    }

    FuriString* old_path = furi_string_alloc_set(cfg->file_path);
    furi_string_set(cfg->display_name, new_display_name);
    furi_string_set(cfg->file_path, new_path);

    bool ok = ur_config_save(cfg);
    if(ok && furi_string_cmp(old_path, new_path) != 0) {
        // Drop the old file. If this fails we still have the new one written.
        Storage* s2 = furi_record_open(RECORD_STORAGE);
        storage_common_remove(s2, furi_string_get_cstr(old_path));
        furi_record_close(RECORD_STORAGE);
    }

    furi_string_free(old_path);
    furi_string_free(new_path);
    return ok;
}
