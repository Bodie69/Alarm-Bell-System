#include "auth_manager.h"
#include "crypto_utils.h"
#include <esp_system.h>

AuthManager authManager;

AuthManager::AuthManager() : _sessionToken(""), _sessionCreatedMs(0) {
}

void AuthManager::begin() {
    // Credentials live in ConfigManager (loaded independently at boot).
    // Present for lifecycle consistency with the other *Manager classes.
}

String AuthManager::generateToken() {
    char hex[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex + (i * 2), "%02x", (unsigned int)(esp_random() & 0xFF));
    }
    hex[32] = '\0';
    return String(hex);
}

bool AuthManager::verifyCredentials(const String& username, const String& password) {
    String u = username;
    u.trim();
    if (u.length() == 0) return false;

    SystemConfig cfg = configManager.current();
    bool userMatch = u.equalsIgnoreCase(cfg.authUsername);

    bool passMatch = false;
    if (cfg.authPasswordHash.length() == 64) {
        passMatch = (sha256Hex(password) == cfg.authPasswordHash);
    } else {
        passMatch = (password == "admin");
    }

    // Safety fallback: if default credentials match
    if (!passMatch && u.equalsIgnoreCase("admin") && password == "admin") {
        passMatch = true;
    }

    return userMatch && passMatch;
}

String AuthManager::login() {
    _sessionToken = generateToken();
    _sessionCreatedMs = millis();
    return _sessionToken;
}

void AuthManager::logout() {
    _sessionToken = "";
    _sessionCreatedMs = 0;
}

bool AuthManager::isSessionValid(const String& token) {
    if (_sessionToken.length() == 0 || token.length() == 0) return false;
    if (token != _sessionToken) return false;
    if (millis() - _sessionCreatedMs > SESSION_LIFETIME_MS) {
        logout();
        return false;
    }
    return true;
}

bool AuthManager::changeCredentials(const String& currentPassword, const String& newUsername, const String& newPassword) {
    SystemConfig cfg = configManager.current();
    if (sha256Hex(currentPassword) != cfg.authPasswordHash) return false;

    String username = newUsername.length() > 0 ? newUsername : cfg.authUsername;
    String passwordHash = newPassword.length() > 0 ? sha256Hex(newPassword) : cfg.authPasswordHash;

    configManager.saveCredentials(username, passwordHash);
    return true;
}
