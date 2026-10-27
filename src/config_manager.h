#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct SystemConfig {
    float timezoneOffset;
    uint8_t brightness;
    String authUsername;
    String authPasswordHash; // SHA-256 hex digest — the plaintext password is never stored
};

// Persists user-editable system settings (timezone, matrix brightness,
// login credentials) to the ESP32's NVS partition (via Preferences) so
// they survive a reboot. Holiday mode is likewise stored in NVS, but under
// ScheduleManager's own namespace (see schedule_manager.h) since it is a
// scheduling concern rather than a system setting.
class ConfigManager {
public:
    ConfigManager();

    // Loads the "sysconfig" NVS namespace if present, otherwise writes and
    // returns defaults.
    SystemConfig begin();

    // Persists the given values to NVS immediately.
    void save(float timezoneOffset, uint8_t brightness);

    // Persists updated login credentials immediately.
    void saveCredentials(const String& username, const String& passwordHash);

    SystemConfig current();

private:
    Preferences _prefs;
    SystemConfig _config;
    void writeToFlash();
};

extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
