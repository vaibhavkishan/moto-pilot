# MotoPilot

**MotoPilot** sends **bike navigation data** from your Android phone to a cheap **ESP32 CYD** (Cheap Yellow Display) over **Bluetooth Low Energy**. The phone computes GPS, routing, and map geometry; the CYD **draws a vector map** (route, nearby streets, your position, and turn instructions) on a dedicated 2.4″ TFT.

Flash the **app and the CYD firmware together**.

> MotoPilot is built on top of [DisplayConnect](https://github.com/malaq88/DisplayConnect) by Antonio Malaquias. See [Author and credits](#author-and-credits).

---

## Why use an ESP32 CYD for bike navigation?

| Problem on the phone                     | How MotoPilot helps                                               |
| ---------------------------------------- | ----------------------------------------------------------------- |
| Screen hard to read in sunlight or rain  | Mount the CYD on the handlebar; keep the phone protected          |
| Battery drain from bright GPS + LTE      | Phone can stay dim or locked; CYD only draws lightweight graphics |
| Touch interaction while riding is unsafe | Glance at the CYD; set destination before you start               |
| Dedicated bike GPS units are expensive   | ESP32-2432S028 boards cost a few dollars                          |

The **ESP32-2432S028** (CYD) combines ESP32, 240×320 ILI9341 TFT, and resistive touch (XPT2046). The map area is **240×232 pixels**; the bottom strip shows the maneuver, distance, remaining time, and a round **light/dark theme button**.

---

## How it works

The app does **not** capture or stream the phone screen. It sends a **navigation JSON packet** on each GPS update (1–5 Hz). The phone is the **brain**; the CYD is the **display**. No Google Maps SDK or API key is required for core navigation (OSRM + OpenStreetMap).

```
┌─────────────────────────────────────────────────────────────────┐
│                        Android phone                            │
│  GPS (Fused Location)                                           │
│  OSRM route (car / bike / walk)  — needs internet               │
│  Nominatim search + Overpass street geometry (OSM)              │
│  MapProjector: lat/lon → canvas pixels (408×408 window)         │
│  NavMessage → JSON                                              │
│  BleNavClient (BLE UART / Nordic NUS)                           │
└────────────────────────────┬────────────────────────────────────┘
                             │  BLE  (newline-delimited JSON)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                        ESP32 CYD                                │
│  NimBLE NUS server                                              │
│  ArduinoJson parse → NavState                                   │
│  MapRenderer (TFT_eSPI): sprite: streets → route → arrow; sheet │
│  Touch (XPT2046): light / dark theme toggle (saved in NVS)      │
└─────────────────────────────────────────────────────────────────┘
```

### Display UI (CYD)

The firmware draws a **Google Maps–inspired** vector map locally (theme is **not** part of the BLE JSON):

| Element      | Light theme (default)                                                                                | Dark theme             |
| ------------ | ---------------------------------------------------------------------------------------------------- | ---------------------- |
| Land / parks | Near-white land, soft green parks                                                                    | Elevated charcoal land |
| Roads        | Light gray casing + white fill                                                                       | Gray casing / fill     |
| Route        | Light-blue line with a dark-blue casing; the travelled part turns grey, the destination is a red pin | Same, lighter blues    |
| User         | Dark-blue arrow with a white ring (and a dark outer edge in light theme); map rotates heading-up     | White ring             |
| Overlay      | White bottom sheet + blue distance                                                                   | Dark card + light text |

The home and loading screens show a landscape scene with the MotoPilot logo. The loading screen has an indeterminate progress bar.

**Display style:** in the app, **Settings > Display style** switches between the classic map above and a **Minimal style**: a black (or white) screen with your route as one bold line, short fading stubs where side roads meet it (no full street map), the next-turn icon, a large distance and a trip progress bar. The icon is a straight arrow while the next turn is more than 400 m away and becomes the turn arrow (left, right, slight, sharp, U-turn, roundabout) from 400 m on. The style changes immediately, also during navigation. The theme button still switches between dark and light. Example screens are in `docs/screens`.

**Arrival:** when you arrive, the CYD shows a "You have arrived" screen that matches the style: a red pin on the landscape scene in the classic style, and in the Minimal style a black or white screen with a large destination ring and a full progress bar.

**Smooth motion:** the CYD redraws at about 25 frames per second (about 30 in the Minimal style) between GPS frames. It predicts the position from speed and heading, and turns the map with an alpha-beta filtered heading, so the map neither jumps nor lags noticeably.

**Zoom near turns (classic style):** the map zooms in as a turn gets close and eases back out afterwards. In the app, **Settings > Zoom near turns** has a switch, a strength slider (1.5–5×) and a slider for how many seconds before the turn it starts. A roundabout stays zoomed in from shortly before the entry until its exit.

**Demo ride (no phone needed):** hold the logo on the CYD home screen for one second, or send `b` over its serial port (115200 baud). The CYD rides a built-in route of about 1.9 km with three turns and ends on the arrival screen. `m` switches between the classic and Minimal style while it runs; tap the map or send `b` again to stop. Send `r` instead of `b` for a 500 m roundabout ride (five exits, the 4th exit is taken). A connecting phone also stops it. Frame statistics are printed on the serial port.

Tap the round **sun / moon button** (bottom-right of the home screen, or right side of the navigation bottom sheet) to toggle the theme. The preference is stored in NVS (`Preferences` namespace `ui` / key `theme`) and survives reboot.

### Update loop (1–5 Hz, configurable)

1. Open the **Connect** tab, scan, and connect to the CYD (`MotoPilot-CYD`).
2. In the **Navigate** tab, **search** a destination by address/place name (Nominatim) or enter coordinates. A coordinate pair typed into the search box (for example `-23.5505, -46.6333` or `23.55 S, 46.63 W`) is recognised and used directly instead of being searched as a place.
3. Choose a **route profile**: car, motorcycle, bike, or walking.
4. Tap **Start navigation** — a foreground `location` service starts.
5. On each GPS update, the app:
   - Fetches or reuses the OSRM route (FOSSGIS car / bike / foot graphs)
   - Filters GPS (age, accuracy, jumps) and may snap small deviations to the route
   - Loads street geometry (Overpass, persisted along the whole corridor)
   - Projects route, streets, and position **without stretching axes**
   - Sends a JSON `nav` message over **BLE** (only the latest frame is kept)
   - Between frames, sends a light `motion` message every 100 ms (heading and last fix)
6. The CYD parses JSON and redraws the map (street casing, route, position arrow, maneuver icon, remaining time/distance, ETA). Between GPS frames it keeps moving and turning the map at about 25 frames per second, so motion looks smooth; the phone also sends a small motion update every 100 ms with a gyroscope-fused heading, so turns are followed without the usual GPS lag. In the classic style the map also zooms in as a turn gets close and eases back out after it (**Settings > Zoom near turns**: on/off, strength and how early it starts).

Optional: **Open Maps in browser** mode keeps a WebView open, extracts navigation text as a small HTML snippet, and includes it in each JSON message. The map polyline still comes from GPS + OSRM on the phone.

Full protocol details: [docs/PROTOCOL.md](docs/PROTOCOL.md)

---

## Technologies

### Android app (Kotlin)

- **Jetpack Compose + Material 3** -> UI (Connect, Navigate, and Settings tabs)
- **MVVM + StateFlow** -> `MainViewModel`, reactive UI state
- **DataStore** -> BLE device address/name, route profile, saved destination
- **Play Services Location** -> Real-time GPS
- **Foreground Service** (`location`) -> Navigation while the screen is locked
- **SensorManager** (gyroscope + gravity) -> Turn rate for the gyro-fused heading (no permission needed)
- **BleNavClient** (Android BLE / NUS) -> Sends JSON to the CYD
- **Coroutines** -> Async GPS, routing, streets, and transmission
- **OSRM** (FOSSGIS `routed-car` / `routed-bike` / `routed-foot`) -> Car, bike, and walking graphs
- **Nominatim + Photon** -> Address and place search
- **Overpass API** (OpenStreetMap) -> Street geometry (also stored for offline use)
- **org.json** (`JSONObject`) -> Builds the `NavMessage` protocol
- **OkHttp** -> HTTP for OSRM / OSM APIs (internet only)
- **WebView** (Maps browser mode) -> Extracts Google Maps navigation text (optional)

**Key modules**

- `navigation/` -> `LocationTracker`, `GpsFilter`, `NavigationEngine`, `NavigationForegroundService`, `HeadingFusion` (gyro + GPS course), `YawRateSensor`
- `routing/` -> OSRM, Nominatim/Photon, Overpass, `RouteProfile`, trip remaining time
- `offline/` -> Persistent street tiles along the route
- `map/` -> `MapProjector`, `ScreenGeometry`, `StreetContextProjector`
- `protocol/` -> `NavMessage` JSON builder
- `network/` -> `BleNavClient`
- `maps/` -> Maps browser WebView + HTML extractor (optional)

**Requirements:** Android 7.0+ (API 24), target API 36. BLE required.

### ESP32 CYD firmware (Arduino)

- **ESP32-2432S028** (CYD) -> BLE + 240×320 TFT + XPT2046 touch
- **TFT_eSPI** -> ILI9341 driver; draws lines, circles, and text
- **NimBLE-Arduino** (h2zero) **v2.x** -> BLE GATT UART (NUS) server
- **ArduinoJson** v7+ -> JSON parsing (on the main `loop()` task only)
- **XPT2046_Touchscreen** -> resistive touch for the theme button
- **Preferences** (ESP32 core) -> persist light/dark theme in NVS

**Firmware modules:** `nav_protocol.cpp`, `map_renderer.cpp`, `scene.cpp`, `logo.cpp`, `html_renderer.cpp`, `loading_screen.cpp`, `maps_theme.cpp`, `touch_cyd.cpp`, `utf8_text.cpp`, `latin_font.h`, plus `nav_heading.cpp` (heading filter), `motion_predictor.cpp` (position between frames) and `zoom_controller.cpp` (zoom near turns).

The CYD does **not** need Wi‑Fi for navigation display. It only draws vectors and text from BLE JSON.

**Important (CYD stability):** NimBLE callbacks must **not** call TFT or ArduinoJson. They only queue RX bytes / UI flags; `loop()` draws and parses. Drawing from a BLE callback can reboot the board (stack overflow).

---

## Wire protocol (summary)

- **Transport:** BLE UART (Nordic NUS), newline-delimited JSON
- **Device name:** `MotoPilot-CYD`
- **Service / RX / TX UUIDs:** Nordic NUS (`6E400001` / `6E400002` / `6E400003` …)
- **MTU:** negotiated (firmware requests 256)
- **Heartbeat:** `{"type":"heartbeat"}\n` every 15 s
- **Navigation:** `{"type":"nav", ...}\n` — see [docs/PROTOCOL.md](docs/PROTOCOL.md)
- **Motion:** `{"type":"motion", ...}\n` every 100 ms between `nav` frames (about 150 bytes: heading, fix, speed)
- **Status screens:** `{"type":"loading"}\n` and `{"type":"arrived"}\n`

Important: `route`, `streets`, `user_x`, and `user_y` are already in **canvas pixels** (a 408×408 window centred on the user). The CYD rotates them heading-up and does no geographic projection. `bearing_valid` tells the CYD when the GPS course can be trusted for rotation.

---

## Project structure

```
MotoPilot/
├── app/                              # Android application
│   └── src/main/java/com/vaibhavkishan/motopilot/
│       ├── navigation/                 # GPS, GpsFilter, NavigationEngine, HeadingFusion, foreground service
│       ├── routing/                    # OSRM, Nominatim/Photon, Overpass, RouteProfile
│       ├── offline/                    # Persistent street cache along the route
│       ├── map/                        # MapProjector, ScreenGeometry, StreetContextProjector
│       ├── protocol/                   # NavMessage JSON
│       ├── network/                    # BleNavClient (BLE UART)
│       ├── maps/                       # Optional Maps browser WebView
│       ├── storage/                    # SettingsRepository (DataStore)
│       ├── ui/                         # Compose app, screens, components, theme
│       ├── viewmodel/                  # MainViewModel, SettingsViewModel
│       ├── models/                     # AppSettings, ConnectionState, stats
│       └── utils/                      # TransmissionHub, StatsTracker
├── DisplaySender/                    # ESP32 CYD firmware (Arduino)
│   ├── DisplaySender.ino
│   ├── map_renderer.cpp / .h           # Vector map + bottom sheet
│   ├── scene.cpp / .h                  # Home, loading and arrival screens, theme button
│   ├── logo.cpp / .h                   # MotoPilot logo
│   ├── maps_theme.cpp / .h             # Light/dark palette + NVS
│   ├── touch_cyd.cpp / .h              # XPT2046 touch (theme toggle)
│   ├── nav_protocol.cpp / .h
│   ├── nav_heading.cpp / .h            # Heading filter (alpha-beta)
│   ├── motion_predictor.cpp / .h       # Position between frames
│   ├── zoom_controller.cpp / .h        # Zoom near turns
│   ├── map_transform.h / nav_types.h
│   ├── utf8_text.cpp / .h              # Latin-1 glyphs (accents)
│   ├── latin_font.h
│   ├── html_renderer.cpp / .h
│   ├── loading_screen.cpp / .h
│   ├── test/                           # Host tests, smoothness report, screen previews
│   └── config.h.example
├── docs/
│   ├── PROTOCOL.md                     # JSON protocol reference
│   └── screens/                        # Example screens (classic and Minimal, dark and light)
└── README.md
```

---

## Hardware

- **Board:** ESP32-2432S028 (Cheap Yellow Display), 240×320 TFT
- **Phone:** Android 7+ with BLE, GPS, and internet (for OSRM / OSM APIs)
- **Link:** Bluetooth Low Energy between phone and CYD (no shared Wi‑Fi required for the display)

**TFT_eSPI setup:** Configure `Arduino/libraries/TFT_eSPI/Setup_User.h` for your CYD (ILI9341_2, `TFT_RGB`, `USE_HSPI_PORT`). Some CYD panels show inverted colours and swapped red/blue (a pale screen in dark theme, orange and blue exchanged): add `#define TFT_INVERSION_ON` and use `TFT_RGB_ORDER TFT_BGR`. Keep the default `LOAD_FONT2`, `LOAD_FONT4` and `LOAD_FONT6` enabled; the large distance in the Minimal style uses font 6.

---

## Getting started

### 1. Flash the ESP32

1. Install Arduino libraries: **TFT_eSPI**, **NimBLE-Arduino** (v2.x), **ArduinoJson** v7+, **XPT2046_Touchscreen**.
2. Copy `DisplaySender/config.h.example` to `DisplaySender/config.h`.
3. Upload `DisplaySender.ino`.
4. On boot, the CYD shows the MotoPilot home screen with the Bluetooth card (`MotoPilot-CYD`) and the theme button at the bottom-right.

### 2. Build and install the Android app

1. Open the project in **Android Studio**.
2. Build and install (`minSdk 24`).
3. Grant **Bluetooth**, **location**, and **notifications** when prompted.

### 3. Ride workflow

1. Power the CYD (it advertises `MotoPilot-CYD`).
2. In MotoPilot, open **Connect**: **Scan for devices** → select the CYD → connect (loading screen on the CYD).
3. Open **Navigate** and select the **route type** (car / motorcycle / bike / walking).
4. **Search** a destination or enter coordinates under Advanced options.
5. Tap **Start navigation** and mount the CYD on your handlebar.
6. Wait until the app shows **Route ready for offline use** if you want streets without internet.
7. Adjust **Settings** if needed (update rate, map radius, display style, zoom near turns).

If the CYD reboots when connecting, make sure you have the current firmware: TFT updates must run in `loop()`, not in BLE callbacks.

---

## Settings reference

| Setting         | Default       | Description                                                                                                |
| --------------- | ------------- | ---------------------------------------------------------------------------------------------------------- |
| BLE device      | —             | Saved CYD address / name after scan                                                                        |
| Route type      | Car           | FOSSGIS OSRM: car / bike / foot graphs (motorcycle still uses car)                                         |
| Display style   | Classic       | Classic (Google Maps style) or Minimal (route line, side-road stubs, turn icon, distance); applies live    |
| Update rate     | 2 Hz          | Full navigation frames per second (a light motion update is sent 10 times per second in addition)          |
| Zoom near turns | On, 3.5×, 8 s | Classic style: zoom in before turns, zoom strength (1.5–5×) and how many seconds before the turn it starts |
| Map radius      | 400 m         | Vertical range from the center (equal m/pixel on both axes)                                                |
| Destination     | —             | Saved query, label, and coordinates                                                                        |

---

## Known limitations

- **No native second screen:** The CYD is not an Android external display; it only renders what the app sends in JSON.
- **BLE range:** Keep phone and CYD within typical BLE distance (~10 m open air; less with pockets/bags).
- **OSRM public graphs:** Bike and walking use FOSSGIS `routed-bike` / `routed-foot`. Motorcycle still uses the car graph.
- **OSM rate limits:** Nominatim, Photon, and Overpass should be used sparingly (search on user action; street tiles persist on the phone).
- **Offline streets:** GPS, BLE, and already-downloaded streets can continue without internet. Recalculating a new route still needs connectivity.
- **Maps browser mode:** Requires the WebView activity to stay open; ESP renders stripped text, not full HTML/CSS.
- **Single BLE client:** One GATT connection at a time.
- **CYD RAM:** Street budget is **192 segments**. App and firmware must be updated together.
- **Heading from the gyroscope:** it needs a phone with a gyroscope; without one, or when stopped, the GPS course is used and turns lag by about a second. It was only checked in simulation, not on a long real ride.
- **Smoothness limit:** at the default 400 m map radius slow movement advances in steps of a few pixels per second; a smaller map radius (150–250 m) makes it finer.
- **Matching versions:** the app sends `style`, `zoom_max`, `zoom_lead_s` and `motion` messages that an older firmware ignores or does not know, so update both together.

---

## Author and credits

- **Author:** [Vaibhav Kishan](https://github.com/vaibhavkishan)
- **Original project:** MotoPilot is built on top of **[DisplayConnect](https://github.com/malaq88/DisplayConnect)** by **Antonio Malaquias**, which provided the original app and ESP32 CYD firmware this project grew from. Thank you to the original author.
- **Navigation improvements:** the equal-axis projection, GPS filtering, remaining time/distance, and related ideas were adapted from **[PLZ-1/DisplayConnectV2.1](https://github.com/PLZ-1/DisplayConnectV2.1)** (LOLIN32 Lite + ST7796S 3.5″) and reworked for the ESP32-2432S028 CYD.

Map data: **© OpenStreetMap contributors**. Routing: OSRM/FOSSGIS. Search: Nominatim and Photon. Street geometry: Overpass.

---

## Development

- **License:** MIT — see [LICENSE](LICENSE). It keeps the original copyright notice of Antonio Malaquias and adds Vaibhav Kishan's.
- **Package name:** `com.vaibhavkishan.motopilot`
- **Internet cleartext:** Enabled for HTTP APIs (OSRM/OSM); CYD link is BLE, not `ws://`.
- **Android tests:** `./gradlew testDebugUnitTest`
- **Firmware host tests** (no board needed; any C++17 compiler):
  `c++ -std=c++17 -I DisplaySender DisplaySender/test/host_tests.cpp DisplaySender/nav_heading.cpp DisplaySender/motion_predictor.cpp DisplaySender/zoom_controller.cpp -o host_tests && ./host_tests`
- **Screen previews:** `DisplaySender/test/screens_preview.cpp` renders every screen on the host; `screens_preview.py` (Pillow) turns the output into the PNGs in `docs/screens`.
