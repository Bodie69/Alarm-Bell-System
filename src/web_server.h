#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "config_manager.h"
#include "auth_manager.h"
#include "gps_manager.h"
#include "schedule_manager.h"
#include "relay_manager.h"
#include "display_manager.h"
#include "activity_log.h"

class WebServerManager {
public:
    WebServerManager();
    void begin(const char* ssid = DEFAULT_AP_SSID, const char* password = DEFAULT_AP_PASS);
    void update();

private:
    WebServer _server;
    DNSServer _dnsServer;
    
    void setupRoutes();
    void handleRoot();
    void handleStatus();
    void handleGetSchedule();
    void handleSaveSchedule();
    void handleGetActivity();
    void handleRing();
    void handleStop();
    void handleConfig();
    void handleTimeSync();
    void handleOptions();
    void handleNotFound();

    void handleLoginPage();
    void handleLoginPost();
    void handleLogout();
    void handleChangeCredentials();
    bool isAuthenticated();
    void sendUnauthorized();

    bool loadFromLittleFS(const String& path, const String& contentType);
};

extern WebServerManager webServerManager;

#endif // WEB_SERVER_H
