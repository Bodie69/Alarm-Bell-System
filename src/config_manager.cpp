#include "config_manager.h"
#include "crypto_utils.h"

static const char* NVS_NAMESPACE = "sysconfig";

ConfigManager configManager;

ConfigManager::ConfigManager() {
    _config.timezoneOffset = DEFAULT_TIMEZONE_OFFSET_HOURS;
    _config.brightness = DEFAULT_BRIGHTNESS;
    _config.authUsername = "admin";
    _config.authPasswordHash = sha256Hex("admin");
}

SystemConfig ConfigManager::begin() {
    // Opening read-only fails if the "sysconfig" namespace has never been
    // written, which is how we detect a first boot / freshly erased NVS.
    if (!_prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("[Config] No saved settings in NVS. Using defaults.");
        writeToFlash();
        return _config;
    }

    _config.timezoneOffset = _prefs.getFloat("tz", DEFAULT_TIMEZONE_OFFSET_HOURS);
    _config.brightness = _prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
    if (_config.brightness == 90) {
        _config.brightness = DEFAULT_BRIGHTNESS;
    }

    _config.authUsername = _prefs.getString("user", "admin");
    String defaultHash = sha256Hex("admin");
    _config.authPasswordHash = _prefs.getString("pwhash", defaultHash);

    if (_config.authPasswordHash.length() != 64) {
        _config.authPasswordHash = defaultHash;
    }
    if (_config.authUsername.length() == 0) {
        _config.authUsername = "admin";
    }

    _prefs.end();

    Serial.printf("[Config] Loaded from NVS: tz=%.1f brightness=%d user=%s\n",
                  _config.timezoneOffset, _config.brightness, _config.authUsername.c_str());

    return _config;
}

void ConfigManager::save(float timezoneOffset, uint8_t brightness) {
    _config.timezoneOffset = timezoneOffset;
    _config.brightness = brightness;
    writeToFlash();
}

void ConfigManager::saveCredentials(const String& username, const String& passwordHash) {
    _config.authUsername = username;
    _config.authPasswordHash = passwordHash;
    writeToFlash();
}

SystemConfig ConfigManager::current() {
    return _config;
}

void ConfigManager::writeToFlash() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[Config] Failed to open NVS namespace for writing!");
        return;
    }
    _prefs.putFloat("tz", _config.timezoneOffset);
    _prefs.putUChar("bright", _config.brightness);
    _prefs.putString("user", _config.authUsername);
    _prefs.putString("pwhash", _config.authPasswordHash);
    _prefs.end();
}
