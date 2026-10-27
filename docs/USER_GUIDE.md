# ESP32 Offline Bell & Matrix System User Guide

This guide covers building, flashing, uploading the offline Web dashboard to LittleFS, and operating the system.

---

## 1. Quick Start with PlatformIO (Recommended)

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the **PlatformIO IDE** extension installed.
- ESP32 Development Board (ESP32-WROOM-32 / NodeMCU ESP32).

### Step 1: Open the Project
Open this repository's root folder (the one containing `platformio.ini`) in VS Code.

### Step 2: Upload Filesystem Image (Web UI Assets)
The Web UI (`index.html`, `login.html`, `style.css`, `app.js`, `login.js`, `schedules.json`) resides in the `data/` folder and must be flashed to the ESP32's LittleFS partition:
1. In PlatformIO, open the **PlatformIO Sidebar** (Alien icon).
2. Expand `esp32dev` $\rightarrow$ `Platform`.
3. Click **Upload Filesystem Image** (or run `pio run --target uploadfs`).

### Step 3: Build & Flash Firmware
1. Click **Upload** in PlatformIO (or run `pio run --target upload`).
2. Open the **Serial Monitor** at baud rate `115200` to observe boot messages and GPS synchronization status.

---

## 2. Using Arduino IDE (Alternative Method)

If using the Arduino IDE (2.x):
1. Install **ESP32 Board Package** from Board Manager.
2. Install required libraries via Library Manager:
   - `ESP32 HUB75 LED MATRIX PANEL DMA Display` (by Dustin Watts / mrfaptastic)
   - `Adafruit GFX Library`
   - `TinyGPSPlus` (by Mikal Hart)
   - `ArduinoJson` (v7 by Benoit Blanchon)
3. Set Tools:
   - Board: `ESP32 Dev Module`
   - Flash Size: `4MB (32Mb)`
   - Partition Scheme: `Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)` or `No OTA (2MB APP/2MB SPIFFS)`
   - Upload Speed: `921600`
4. Use the **ESP32 LittleFS Data Upload** tool to flash the `data/` folder.

---

## 3. Connecting to the Offline Web Portal

1. On your smartphone, tablet, or laptop, open Wi-Fi settings.
2. Connect to the Wi-Fi network:
   - **SSID**: `ESP32-Bell-System`
   - **Password**: `12345678`
3. Open any web browser and navigate to:
   - **URL**: `http://192.168.4.1`
4. You will land on a **Sign In** page — this is a second, independent login on top of the WiFi password, so that anyone who joins the network can't reconfigure bells without the admin credentials.
   - **Default login**: `admin` / `admin`
   - Change this immediately after your first login, from **System Settings → Admin Login Credentials**.
   - A session stays signed in for up to 8 hours, or until you click **Logout** in the sidebar.
   - **Recovery if locked out**: credentials are stored on the ESP32's LittleFS filesystem. Re-flashing the filesystem image (`pio run --target uploadfs`) wipes `/config.json` and resets the login back to the `admin` / `admin` default — this also resets timezone, brightness, and default bell duration, but does **not** touch your saved timetable in `/schedules.json`.
5. The dashboard will load instantly from the ESP32's internal flash memory without needing an internet connection.

---

## 4. Key Features & Dashboard Operations

The sidebar has five pages: **Dashboard**, **Bell Schedule**, **Manual Bell**, **GPS Status**, and **Settings**.

### 🏠 Dashboard
- **Current Time**, **Next Bell** (with a live countdown), and **System Status** at a glance.
- **Today's Bell Schedule** — today's periods only, with a live Done / In Session / Next / Pending status per row.
- **Recent Bell Activity** — a real, running log of the last 20 events (scheduled rings, manual triggers, physical button presses, boot), not sample data.

### 📅 Managing Schedules
- Navigate to the **Bell Schedule** page.
- Click **+ Add Period** to add a new slot.
- Specify the Day (e.g. Weekdays, Monday, or Everyday), Start Time, End Time, and Subject Name (e.g. `MATHEMATICS`, `RECESS`).
- Check **Ring Start** and/or **Ring End** to automatically trigger the bell.
- Click **💾 Save to ESP32 Flash** to persist changes across power cycles. Rows with an empty subject name or an end time that isn't after the start time are rejected (both in the browser and on the device) with a specific error message.

### 🔔 Manual Bell Ringing
- Navigate to the **Manual Bell** page.
- Press **🔔 Trigger Bell (5s)** to ring immediately. Every ring — scheduled, manual web, or the physical button — lasts a fixed 5 seconds; there's no pattern or duration to configure.
- Press **🛑 Emergency Bell Stop** to abort an ongoing ring immediately.
- Every manual trigger (from the web UI or the physical button) is recorded in Recent Bell Activity.

### 📍 GPS Status
- Live fix status, satellite count, and HDOP (lower = better accuracy).
- **Last Clock Sync** shows how long ago the RTC last received an authoritative time reading (GPS fix or manual browser sync) — this is informational, not an error indicator; the clock keeps running accurately between syncs.

### ⚙️ Timezone & Calibration
- Set your local UTC Offset (e.g. `UTC +8:00` for Singapore/Manila/Perth, `UTC +7:00` for Jakarta/Bangkok) from **Settings**. Changing it immediately corrects the clock's displayed/used time — this works even indoors with no live GPS fix, as long as the clock has been set at least once (by GPS or manual sync) since boot.
- Adjust LED Matrix brightness (10 to 255) to suit indoor classroom or outdoor hallway lighting.
- Toggle **Holiday Mode** during vacations to pause all scheduled bell ringing while keeping the clock and dashboard active.
- If testing indoors before connecting a GPS antenna, click **🕒 Sync Clock from Browser Now** (Settings page) to calibrate the ESP32 RTC from your device's clock. This sends true UTC time, so it's converted using whatever timezone the device is configured for — your browser and the ESP32 don't need to be set to the same timezone.
- Until the clock has been synced at least once (by GPS or manual sync), the Dashboard and GPS Status pages clearly show "Not Synced" / "Awaiting time sync" instead of a misleading placeholder time, and no bells will fire automatically.
