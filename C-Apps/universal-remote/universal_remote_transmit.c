#include "universal_remote_transmit.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/types.h>
#include <lib/subghz/subghz_protocol_registry.h>

#include <lib/infrared/encoder_decoder/infrared.h>
#include <lib/infrared/worker/infrared_transmit.h>

#define TAG "UniversalRemote"

void ur_tx_init(void) {
    subghz_devices_init();
}

void ur_tx_deinit(void) {
    subghz_devices_deinit();
}

// ---------------------------------------------------------------------------
// Sub-GHz
// ---------------------------------------------------------------------------

static FuriHalSubGhzPreset preset_from_name(const char* name) {
    if(strcmp(name, "FuriHalSubGhzPresetOok270Async") == 0) {
        return FuriHalSubGhzPresetOok270Async;
    } else if(strcmp(name, "FuriHalSubGhzPresetOok650Async") == 0) {
        return FuriHalSubGhzPresetOok650Async;
    } else if(strcmp(name, "FuriHalSubGhzPreset2FSKDev238Async") == 0) {
        return FuriHalSubGhzPreset2FSKDev238Async;
    } else if(strcmp(name, "FuriHalSubGhzPreset2FSKDev476Async") == 0) {
        return FuriHalSubGhzPreset2FSKDev476Async;
    } else if(strcmp(name, "FuriHalSubGhzPresetMSK99_97KbAsync") == 0) {
        return FuriHalSubGhzPresetMSK99_97KbAsync;
    } else if(strcmp(name, "FuriHalSubGhzPresetGFSK9_99KbAsync") == 0) {
        return FuriHalSubGhzPresetGFSK9_99KbAsync;
    } else if(strcmp(name, "FuriHalSubGhzPresetCustom") == 0) {
        return FuriHalSubGhzPresetCustom;
    }
    return FuriHalSubGhzPresetIDLE;
}

bool ur_tx_subghz(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);

    FuriString* file_type = furi_string_alloc();
    FuriString* protocol_name = furi_string_alloc();
    FuriString* preset_name = furi_string_alloc();
    SubGhzEnvironment* env = NULL;
    SubGhzTransmitter* tx = NULL;
    uint8_t* preset_data = NULL;
    bool tx_started = false;
    const SubGhzDevice* device = NULL;
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(fff, path)) {
            FURI_LOG_E(TAG, "subghz: cannot open %s", path);
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(fff, file_type, &version)) {
            FURI_LOG_E(TAG, "subghz: bad header in %s", path);
            break;
        }

        uint32_t frequency = 0;
        if(!flipper_format_read_uint32(fff, "Frequency", &frequency, 1)) {
            FURI_LOG_E(TAG, "subghz: missing Frequency in %s", path);
            break;
        }

        if(!flipper_format_read_string(fff, "Preset", preset_name)) {
            FURI_LOG_E(TAG, "subghz: missing Preset in %s", path);
            break;
        }
        FuriHalSubGhzPreset preset = preset_from_name(furi_string_get_cstr(preset_name));
        if(preset == FuriHalSubGhzPresetCustom) {
            uint32_t data_size = 0;
            if(!flipper_format_get_value_count(fff, "Custom_preset_data", &data_size) ||
               data_size == 0) {
                FURI_LOG_E(TAG, "subghz: Custom preset missing Custom_preset_data");
                break;
            }
            preset_data = malloc(data_size);
            if(!flipper_format_read_hex(fff, "Custom_preset_data", preset_data, data_size)) {
                FURI_LOG_E(TAG, "subghz: unreadable Custom_preset_data");
                break;
            }
        }

        if(!flipper_format_read_string(fff, "Protocol", protocol_name)) {
            FURI_LOG_E(TAG, "subghz: missing Protocol in %s", path);
            break;
        }

        env = subghz_environment_alloc();
        subghz_environment_set_protocol_registry(env, (void*)&subghz_protocol_registry);
        // Best-effort keystore load: keeloq-family protocols need it; others don't care.
        subghz_environment_load_keystore(env, SUBGHZ_KEYSTORE_DIR_NAME);

        tx = subghz_transmitter_alloc_init(env, furi_string_get_cstr(protocol_name));
        if(!tx) {
            FURI_LOG_E(TAG, "subghz: unknown protocol %s", furi_string_get_cstr(protocol_name));
            break;
        }

        if(subghz_transmitter_deserialize(tx, fff) != SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "subghz: deserialize failed for %s", path);
            break;
        }

        device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!device) {
            FURI_LOG_E(TAG, "subghz: no internal radio device");
            break;
        }

        if(!subghz_devices_begin(device)) {
            FURI_LOG_E(TAG, "subghz: device begin failed");
            device = NULL;
            break;
        }
        subghz_devices_reset(device);
        subghz_devices_load_preset(device, preset, preset_data);
        subghz_devices_set_frequency(device, frequency);

        if(!subghz_devices_start_async_tx(device, subghz_transmitter_yield, tx)) {
            FURI_LOG_E(TAG, "subghz: start_async_tx refused (frequency %lu)", frequency);
            break;
        }
        tx_started = true;

        // Drain the encoder. yield() returns LEVEL_DURATION_RESET when done.
        // 30 s hard cap so a stuck encoder / bad .sub doesn't lock the app.
        // The RF was already driven during the wait, so the user's remote may
        // already have triggered — we still report success.
        const uint32_t timeout_ticks = furi_ms_to_ticks(30000);
        uint32_t start_tick = furi_get_tick();
        while(!subghz_devices_is_async_complete_tx(device)) {
            if(furi_get_tick() - start_tick > timeout_ticks) {
                FURI_LOG_W(TAG, "subghz: tx timeout after 30s, aborting wait");
                break;
            }
            furi_delay_ms(20);
        }
        ok = true;
    } while(0);

    if(tx_started && device) subghz_devices_stop_async_tx(device);
    if(device) {
        subghz_devices_sleep(device);
        subghz_devices_end(device);
    }
    if(tx) subghz_transmitter_free(tx);
    if(env) subghz_environment_free(env);
    free(preset_data);
    furi_string_free(file_type);
    furi_string_free(protocol_name);
    furi_string_free(preset_name);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ---------------------------------------------------------------------------
// Infrared
// ---------------------------------------------------------------------------

// Read 1..4 bytes of address/command into a uint32 (little-endian on disk).
static uint32_t bytes_le_to_u32(const uint8_t b[4]) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static bool send_parsed(FlipperFormat* fff) {
    FuriString* proto = furi_string_alloc();
    bool ok = false;
    do {
        if(!flipper_format_read_string(fff, "protocol", proto)) break;
        InfraredProtocol protocol =
            infrared_get_protocol_by_name(furi_string_get_cstr(proto));
        if(!infrared_is_protocol_valid(protocol)) {
            FURI_LOG_E(TAG, "ir: unknown protocol '%s'", furi_string_get_cstr(proto));
            break;
        }
        uint8_t addr[4] = {0};
        uint8_t cmd[4] = {0};
        if(!flipper_format_read_hex(fff, "address", addr, 4)) break;
        if(!flipper_format_read_hex(fff, "command", cmd, 4)) break;

        InfraredMessage msg = {
            .protocol = protocol,
            .address = bytes_le_to_u32(addr),
            .command = bytes_le_to_u32(cmd),
            .repeat = false,
        };
        infrared_send(&msg, 1);
        ok = true;
    } while(0);
    furi_string_free(proto);
    return ok;
}

static bool send_raw(FlipperFormat* fff) {
    uint32_t frequency = 0;
    float duty_cycle = 0.f;
    uint32_t count = 0;
    uint32_t* timings = NULL;
    bool ok = false;
    do {
        if(!flipper_format_read_uint32(fff, "frequency", &frequency, 1)) break;
        if(!flipper_format_read_float(fff, "duty_cycle", &duty_cycle, 1)) break;
        if(!flipper_format_get_value_count(fff, "data", &count) || count == 0) break;
        timings = malloc(count * sizeof(uint32_t));
        if(!flipper_format_read_uint32(fff, "data", timings, count)) break;

        infrared_send_raw_ext(timings, count, /*start_from_mark=*/true, frequency, duty_cycle);
        ok = true;
    } while(0);
    free(timings);
    return ok;
}

bool ur_tx_infrared(const char* path, const char* signal_name) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_buffered_file_alloc(storage);
    FuriString* header_type = furi_string_alloc();
    FuriString* name = furi_string_alloc();
    FuriString* type = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_buffered_file_open_existing(fff, path)) {
            FURI_LOG_E(TAG, "ir: cannot open %s", path);
            break;
        }
        uint32_t version = 0;
        if(!flipper_format_read_header(fff, header_type, &version)) {
            FURI_LOG_E(TAG, "ir: bad header in %s", path);
            break;
        }

        bool found = false;
        while(flipper_format_read_string(fff, "name", name)) {
            if(furi_string_cmp_str(name, signal_name) != 0) continue;
            if(!flipper_format_read_string(fff, "type", type)) break;
            if(furi_string_cmp_str(type, "parsed") == 0) {
                ok = send_parsed(fff);
            } else if(furi_string_cmp_str(type, "raw") == 0) {
                ok = send_raw(fff);
            } else {
                FURI_LOG_E(TAG, "ir: unknown signal type '%s'", furi_string_get_cstr(type));
            }
            found = true;
            break;
        }
        if(!found) FURI_LOG_E(TAG, "ir: signal '%s' not found in %s", signal_name, path);
    } while(0);

    furi_string_free(type);
    furi_string_free(name);
    furi_string_free(header_type);
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
