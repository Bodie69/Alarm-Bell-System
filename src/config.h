#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// 128x64 HUB75 LED MATRIX PIN DEFINITIONS
// ==========================================
#define PANEL_RES_X 128 // Width of a single 128x64 panel
#define PANEL_RES_Y 64  // Height of a single panel
#define PANEL_CHAIN 1   // 1 chain (single 128x64 matrix panel)
#define TOTAL_SCREEN_WIDTH (PANEL_RES_X * PANEL_CHAIN)
#define TOTAL_SCREEN_HEIGHT (PANEL_RES_Y)

// HUB75 Pinout configuration for ESP32 WROOM-32
#define PIN_R1 25
#define PIN_G1 26
#define PIN_B1 27
#define PIN_R2 14
#define PIN_G2 12
#define PIN_B2 13

#define PIN_A 19
#define PIN_B 5
#define PIN_C 33
#define PIN_D 18
#define PIN_E 23 // Required for 64-row matrices (1/32 scan)

#define PIN_CLK 4
#define PIN_LAT 15
#define PIN_OE 32

// ==========================================
// GPS MODULE (NEO-6M / NEO-8M) PINS
// ==========================================
#define GPS_RX_PIN 16 // ESP32 RX2 connected to GPS TX
#define GPS_TX_PIN 17 // ESP32 TX2 connected to GPS RX
#define GPS_BAUD_RATE 9600

// ==========================================
// RELAY ACTUATOR & MANUAL BUTTON
// ==========================================
#define RELAY_PIN 22
#define RELAY_ACTIVE_HIGH                                                      \
  false // Set to false for standard Active-LOW relay modules
#define MANUAL_BELL_BUTTON_PIN 21 // Active LOW with internal pull-up

// ==========================================
// DEFAULT SETTINGS
// ==========================================
#define DEFAULT_TIMEZONE_OFFSET_HOURS 8 // UTC+8 (configurable via Web UI)
#define BELL_RING_DURATION_MS                                                  \
  10000 // Fixed 10-second ring — not configurable, matches the physical bell
        // hardware.
#define DEFAULT_AP_SSID "ESP32-Bell-System"
#define DEFAULT_AP_PASS "12345678"
#define DEFAULT_BRIGHTNESS 255 // 0 - 255 (HUB75 matrix brightness)

// Default School Name for Display Banner
#define SCHOOL_NAME "PANAGUINTINGAN ELEMENTARY SCHOOL"

// Storage paths
// Note: system settings (timezone/brightness/credentials) and holiday mode
// live in NVS (see ConfigManager / ScheduleManager), not LittleFS. Only the
// variable-length timetable itself is stored as a file.
#define SCHEDULES_FILE_PATH "/schedules.json"

#endif // CONFIG_H
