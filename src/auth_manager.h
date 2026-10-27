#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include "config_manager.h"

// Handles the admin login credentials and a single active browser session.
// One active session at a time is sufficient for a single-administrator
// embedded device on an isolated offline WiFi AP, and keeps this simple.
class AuthManager {
public:
    AuthManager();
    void begin();

    bool verifyCredentials(const String& username, const String& password);

    // Starts a new session (invalidating any previous one) and returns its token.
    String login();
    void logout();
    bool isSessionValid(const String& token);

    // Returns false (leaving credentials unchanged) if currentPassword is wrong.
    bool changeCredentials(const String& currentPassword, const String& newUsername, const String& newPassword);

private:
    String _sessionToken;
    unsigned long _sessionCreatedMs;
    static const unsigned long SESSION_LIFETIME_MS = 8UL * 60UL * 60UL * 1000UL; // 8 hours

    String generateToken();
};

extern AuthManager authManager;

#endif // AUTH_MANAGER_H
