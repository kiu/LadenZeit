# CLAUDE.md

Guidance for working on the **LadenZeit** firmware in this directory.

> **Keep this file in sync.** Whenever you change something this document
> describes — architecture, the state-machine flow, the modules table, the
> "places" wire format, build/networking config, conventions, or the "before
> shipping" list — update the relevant section in the same change. Before
> finishing a task, verify the affected sections still match the code (file
> paths, function names, constants). Treat a drift between code and CLAUDE.md as
> a bug to fix, not to leave.

## What this is

LadenZeit ("opening times") is firmware for a small desk gadget built around an
**ESP32-C6**. It shows the opening hours / open-closed status of a configurable
list of "places" (shops), fetched from a cloud IoT API, on a **256×64 SSD1322
OLED**, driven by a single **rotary encoder with push button** ("dial"). It is an
Arduino `.ino` sketch (Arduino-ESP32 core), not a PlatformIO/CMake project.

Hardware revision: REV_B.

## Build / flash

- Arduino IDE or `arduino-cli`, board = ESP32-C6.
- No build files are checked in here; the sketch is the unit of compilation.
  All `.ino`/`.cpp`/`.h` in this folder compile together.
- Required libraries: `U8g2` (display), `ESP32Time` (RTC), plus the ESP32 core
  (`WiFi`, `HTTPClient`, `WiFiClientSecure`, `Preferences`, `nvs_flash`,
  `esp_sleep`, `esp_wifi`, `esp_bt`).

### Local config & secrets (`lz_secrets.h`)
Machine-local settings live in `lz_secrets.h`, which is **gitignored**.
`lz_config.h` includes it optionally via `__has_include`, so a fresh checkout
without the file compiles as a **production** image. Copy
`lz_secrets.example.h` → `lz_secrets.h` and edit to activate development.

There is **one toggle: `DEV_MODE`** (defined in `lz_secrets.h`). Defining it
switches every development behavior on at once:
| `DEV_MODE` | Development (defined) | Production (undefined) |
|-----------|-----------------------|------------------------|
| WiFi creds | hardcoded `DEV_WIFI_SSID`/`_PASS` | from NVS (provisioning menu) |
| Device id | hardcoded `DEV_DEVICE_ID` | generated once, stored in NVS |
| Endpoint | `http://machariel.qnet:8080/...` (plain HTTP) | `https://iot.nakamura-labs.com/...` + pinned CA |
| Sleep | disabled (never sleeps) | enabled (`SLEEP_IN_SECONDS`) |
| Serial | `LOG*` macros print | `LOG*` compile to nothing — **zero serial output** |

`DEV_WIFI_*` / `DEV_DEVICE_ID` are the concrete values, defined inside the
`DEV_MODE` guard in `lz_secrets.h`. The pinned root CA (`iotRootCa`) is checked
in — a CA cert is not a secret.

### Logging
All serial output goes through the `LOG` / `LOG_BEGIN` / `LOG_FLUSH` macros in
`lz_config.h` — never call `Serial.*` directly. In production these expand to
`((void)0)`, so a production build emits nothing (serial dies at first sleep
anyway). Any `.cpp` that logs must `#include "lz_config.h"`.

## Architecture

Everything is coordinated by a **global state machine**. `AppState` (in
`LadenZeit.h`) enumerates every screen; three globals drive it:
`state`, `stateNext`, `statePrev` (plus `stateSsidPrev` for return paths).

Each screen is one row in the **`STATES[]` dispatch table** (in `LadenZeit.ino`,
indexed by `AppState`), holding up to three handlers — `onEnter`, `onButton`,
`onRender` (any may be null). The three `loop*` passes are generic dispatchers
over that table:
1. **`loopState()`** — on a `state -> stateNext` transition, restores/resets the
   nav window (`mainNavStart`/`mainNavSelected`), then calls the state's
   `onEnter` (sets `mainNavCount`/`mainNavPerPage` + side effects like network
   calls / wifi scan; may set `stateNext` for action-only states).
2. **`loopInteraction()`** — reads the dial. Button press → the state's
   `onButton` decides the next state. Rotation → `mainNavDial()` moves the
   selection/scroll window.
3. **`loopOled()`** — if `oledUpdate` is set, calls the state's `onRender`
   (an `oledShow*` call), bracketed by buffer clear/send.

A null handler for a phase logs an "undefined state" notice (matching the old
`switch` defaults); `STATE_MENU_PLACES` keeps an empty non-null `onRender` so it
stays silent. `STATE_COUNT` (last enum member) sizes the table.

The **one intentional exception** to "rendering goes through `onRender`": a
blocking network op (`mainNetwork`, run from an `onEnter`) parks `loop()` for
seconds, so it pushes a live progress screen straight to the panel via
`oledShowConnectionStatus()` (which self-clears/sends). This stays synchronous on
purpose — the device has nothing else to do mid-fetch (see Option B in TODO for
the async alternative, deferred). `oledShowSplash` (setup only) draws directly
for the same reason.

If nothing changed for `SLEEP_IN_SECONDS` (120), `enterSleep()` puts the chip in
light sleep, waking **only** on encoder GPIO (enabled once in `dialSetup`). No
timed wake: the display is off while asleep so nothing needs refreshing, and the
system clock keeps running (see Time model), so time is already correct on wake.

### Modules
| File | Responsibility |
|------|----------------|
| `LadenZeit.ino` / `.h` | `setup`/`loop`, the `STATES[]` dispatch table + per-state handlers, navigation math, sleep |
| `lz_dial.{cpp,h}` | Rotary encoder + button, quadrature decode & debounce in ISRs |
| `lz_oled.{cpp,h}` | All U8g2 rendering: partials (`oledRender*`) and screens (`oledShow*`) |
| `lz_network.{cpp,h}` | WiFi connect/scan, HTTPS client, API calls, credential storage in NVS |
| `lz_places.{cpp,h}` | Parses the downloaded binary "places" blob; accessors for names/slots |
| `lz_time.{cpp,h}` | Wall-clock reads off the ESP32 RTC/system clock; the cache-expiry epoch |
| `lz_config.h` | Optional include of `lz_secrets.h`; `DEV_MODE`-gated `LOG*` macros |

### The "places" wire format (`lz_places.cpp`)
A raw byte buffer (`placesBuffer`, max 1024 B) downloaded from the API and parsed
in place — **no JSON**. Layout:
- `[0..3]` big-endian `uint32` **wall-clock epoch** (server-local time as
  seconds-since-1970, not true UTC — see Time model; used to seed the clock),
- `[4]` place count,
- then per place: `len`, name bytes, `slotCount`, then `slotCount × 4` bytes.
- Each slot is two big-endian `uint16`s: `from` then `to`, each a **week-minute**
  (`day*1440 + hour*60 + min`, `0 = Sunday`). Slots are sorted and
  non-overlapping; a slot may cross midnight or the Sat→Sun week boundary
  (`to < from`), so open/closed math is done modulo the week (`placesStatus`).
  24/7 places are filtered out server-side (`to == from` never occurs). The
  per-day detail view (`oledRenderPlaceDay`) clamps slots to each day's window
  and renders the week **Monday-first** for display, though the encoding and all
  computation stay Sunday-based.

`placesValidate(len)` walks this structure with full bounds checking right after
download (called from `httpDownloadPlaces`); a malformed payload is rejected
(`placesReset()` + `NETWORK_ERROR_DATA`) before any accessor reads it, so the
`placesIndex`/`placesName`/`placesSlot` accessors can trust the buffer.

### Time model
There is no NTP. The **ESP32 RTC/system clock** (`ESP32Time rtc`) is the single
source of truth, seeded from the first 4 bytes of each places download via
`timeSet(epoch)`, which sets the RTC directly. The value is a **wall-clock
epoch**: the server's local time as seconds-since-1970 (real UTC epoch +
timezone/DST offset, computed server-side), *not* a true UTC epoch. `rtc` has
offset 0, so it's interpreted verbatim and `timeDayGet`/`timeMinutesGet` read
back the intended local calendar values off `rtc` (`getDayofWeek()` is
0=Sunday..6=Saturday). This keeps all timezone/DST
knowledge on the server; the device stays tz-agnostic, at the cost of the clock
being briefly wrong after a DST change until the next fetch. There is **no timer ISR** — the RTC/LP domain keeps advancing through
light sleep, so time survives sleep with no catch-up. `loop()` detects
minute-of-day changes by comparison and forces a redraw (this doesn't count as
activity, so it never defers sleep). Cache expiry is an epoch comparison:
`timeCacheInit()` records `rtc.getEpoch() + 3600`, `timeCacheExpired()` compares
against `rtc.getEpoch()`.

## Conventions

- Module prefix naming: `lz_<module>` files, functions prefixed by domain
  (`oled*`, `wifi*`/`http*`, `places*`, `time*`, `dial*`, `main*`).
- `camelCase` for variables/functions; `UPPER_SNAKE` for macros and file-scope
  `const` config (`OLED_WIDTH`, `WIFI_PASS_CHARS`). Don't reintroduce
  `snake_case` locals/globals.
- **Declarations first:** `#define`s, file-scope `const`/config, and module-global
  variables live at the **top of the file**, above the functions — not interleaved
  between them. The one forced exception is the `STATES[]` dispatch table
  (`LadenZeit.ino`): it holds pointers to the handler functions, so it must follow
  them.
- Text/drawing APIs take `const char*`, not `String` — a literal or `.c_str()`
  then passes with no heap allocation. Don't build `String` temporaries in render
  code (it runs every redraw and every minute → heap fragmentation on this MCU);
  format into a stack `char` buffer with `snprintf(buf, sizeof buf, …)` (never
  `sprintf`), and hand back data via a caller buffer (see `placesName()`).
  `String` is reserved for genuinely dynamic, persistent state (`mainPass`,
  `mainSsid`).
- Screens: add a `STATE_*` enum value (before `STATE_COUNT`), a matching row in
  the `STATES[]` table (positional — keep it at the **same position as the enum**),
  and its `st*Enter`/`st*Button`/`st*Render` handlers (leave any unused phase
  `nullptr`). A `static_assert` guards the row count. The table is positional (not
  `[STATE_X] = {…}`) on purpose: this toolchain (Arduino ESP32 GCC, gnu++17)
  **rejects C++ array designated initializers** ("sorry, unimplemented"). Same
  reason to prefer plain arrays elsewhere.
- Display layout uses hardcoded pixel constants (`OLED_HEAD`, `OLED_LINE`,
  `OLED_L3_*`, `OLED_ROW_TOP`/`OLED_ROW_BASE` for the bottom selectable row) and
  literal coordinates for one-off offsets.
- Don't hardcode list indices/counts: main-menu selections use the `MenuItem`
  enum (`LadenZeit.h`, kept in sync with `oledRenderIcon4MenuItems[]` via
  `static_assert`); per-screen paging uses the `*_PER_PAGE` constants
  (`lz_oled.h`); the pass-entry keyboard layout goes through
  `wifiPassSlotKey()`/`wifiPassSlotCount()` (`lz_network`).
- Module-global state is the norm; ISR-shared variables are `volatile`.
- Serial debug logging via `LOG*` (dev only), prefixed by module (`wifi:`,
  `http:`, `places:`, `main:`).

## ⚠️ Before shipping
- Build production by leaving `DEV_MODE` undefined (delete/rename `lz_secrets.h`
  or comment its `#define DEV_MODE`). That gives real provisioning, HTTPS, sleep,
  and zero serial output. See "Local config & secrets".
- The known correctness bugs are fixed; the remaining TODO items are
  structural/optional. Skim them, but none block a release.

## TODO / open findings
From code review. Unchecked = open; ranked most impactful first within each group.

### Structure / duplication
- [ ] `st*Button` switches mostly just map an index → next state; a small
  data-driven dialog table could fold the static confirm screens together, but
  the ones with side effects (`Factory` reset, `Handed` flip) or dynamic content
  (OTP code, `stateSsidPrev` target) resist it — deferred, low value for a ~17
  state machine. (The repetitive `st*Enter` bodies are already handled by the
  `mainNavSet(count, selected)` helper.)
- [ ] (Option B, deferred) De-block the network flow: `mainNetwork` freezes the
  UI for the whole connect+fetch (worst case ~21 s across the three wifi TX-power
  retries). Could become polled states (`STATE_NET_CONNECT`/`FETCH`) driven from
  `loop()` so the dial stays live and progress renders through `onRender`;
  highest value is polling just the connect phase and leaving the short HTTP GET
  synchronous. The direct-draw itself is now a documented, intentional exception
  (Option A), not a bug — see the Architecture render-phase note.
