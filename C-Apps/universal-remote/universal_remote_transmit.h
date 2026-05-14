#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Process-wide init/deinit. Call once per app run.
void ur_tx_init(void);
void ur_tx_deinit(void);

// Transmit a saved Sub-GHz .sub file (path is absolute, e.g. /ext/subghz/foo.sub).
// Blocks until the transmission completes. Returns true on success.
bool ur_tx_subghz(const char* path);

// Transmit a named signal from a Flipper .ir file. signal_name must match one
// of the named signals inside the file. Blocks until the transmission
// completes. Returns true on success.
bool ur_tx_infrared(const char* path, const char* signal_name);

#ifdef __cplusplus
}
#endif
