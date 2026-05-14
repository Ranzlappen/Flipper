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
};

static const char default_config[] =
    "# Universal Remote — button mapping.\n"
    "#\n"
    "# Format:  BUTTON=KIND:PATH[,SIGNAL_NAME]\n"
    "#   BUTTON       UP | DOWN | LEFT | RIGHT | OK   (BACK always exits)\n"
    "#   KIND         subghz | ir\n"
    "#   PATH         full SD path to a .sub or .ir file\n"
    "#   SIGNAL_NAME  (IR only) name of the signal inside the .ir file\n"
    "#\n"
    "# Lines starting with '#' or blank lines are ignored.\n"
    "# Comment out a binding with '#' to disable it.\n"
    "#\n"
    "# Examples — replace with your own:\n"
    "#UP=subghz:/ext/subghz/garage_open.sub\n"
    "#DOWN=subghz:/ext/subghz/garage_close.sub\n"
    "#LEFT=ir:/ext/infrared/tv.ir,Vol_dn\n"
    "#RIGHT=ir:/ext/infrared/tv.ir,Vol_up\n"
    "#OK=ir:/ext/infrared/tv.ir,Power\n";

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

static void parse_line(UrConfig* cfg, FuriString* line) {
    // Trim leading whitespace
    furi_string_trim(line);
    if(furi_string_empty(line)) return;
    if(furi_string_get_char(line, 0) == '#') return;

    // Split on '='
    size_t eq = furi_string_search_char(line, '=');
    if(eq == FURI_STRING_FAILURE || eq == 0) return;

    FuriString* key = furi_string_alloc();
    FuriString* val = furi_string_alloc();
    furi_string_set_n(key, line, 0, eq);
    furi_string_set_n(val, line, eq + 1, furi_string_size(line) - eq - 1);
    furi_string_trim(key);
    furi_string_trim(val);

    int b = ur_button_from_name(furi_string_get_cstr(key));
    if(b < 0 || furi_string_empty(val)) {
        furi_string_free(key);
        furi_string_free(val);
        return;
    }

    // Split val on first ':' to get KIND:REST
    size_t colon = furi_string_search_char(val, ':');
    if(colon == FURI_STRING_FAILURE) {
        furi_string_free(key);
        furi_string_free(val);
        return;
    }
    FuriString* kind = furi_string_alloc();
    FuriString* rest = furi_string_alloc();
    furi_string_set_n(kind, val, 0, colon);
    furi_string_set_n(rest, val, colon + 1, furi_string_size(val) - colon - 1);
    furi_string_trim(kind);
    furi_string_trim(rest);

    UrBinding* bind = &cfg->bindings[b];

    if(furi_string_cmpi_str(kind, "subghz") == 0) {
        bind->kind = UrActionSubGhz;
        furi_string_set(bind->path, rest);
        furi_string_reset(bind->name);
    } else if(furi_string_cmpi_str(kind, "ir") == 0) {
        // rest is PATH[,SIGNAL_NAME]
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

            furi_string_free(path);
            furi_string_free(sig);
        }
    } else {
        FURI_LOG_W(TAG, "unknown action kind: %s", furi_string_get_cstr(kind));
    }

    furi_string_free(kind);
    furi_string_free(rest);
    furi_string_free(key);
    furi_string_free(val);
}

static void ensure_default_config(Storage* storage) {
    if(storage_common_stat(storage, UNIVERSAL_REMOTE_CONFIG_PATH, NULL) == FSE_OK) return;

    storage_common_mkdir(storage, UNIVERSAL_REMOTE_CONFIG_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, UNIVERSAL_REMOTE_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, default_config, strlen(default_config));
        FURI_LOG_I(TAG, "wrote default config to %s", UNIVERSAL_REMOTE_CONFIG_PATH);
    } else {
        FURI_LOG_E(TAG, "could not create %s", UNIVERSAL_REMOTE_CONFIG_PATH);
    }
    storage_file_close(file);
    storage_file_free(file);
}

UrConfig* ur_config_load(void) {
    UrConfig* cfg = malloc(sizeof(UrConfig));
    for(size_t i = 0; i < UrButtonCount; i++) {
        cfg->bindings[i].kind = UrActionNone;
        cfg->bindings[i].path = furi_string_alloc();
        cfg->bindings[i].name = furi_string_alloc();
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_default_config(storage);

    Stream* stream = buffered_file_stream_alloc(storage);
    bool opened = buffered_file_stream_open(
        stream, UNIVERSAL_REMOTE_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(opened) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            parse_line(cfg, line);
        }
        furi_string_free(line);
    } else {
        FURI_LOG_E(TAG, "config file unreadable: %s", UNIVERSAL_REMOTE_CONFIG_PATH);
    }

    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return cfg;
}

void ur_config_free(UrConfig* cfg) {
    if(!cfg) return;
    for(size_t i = 0; i < UrButtonCount; i++) {
        furi_string_free(cfg->bindings[i].path);
        furi_string_free(cfg->bindings[i].name);
    }
    free(cfg);
}
