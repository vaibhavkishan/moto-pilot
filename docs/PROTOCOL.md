# MotoPilot — Protocol (JSON navigation over BLE)

The phone computes navigation and map geometry; the CYD only **draws** what it receives. All geographic work happens on Android.

---

## Architecture

```
Android                              ESP32 CYD
────────                             ─────────
GPS (LocationTracker)
  ↓
OSRM route (OsrmRouteProvider)
Overpass streets (OverpassStreetProvider)  [cached, refreshed on movement]
  ↓
MapProjector / StreetContextProjector
  lat/lon → screen pixels (240×232)
  ↓
NavMessage.toJson()
  ↓
BleNavClient.sendNavMessage()  ──BLE UART (NUS)──►  line buffer
                                                     parseNavJson()
                                                     MapRenderer.draw()
```

**Design principle:** `route`, `streets`, `user_x`, and `user_y` are **screen coordinates**, not WGS84. The ESP32 does not need a map projection library.

---

## Transport: BLE UART (Nordic NUS)

| Property         | Value                                                           |
| ---------------- | --------------------------------------------------------------- |
| Link             | Bluetooth Low Energy GATT                                       |
| Device name      | `MotoPilot-CYD`                                                 |
| Service UUID     | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`                          |
| RX (phone → CYD) | `6E400002-...` — Write / Write Without Response                 |
| TX (CYD → phone) | `6E400003-...` — Notify                                         |
| Framing          | Each JSON message ends with `\n`                                |
| Chunking         | Large payloads split to MTU−3 bytes; ESP reassembles until `\n` |
| Requested MTU    | 256 (firmware)                                                  |
| Heartbeat        | `{"type":"heartbeat"}\n` every 15 s                             |
| On connect       | CYD shows loading UI (from `loop()`) and notifies `OK\n`        |

The phone still uses **Wi‑Fi / mobile data** for OSRM, Nominatim, and Overpass. Only the **phone ↔ CYD** link is BLE; the CYD needs no Wi‑Fi.

### ESP32 receive path (stable)

NimBLE callbacks run on a small BLE task stack. Touching the TFT or allocating large JSON documents there can **reboot the CYD**.

Current firmware:

1. `onWrite` → push bytes into an RX ring buffer (no TFT / no JSON)
2. `onConnect` / `onDisconnect` → set flags only
3. `loop()` → drain RX queue, reassemble lines, `parseNavJson` / `MapRenderer`, update loading/waiting screens, restart advertising

Do not move drawing or ArduinoJson back into the BLE callbacks.

---

## Message types

### Heartbeat

```json
{ "type": "heartbeat" }
```

No drawing update. Keeps the connection alive. Sent as a line over BLE RX.

### Loading

```json
{ "type": "loading" }
```

Shows the loading screen on the CYD (logo, indeterminate progress bar, “Getting directions…”) until the next `nav` message. Sent when the app starts navigation while OSRM/GPS data is still being prepared. Also shown automatically when a BLE client connects. Colors follow the local light/dark theme (not sent in JSON).

### Motion

```json
{
  "type": "motion",
  "lat": -23.5505,
  "lon": -46.6333,
  "bearing": 127.5,
  "bearing_valid": true,
  "speed_mps": 6.2,
  "age_ms": 180,
  "gps_weak": false
}
```

A light update (about 150 bytes) the app sends every 100 ms between the full `nav` frames: the newest heading and the last GPS fix with its age, no map geometry. The CYD feeds the heading to its heading filter and the fix to its position predictor, so the map turns and moves between GPS frames without waiting for the next large frame. It is ignored until a `nav` frame has been shown, and treated like a `nav` frame's `bearing`, `bearing_valid`, `speed_mps`, `age_ms` and `gps_weak` otherwise. While riding, the app builds `bearing` from the gyroscope, corrected by each GPS course, so it follows a turn without the ~1 s lag of the GPS course; without gyroscope or at a standstill it repeats the GPS course.

On BLE the app sends full frames first and drops a motion update that had to wait more than 250 ms, so motion updates never delay a `nav` frame.

### Arrived

```json
{ "type": "arrived" }
```

Shows the arrival screen (red pin, “You have arrived”; in the Minimal style a black or white screen with a destination ring and a full progress bar) and stays on it until the next `loading` or `nav` message or a disconnect. The app sends it once the remaining route distance stays within 15 m (walking), 20 m (bike) or 30 m (car, motorcycle) for two consecutive updates, then stops navigation about 3 seconds later.

### Navigation (`nav`)

```json
{
  "type": "nav",
  "lat": -23.5505,
  "lon": -46.6333,
  "bearing": 127.5,
  "instruction": "Turn right",
  "distance_m": 200,
  "remaining_m": 5400,
  "remaining_s": 980,
  "off_route": false,
  "gps_weak": false,
  "speed_mps": 6.2,
  "bearing_valid": true,
  "street": "Av. Paulista",
  "route": [
    [204, 180],
    [209, 170],
    [214, 160]
  ],
  "streets": [
    [150, 120, 190, 140],
    [240, 130, 280, 160]
  ],
  "user_x": 204,
  "user_y": 204,
  "html": "<div>optional snippet</div>"
}
```

| Field              | Type   | Description                                                                                                                                                                                    |
| ------------------ | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`             | string | Must be `"nav"`                                                                                                                                                                                |
| `lat`, `lon`       | number | Current WGS84 position (for logging / future use)                                                                                                                                              |
| `bearing`          | number | Course in degrees (0 = north, clockwise). Falls back to the route direction when GPS has no course, so check `bearing_valid` before rotating the map                                           |
| `bearing_valid`    | bool   | `true` only for a real GPS course while moving (speed hysteresis, bearing accuracy ≤ 30°). When `false` the CYD holds its last heading. If the field is absent the CYD does not rotate the map |
| `speed_mps`        | number | Ground speed in m/s; with `px_per_m` and `age_ms` the CYD moves the map between frames                                                                                                         |
| `px_per_m`         | number | Optional. Canvas pixels per metre (232 / (2 × map radius)). Without it the CYD does not predict movement                                                                                       |
| `style`            | string | Optional. `classic` (default) or `minimal`, see Display styles                                                                                                                                 |
| `progress`         | number | Optional. Share of the route already travelled, 0..1; used by the Minimal style progress bar                                                                                                   |
| `zoom_max`         | number | Optional, classic style. Strongest zoom near turns, 1..6; `1` switches the zoom off. Default 3.5                                                                                               |
| `zoom_lead_s`      | int    | Optional, classic style. Seconds of riding before a turn at which the zoom starts. Default 8                                                                                                   |
| `age_ms`           | int    | Optional. Age of the GPS fix when the frame was sent                                                                                                                                           |
| `maneuver`         | string | Icon for the next maneuver: `depart`, `arrive`, `straight`, `left`, `right`, `slight_left`, `slight_right`, `sharp_left`, `sharp_right`, `uturn`, `roundabout`. Absent or unknown = no icon    |
| `eta`              | string | Arrival clock time `HH:MM` (24 h, phone time zone); absent when the remaining time is unknown                                                                                                  |
| `rerouting`        | bool   | The app is requesting a new route after you left the old one; the CYD shows "Rerouting..."                                                                                                     |
| `instruction`      | string | Maneuver text (e.g. "Turn right")                                                                                                                                                              |
| `distance_m`       | int    | Distance to next maneuver in meters                                                                                                                                                            |
| `street`           | string | Street name for current step (optional)                                                                                                                                                        |
| `route`            | array  | Polyline `[[x,y], ...]` in **canvas pixels** (max 64 points on ESP): the part of the route still ahead                                                                                         |
| `route_done`       | array  | Optional polyline of the part already travelled (max 32 points); drawn grey. Absent when off route                                                                                             |
| `dest_x`, `dest_y` | int    | Optional destination on the canvas; the CYD draws a red pin. Absent when it is outside the canvas                                                                                              |
| `streets`          | array  | Optional road segments `[[x0,y0,x1,y1], ...]` in canvas pixels (max **192** on CYD)                                                                                                            |
| `user_x`, `user_y` | int    | Current position on the canvas in pixels                                                                                                                                                       |
| `html`             | string | Optional HTML fragment (max ~480 chars); ESP strips tags and draws plain text                                                                                                                  |
| `remaining_m`      | int    | Remaining route distance in meters; `-1` if unknown                                                                                                                                            |
| `remaining_s`      | int    | Remaining duration in seconds; `-1` if unknown                                                                                                                                                 |
| `off_route`        | bool   | Position is off the route                                                                                                                                                                      |
| `gps_weak`         | bool   | GPS is stale/imprecise; firmware may keep the last good puck                                                                                                                                   |

`[-1,-1]` in `route` marks a gap: the renderer must **not** connect the previous point to the next (off-screen excursion).

### Map pixel space

Geometry is sent in a **408 × 408 canvas** centred on the user at `(204, 204)`, clipped to a circle of radius 203 px. The CYD rotates the geometry itself, so the window must cover the visible area for any heading: the user sits at screen `(120, 160)` and the farthest corner of the 240 × 232 map area is 200 px away.

- Origin: top-left `(0, 0)`; canvas centre = current GPS position
- Scale: `mapScaleMeters` is the half-range of the **visible map height** (232 px); both axes use the same meters/pixel (roads are not stretched)
- Visible map area on the CYD: **240 × 232** pixels (top of the 240×320 display)
- Bottom **88 px** overlay: maneuver, remaining time/distance, GPS-weak hint + on-device **light/dark theme button**

`MapProjector.ANCHOR_X/ANCHOR_Y`, `CANVAS_SIZE` and `CANVAS_RADIUS` on Android must match `MAP_ANCHOR_X/MAP_ANCHOR_Y` in `nav_types.h` and the street budget `NAV_MAX_STREET_SEGMENTS`.

### Display styles (CYD)

`style` selects how the CYD draws the frame: absent or `"classic"` is the Google Maps style described below; `"minimal"` is a stripped-down turn-by-turn style. It can change from one frame to the next. In Minimal style the CYD draws:

- a black screen with white drawing in the dark theme, and the reverse in the light theme
- the route ahead as one thick line with a clear halo, other roads only as short, fading stubs (about 30 px) where a side road meets the route, never as a full map, a ring at the destination, and a large arrow for you
- in the bottom panel: the next-turn icon, the distance to the turn in large digits (`300 m`, `1.8 km`), a small theme button, and a trip progress bar

The progress bar uses `progress` (0..1, the share of the route already travelled) and is hidden when the field is absent. Street name, remaining time and ETA are not shown in this style.

### Smooth motion (CYD)

GPS frames arrive at 1–5 Hz, but the CYD redraws as fast as the panel allows while moving. Between frames it advances the map along the heading by `speed_mps × time × px_per_m` (starting from the fix `age_ms`) and keeps turning at the estimated turn rate. When a frame arrives, the canvas is re-centred on the real fix and the small difference from the predicted position fades out over about 0.35 s, so the map never jumps. Prediction stops when `bearing_valid` is `false`, the speed is below 0.5 m/s, GPS is weak, or no frame has arrived for 2 s. A gap larger than 60 px (a reroute, a new trip) snaps instead of gliding.

**Demo ride (no phone needed):** hold the logo on the CYD home screen for one second, or send `b` over its serial port (115200 baud). The CYD then rides a built-in route on a street grid with a right, a left and a right turn, slowing for each one, and ends on the arrival screen before starting over. Tap the map or send `b` again to stop; `m` switches the map style. The ride sends a full frame once a second (like a phone with 1 Hz GPS) plus a motion update every 100 ms. A connecting phone also stops it. Frames per second and the ride position are printed to the serial port.

### Zoom near turns (CYD, classic style only)

As a turn gets close the CYD zooms the map in around you, and eases back out after the turn, like Google Maps. It uses `distance_m` (reduced by the distance predicted since the frame), `maneuver`, `speed_mps`, `px_per_m`, and the optional `zoom_max` and `zoom_lead_s` (Settings > Zoom near turns):

- Only real turns, roundabouts and the arrival zoom; `straight` and `depart` do not.
- The zoom starts `zoom_lead_s` seconds before the turn (speed × lead, at least 80 m, at most 450 m) and reaches its maximum 40 m before it. The maximum is `zoom_max`, limited so the map still shows about 93 m across its height. It never zooms below the phone's scale. `zoom_max` of 1 switches it off.
- The zoom follows its target with easing and a rate limit, so it never jumps. After the turn the next maneuver is far away, so it eases back to the normal scale in about two seconds.
- No zoom while off route, rerouting or with weak GPS, and none in the Minimal style.

### Map rotation (CYD)

When `bearing_valid` is present, the CYD draws heading-up: it translates `(user_x, user_y)` to the screen anchor `(120, 160)` and rotates all geometry by the smoothed heading so the direction of travel points up. The heading is filtered on the CYD (angle wrap, 2° dead band, an alpha-beta tracker that follows the heading and its turn rate, eased between samples) and is held while `bearing_valid` is `false`. Until the first valid heading the map stays north-up and the position is a plain dot.

Frames without `bearing_valid` are drawn as received, north-up, with the user at `(user_x, user_y)`.

The app and CYD firmware must be flashed together: mismatched firmware would draw the larger canvas unrotated and off-centre.

Theme preference is stored on the CYD in NVS and does not affect the JSON schema.

---

## Android pipeline (per update)

1. `LocationTracker` emits GPS fixes; `GpsFilter` rejects stale, imprecise, and implausible jumps.
2. `NavigationEngine` loads OSRM route (FOSSGIS car/bike/foot) and remaining distance/time.
3. `MapProjector` + `ScreenGeometry` project with equal axes and clip segments (gaps as `[-1,-1]`).
4. `OfflineMapRepository` supplies Overpass streets along the whole corridor (persisted).
5. `StreetContextProjector` distributes up to **192** visible segments across the canvas.
6. `NavMessage` is built and serialized with `org.json`.
7. `BleNavClient` keeps only the latest frame, then writes with GATT ACKs (chunked to MTU).

Typical payload size: **under 5 KB** (the CYD line buffer is 8 KB).

---

## ESP32 render order

BLE parsing only stores the latest frame. `renderTick()` in `loop()` draws whenever a frame arrived, the heading is still moving, or the map is gliding, with no fixed frame interval. `MapRenderer::draw()`:

1. Compose the map in a 4-bit palette sprite (240×232), then push it once — nothing is cleared on the panel
   - Land colour; soft park blobs in light mode (decorative, fixed on screen)
   - `streets` — road casing then fill
   - `route_done` polyline — grey with a darker edge (travelled part)
   - `route` polyline — blue with light edge (two passes)
   - destination pin — red, tip on `dest_x`, `dest_y`
   - User arrow (dark-blue fill, white ring, plus a dark outer edge in the light theme) or a position dot before the first valid heading
2. Repaint only the bottom-sheet parts that changed:
   - maneuver icon (48 px, drawn from `maneuver`)
   - distance to the next maneuver (large font), instruction and street
   - summary line `remaining km | minutes | ETA hh:mm`, or `Rerouting...` / `Off route`
   - theme button

If the sprite cannot be allocated, the renderer falls back to drawing directly on the panel.

---

## Route profiles (Android → OSRM)

| UI label   | Graph                                                           | Notes                         |
| ---------- | --------------------------------------------------------------- | ----------------------------- |
| Car        | `https://routing.openstreetmap.de/routed-car/route/v1/driving/` | Default                       |
| Motorcycle | same car graph                                                  | No dedicated motorcycle graph |
| Bike       | `.../routed-bike/route/v1/cycling/`                             | Separate FOSSGIS bike graph   |
| Walking    | `.../routed-foot/route/v1/walking/`                             | Separate FOSSGIS foot graph   |

Changing only `/v1/{profile}` on `router.project-osrm.org` does **not** switch the routing graph. MotoPilot uses the FOSSGIS hosts above.

---

## External APIs (Android only)

| API                                       | Purpose                       | API key                   |
| ----------------------------------------- | ----------------------------- | ------------------------- |
| OSRM FOSSGIS (`routing.openstreetmap.de`) | Turn-by-turn route + geometry | No                        |
| Nominatim (`nominatim.openstreetmap.org`) | Address / place search        | No (respect usage policy) |
| Photon (`photon.komoot.io`)               | Complementary search          | No                        |
| Overpass (`overpass-api.de`)              | Road geometry along the route | No (rate limit)           |

The ESP32 does not call these APIs and does not need Wi‑Fi for navigation display.

---

## Maps browser mode

Optional flow:

1. App opens Google Maps in a **WebView** (`MapsBrowserActivity`).
2. JavaScript extracts visible navigation text every ~2 s.
3. Snippet stored in `MapsHtmlHolder` and attached to each `nav` message as `html`.
4. Map polyline still comes from GPS + OSRM on the phone.

The ESP32 does **not** render HTML/CSS — `html_renderer` strips simple tags and draws text lines.

---

## Libraries

### ESP32

- **TFT_eSPI** — display driver and drawing primitives
- **NimBLE-Arduino** v2.x — BLE GATT UART server (callbacks with `NimBLEConnInfo`)
- **ArduinoJson** v7+ — JSON parse (only from `loop()`)
- **XPT2046_Touchscreen** — resistive touch for the on-device theme button
- **Preferences** (ESP32 Arduino core) — persist light/dark theme in NVS

Firmware UI modules: `map_renderer`, `scene`, `logo`, `maps_theme`, `touch_cyd`, `loading_screen`, `html_renderer`, `utf8_text`.

### Android

- **Android BLE APIs** (`BluetoothLeScanner`, `BluetoothGatt`) — NUS client
- **Play Services Location** — GPS
- **org.json** — `NavMessage` serialization
- **OkHttp** — HTTP for OSRM (FOSSGIS), Nominatim, Photon, Overpass (internet, not CYD link)

---

## Credits

The protocol extensions for the CYD 240×232 map (remaining time, `gps_weak`, `maneuver`, `eta`, `rerouting`, 192 street segments) build on ideas from [PLZ-1/DisplayConnectV2.1](https://github.com/PLZ-1/DisplayConnectV2.1), adapted from its 480×232 ST7796S layout. MotoPilot itself is built on [DisplayConnect](https://github.com/malaq88/DisplayConnect) by Antonio Malaquias.
