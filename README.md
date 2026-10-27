# ESP32 Offline Bell & Matrix System

A school bell controller that runs on an ESP32 without needing internet access. It rings a bell through a relay on a weekly timetable, keeps time from a GPS module, and shows the clock and current period on a 128x64 HUB75 RGB LED matrix. You manage everything from a web dashboard that the ESP32 serves over its own Wi-Fi hotspot.

## Features

- **Scheduled bells:** a weekly timetable of periods. Each period can ring at its start, its end, or both, and can apply to every day or to one day of the week.
- **GPS timekeeping:** a NEO-6M/8M module supplies the time, with an adjustable timezone offset (default UTC+8).
- **LED matrix display:** a 128x64 HUB75 panel driven by DMA shows the school name, the time and the current subject.
- **Offline web dashboard:** the ESP32 runs a SoftAP with a captive portal. From the dashboard you can edit schedules, ring or stop the bell, switch on holiday mode, change brightness and timezone, and view an activity log.
- **Admin login:** the dashboard sits behind a login that is separate from the Wi-Fi password, and logins are stored as SHA-256 hashes.
- **Manual bell button:** a physical push button rings the bell on demand. Each ring lasts a fixed 10 seconds.

## Hardware

| Component | Notes |
|---|---|
| ESP32-WROOM-32 dev board | `esp32dev` |
| 128x64 HUB75 RGB LED matrix | 1/32 scan, needs the E pin |
| NEO-6M / NEO-8M GPS module | UART2 (RX 16, TX 17), 9600 baud |
| Relay module | GPIO 22, active-LOW by default |
| Push button | GPIO 21 to GND (uses the internal pull-up) |
| 5V 4–5A power supply | The matrix alone can draw up to 4A |

All pins are defined in [`src/config.h`](src/config.h). Full wiring tables and the power layout are in [`docs/WIRING_DIAGRAM.md`](docs/WIRING_DIAGRAM.md).

> **Important:** do not power the matrix from the ESP32 or from USB. Use a dedicated 5V supply, and connect the grounds of every module together.

## Getting started

Requires [PlatformIO](https://platformio.org/) (the VS Code extension or the CLI).

```sh
# 1. Flash the web UI to LittleFS
pio run --target uploadfs

# 2. Build and flash the firmware
pio run --target upload

# 3. Watch boot and GPS logs
pio device monitor
```

`platformio.ini` sets `upload_port = COM12`. Change it to your board's port, or delete the line to let PlatformIO detect the port.

Then:

1. Join the Wi-Fi network **`ESP32-Bell-System`** (password `12345678`).
2. Open **http://192.168.4.1**.
3. Sign in with **`admin` / `admin`**, then change the login under **System Settings → Admin Login Credentials**.

> **Security:** the Wi-Fi password and admin login above are public defaults. Change the Wi-Fi password in `src/config.h` (`DEFAULT_AP_PASS`) and the admin login from the dashboard before you install a unit.

Arduino IDE setup, dashboard usage and recovery steps are covered in [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).

## Project layout

```
src/            Firmware modules (GPS, relay, schedule, display, web server, auth, config, activity log)
data/           Web UI served from LittleFS (HTML/CSS/JS, gzipped copies, default schedules.json)
tools/          inline_assets.py: inlines CSS/JS into the HTML and regenerates the .gz files
docs/           User guide and wiring diagram
```

### Editing the web UI

After you change `data/style.css`, `data/app.js` or `data/login.js`, regenerate the inlined pages and gzipped files from the repo root, then flash the filesystem again:

```sh
python tools/inline_assets.py
pio run --target uploadfs
```

## Configuration

Compile-time defaults are in `src/config.h`: pins, panel size, AP SSID and password, default timezone, brightness and the school name shown on the display. Settings you change at runtime (timezone, brightness, login, holiday mode) are saved in NVS. The timetable is saved to `/schedules.json` on LittleFS.

## HTTP API

The dashboard uses a small JSON API, and every endpoint except login requires a session.

| Method | Path | Purpose |
|---|---|---|
| POST | `/api/login`, `/api/logout` | Start / end a session |
| POST | `/api/auth/change` | Change admin credentials |
| GET | `/api/status` | Time, GPS, relay and display status |
| GET / POST | `/api/schedule` | Read / save the timetable |
| POST | `/api/ring`, `/api/stop` | Manual bell control |
| POST | `/api/config` | Timezone, brightness, etc. |
| POST | `/api/timesync` | Set the clock manually (e.g. from the browser) |
| GET | `/api/activity` | Recent activity log |

## Libraries

- [ESP32 HUB75 LED MATRIX PANEL DMA Display](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) / BusIO
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)
- [ArduinoJson v7](https://arduinojson.org/)
