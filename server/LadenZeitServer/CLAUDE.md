# CLAUDE.md

Guidance for working on the **LadenZeit** server in this directory.

> **Keep this file in sync.** Whenever you change something this document
> describes — architecture, the data model, the endpoint tables, the pairing/
> session flow, the device "places" wire format, config keys, or conventions —
> update the relevant section in the same change. Before finishing a task, verify
> the affected sections still match the code (class names, routes, field names,
> constants). Treat a drift between code and CLAUDE.md as a bug to fix, not to
> leave.

## What this is

The cloud backend for **LadenZeit** ("opening times"), an ESP32-C6 desk gadget
that shows the open/closed status of a configurable list of shops ("places") on
an OLED. See the firmware's `CLAUDE.md` under `REV_B/firmware/LadenZeit/` for the
device side.

A **Spring Boot 4.1 / Java 21** app (Maven) exposing two REST surfaces under
`/api/v1`:

- **Device API** (`/api/v1/devices`, `DeviceV1Controller`) — called by the
  firmware. Serves the binary "places" blob and issues pairing OTPs.
- **Web API** (`/api/v1/web`, `WebV1Controller`) — called by a companion web
  frontend. Session-cookie authenticated; lets a user pick which places a device
  shows and search Google for places.

Opening hours come from **Google Maps Places API (v1)** and are cached in MySQL.

## Build / run

Maven wrapper is checked in — use `./mvnw` (POSIX) or `mvnw.cmd` (Windows).

- Run: `./mvnw spring-boot:run`
- Test: `./mvnw test`
- Package: `./mvnw package` → runnable jar in `target/`

Requires at runtime:
- A **MySQL** instance at `127.0.0.1:3306`, database `ladenzeit`, user/pass
  `ladenzeit`/`ladenzeit` (see `application.properties`). Schema is auto-managed
  (`spring.jpa.hibernate.ddl-auto=update`).
- **`MAPS_API_KEY`** environment variable — the Google Maps API key. Without it,
  place lookups/searches log an error and return null/empty (the app still
  starts).

Listens on `0.0.0.0:8080`.

## Layout & conventions

Base package `com.nakamura_labs.ladenzeit`, standard layered structure:

| Package | Contents |
|---------|----------|
| `controller` | `DeviceV1Controller`, `WebV1Controller` (`@RestController`) |
| `service` | `LZDeviceService`, `LZPlaceService` (business logic) |
| `repository` | Spring Data JPA repos (`LZDeviceRepository`, `LZPlaceRepository`) |
| `entity` | JPA `@Entity` classes, all prefixed `LZ`; `entity.common` has mapped superclasses |
| `dto` | Records for API payloads (e.g. `PlaceSearchEntry`) |
| `misc` | Cross-cutting: `GlobalExceptionHandler`, `GoogleMapsConfiguration`, `OtpCodeGenerator` |

- Entities are prefixed `LZ`; routes are versioned under `/api/v1`.
- Entities extend `RowCreatedAt` (or `RowCreatedAndModifiedAt`) for automatic
  `createdAt` / `modifiedAt` (`entity.common`).
- **Weekday convention is Monday=0 … Sunday=6.** Google returns Sunday=0-based
  days; `LZPlaceService.toMondayStartsWeek()` converts on ingest, and the device
  protocol emits Monday=0.
- Lombok is on the classpath but the entities use hand-written getters/setters —
  match the surrounding style (don't sprinkle `@Data`) unless refactoring
  deliberately.
- Logging via SLF4J `LOG.debug/warn/error`; `logging.level.com.nakamura_labs`
  is `DEBUG`, root is `WARN`.

## Data model

- **`LZDevice`** — a physical device. `@Id` is the firmware-supplied device id
  string. Holds session state (`sessionCookie` + `sessionCookieCreatedAt`,
  `sessionOtp` + `sessionOtpCreatedAt`; all `@JsonIgnore`) and an **ordered**
  `List<LZDevicePlace>` (`@OneToMany`, cascade ALL, orphan removal,
  `@OrderColumn sort_order`). Setting the cookie/OTP also stamps its created-at.
- **`LZDevicePlace`** — one place shown by a device: `placeId` (Google place id)
  + `placeName`. `placeName` is bean-validated: **1–8 chars** against a
  restricted character pattern — the 8-char limit mirrors the OLED display width.
- **`LZPlace`** — a **cache entry** for a Google place, keyed by the Google place
  id. Has `name` and an `@OneToMany` list of `LZPlaceHour`. Evicted by age (see
  cache below).
- **`LZPlaceHour`** — one opening period. Stores both a display form
  (`fromString`/`toString`, `"HH:MM"`) and a machine form (`fromWeekday`/
  `toWeekday` Monday=0, `fromInteger`/`toInteger` = minutes-of-day).

## Pairing & session flow

1. **Firmware requests an OTP:** `PATCH /api/v1/devices/{deviceId}/auth/otp`.
   `LZDeviceService.activateSessionOtp` generates a 9-char OTP **and** a fresh
   session-cookie UUID, saves both, and returns the OTP formatted `XXX-XXX-XXX`
   for the device to display. ⚠️ The cookie's created-at is stamped here (at OTP
   issue), so the cookie-expiry clock starts at pairing request time, not at
   redemption.
2. **Web redeems the OTP:** `POST /api/v1/web/auth/otp` with header
   `X-Device-Token: <otp>`. `sessionCreateByOtp` looks up the device by OTP,
   rejects if older than `otp_expire_minutes`, clears the OTP, and returns the
   stored session cookie. The controller sets it as the `SESSION_ID` cookie:
   `httpOnly`, `secure`, `SameSite=Strict`, `path=/api/v1/web`,
   `maxAge = cookie_expire_hours`.
3. **Authenticated web calls** carry `SESSION_ID`. `WebV1Controller.validateSession`
   (a `@ModelAttribute` running before every handler **except** `/auth/otp`)
   verifies the cookie via `sessionVerify` (must exist and be younger than
   `cookie_expire_hours`), loads the `LZDevice`, and stores it as the `device`
   request attribute — handlers receive it via `@RequestAttribute LZDevice device`.

`OtpCodeGenerator` uses a `SecureRandom` over an **unambiguous** alphabet
(`ABCDEFGHKMNPQRSTWXYZ234568` — no `0/O`, `1/I/L`, etc.).

## Web API (`/api/v1/web`, `WebV1Controller`)

| Method & path | Auth | Purpose |
|---------------|------|---------|
| `POST /auth/otp` | none (OTP header) | Exchange OTP for `SESSION_ID` cookie |
| `GET /auth/status` | session | 200 if the session is valid |
| `POST /auth/invalidate` | session | Log out (clears the stored cookie) |
| `GET /places` | session | List this device's `LZDevicePlace`s |
| `PUT /places` | session | Replace the device's places (validated), returns the new list |
| `GET /search-google?query=` | session | Google Places text search (min 3 chars) → `List<PlaceSearchEntry>` |

## Device API (`/api/v1/devices`, `DeviceV1Controller`)

| Method & path | Purpose |
|---------------|---------|
| `GET /{deviceId}/places` | The binary "places" blob (`application/octet-stream`) |
| `PATCH /{deviceId}/auth/otp` | Issue + return a formatted OTP for pairing (device displays it) |

### The "places" wire format (`DeviceV1Controller.places`)

The server **writes** the exact byte layout the firmware parses (see the firmware
`CLAUDE.md`, `lz_places.cpp`). Built with a `ByteArrayOutputStream`; each
`baos.write(int)` emits **one low byte**, so all counts/lengths must stay ≤ 255.

Header:
- `[0]` day-of-week, `[1]` hour, `[2]` minute, `[3]` second — current
  **Europe/Berlin** time, used by the device to seed its clock.
- `[4]` place count.

Then per place:
- `len` = name length **+ 1**, then the name bytes, then a `0` (NUL) terminator.
- `slotCount`, then `slotCount × 4` bytes: each slot is a packed big-endian
  `uint32`:
  - bits 31..29 — from-weekday (Monday=0)
  - bits 28..18 — from-minutes-of-day (0–1439)
  - bits 17..15 — to-weekday
  - bits 14..4  — to-minutes-of-day
  - (bits 3..0 unused)

If a device id is unknown, the place count is `0` and no place records follow
(the header is still emitted).

## Google Places integration & caching (`LZPlaceService`)

- `getPlace(placeId)` is **cache-through**: returns the DB row if present,
  otherwise `updatePlaceFromGoogle` fetches it and saves it.
- `updatePlaceFromGoogle` calls Google `getPlace` with a field mask for
  `currentOpeningHours,regularOpeningHours,displayName`. **Current** opening
  hours override **regular** when both are present. Periods missing open/close
  are skipped.
- `searchPlacesFromGoogle(query)` runs a text search (max 10 results) →
  `PlaceSearchEntry(placeId, name, address)`.
- `GoogleMapsConfiguration` builds a `PlacesClient` per call, injecting the API
  key and an `X-Goog-FieldMask` header. Returns `null` (logged) if the key is
  missing or client setup fails — callers must null-check.
- **Cache eviction:** `cleanCache()` runs on `ApplicationReadyEvent` and then
  hourly (`@Scheduled fixedDelay = 1h`), deleting `LZPlace` rows older than
  `place_cache_hours` via `LZPlaceRepository.deleteByCreatedAtBefore`.

## Error handling (`misc.GlobalExceptionHandler`, `@RestControllerAdvice`)

- `ResponseStatusException` with `UNAUTHORIZED` → `401` (empty body); other
  statuses fall through to the generic 500 handler.
- `NoResourceFoundException` → `404`.
- `TransactionSystemException` wrapping a Bean-Validation
  `ConstraintViolationException` → `400` with a `{ field: message }` map (this is
  how `PUT /places` validation failures surface).
- Any other `Exception` → `500` (logged).

## Config keys (`src/main/resources/application.properties`)

| Key | Default | Meaning |
|-----|---------|---------|
| `server.port` / `server.address` | `8080` / `0.0.0.0` | Listen socket |
| `spring.datasource.*` | local MySQL `ladenzeit` | DB connection |
| `spring.jpa.hibernate.ddl-auto` | `update` | Auto schema management |
| `com.google.maps.api.key` | `${MAPS_API_KEY:}` | Google Maps key (from env) |
| `com.nakamura_labs.ladenzeit.place_cache_hours` | `6` | Place-cache TTL |
| `com.nakamura_labs.ladenzeit.cookie_expire_hours` | `6` | Session cookie lifetime |
| `com.nakamura_labs.ladenzeit.otp_expire_minutes` | `15` | OTP validity window |

> Note: the DB credentials and other values in `application.properties` are
> committed as local-dev defaults. Don't add real secrets here — inject them via
> environment/externalized config, as `MAPS_API_KEY` already is.
