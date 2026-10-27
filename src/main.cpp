#include <Arduino.h>
#include "config.h"
#include "config_manager.h"
#include "auth_manager.h"
#include "activity_log.h"
#include "gps_manager.h"
#include "relay_manager.h"
#include "schedule_manager.h"
#include "display_manager.h"
#include "web_server.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==============================================");
    Serial.println("  ESP32 Offline Bell, Schedule & Matrix System");
    Serial.println("==============================================");

    // 1. Initialize Relay Actuator & Manual Button
    Serial.println("[Init] Configuring Relay & Manual Trigger Button...");
    relayManager.begin(RELAY_PIN, RELAY_ACTIVE_HIGH, MANUAL_BELL_BUTTON_PIN);

    // 2. Initialize LittleFS and load saved schedules
    Serial.println("[Init] Initializing LittleFS Storage...");
    if (!scheduleManager.begin()) {
        Serial.println("[Init] Warning: LittleFS failed or empty, loaded default timetable.");
    }

    // 2b. Load persisted system settings (timezone, brightness)
    Serial.println("[Init] Loading saved system settings...");
    SystemConfig savedConfig = configManager.begin();
    authManager.begin();
    activityLog.begin();

    // 3. Initialize GPS Module (HardwareSerial 2)
    Serial.println("[Init] Starting GPS UART...");
    gpsManager.begin(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE, savedConfig.timezoneOffset);

    // 4. Initialize HUB75 LED Matrix
    Serial.printf("[Init] Initializing HUB75 LED Matrix (%dx%d DMA)...\n", TOTAL_SCREEN_WIDTH, TOTAL_SCREEN_HEIGHT);
    if (displayManager.begin()) {
        displayManager.setBrightness(savedConfig.brightness);
        displayManager.showBootSplash("Starting AP & Services...");
    } else {
        Serial.println("[Display] Error initializing Matrix panel!");
    }

    // 5. Start SoftAP & Web Portal
    Serial.println("[Init] Starting WiFi SoftAP & Offline Web Portal...");
    webServerManager.begin(DEFAULT_AP_SSID, DEFAULT_AP_PASS);

    activityLog.addEvent("System Started");

    delay(1500);
    Serial.println("[System] Ready! Connect to WiFi: " DEFAULT_AP_SSID " (192.168.4.1)");
}

void loop() {
    // 1. Process incoming GPS NMEA sentences & sync RTC
    gpsManager.update();

    // 2. Evaluate daily schedule & automatic bell trigger
    SystemDateTime dt = gpsManager.getCurrentDateTime();
    scheduleManager.update(dt);

    // 3. Update relay timing state machine & manual button
    relayManager.update();

    // 4. Handle Web requests & Captive Portal DNS
    webServerManager.update();

    // 5. Render live HUB75 display frame
    displayManager.update();
}
