#include "web_server.h"

WebServerManager webServerManager;

WebServerManager::WebServerManager() : _server(80) {
}

void WebServerManager::begin(const char* ssid, const char* password) {
    // Configure SoftAP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[WiFi AP] SSID: %s | IP: %s\n", ssid, IP.toString().c_str());
    
    // Setup Captive Portal DNS on port 53
    _dnsServer.start(53, "*", IP);
    
    // CRITICAL: Register "Cookie", "Authorization", and "Accept-Encoding" so WebServer extracts them!
    const char* headerKeys[] = {"Cookie", "Authorization", "Accept-Encoding"};
    _server.collectHeaders(headerKeys, 3);

    setupRoutes();
    _server.begin();
    Serial.println("[HTTP] Web server started on port 80");
}

void WebServerManager::setupRoutes() {
    _server.enableCORS(true);

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/index.html", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/style.css", HTTP_GET, [this]() { 
        if (!loadFromLittleFS("/style.css", "text/css")) {
            _server.send(404, "text/plain", "Not found");
        }
    });
    _server.on("/app.js", HTTP_GET, [this]() { 
        if (!loadFromLittleFS("/app.js", "application/javascript")) {
            _server.send(404, "text/plain", "Not found");
        }
    });

    // Login (unauthenticated) routes
    _server.on("/login.html", HTTP_GET, [this]() { handleLoginPage(); });
    _server.on("/login.js", HTTP_GET, [this]() { 
        if (!loadFromLittleFS("/login.js", "application/javascript")) {
            _server.send(404, "text/plain", "Not found");
        }
    });
    _server.on("/api/login", HTTP_POST, [this]() { handleLoginPost(); });
    _server.on("/api/login", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/logout", HTTP_POST, [this]() { handleLogout(); });
    _server.on("/api/logout", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/auth/change", HTTP_POST, [this]() { handleChangeCredentials(); });
    _server.on("/api/auth/change", HTTP_OPTIONS, [this]() { handleOptions(); });

    // REST API Routes (authenticated)
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/status", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/activity", HTTP_GET, [this]() { handleGetActivity(); });
    _server.on("/api/activity", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/schedule", HTTP_GET, [this]() { handleGetSchedule(); });
    _server.on("/api/schedule", HTTP_POST, [this]() { handleSaveSchedule(); });
    _server.on("/api/schedule", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/ring", HTTP_POST, [this]() { handleRing(); });
    _server.on("/api/ring", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
    _server.on("/api/stop", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/config", HTTP_POST, [this]() { handleConfig(); });
    _server.on("/api/config", HTTP_OPTIONS, [this]() { handleOptions(); });
    _server.on("/api/timesync", HTTP_POST, [this]() { handleTimeSync(); });
    _server.on("/api/timesync", HTTP_OPTIONS, [this]() { handleOptions(); });

    // Captive portal redirects
    _server.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });

    _server.onNotFound([this]() { handleNotFound(); });
}

bool WebServerManager::isAuthenticated() {
    // 1. Check URL query parameter first (?token=...)
    if (_server.hasArg("token")) {
        String token = _server.arg("token");
        token.trim();
        if (authManager.isSessionValid(token)) return true;
    }

    // 2. Check Authorization header (Bearer <token>)
    if (_server.hasHeader("Authorization")) {
        String auth = _server.header("Authorization");
        if (auth.startsWith("Bearer ")) {
            String token = auth.substring(7);
            token.trim();
            if (authManager.isSessionValid(token)) return true;
        }
    }

    // 3. Check Cookie header (session=<token>)
    if (_server.hasHeader("Cookie")) {
        String cookie = _server.header("Cookie");
        int idx = cookie.indexOf("session=");
        if (idx >= 0) {
            int start = idx + 8;
            int end = cookie.indexOf(';', start);
            String token = (end < 0) ? cookie.substring(start) : cookie.substring(start, end);
            token.trim();
            if (authManager.isSessionValid(token)) return true;
        }
    }

    return false;
}

void WebServerManager::sendUnauthorized() {
    _server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Not authenticated\"}");
}

bool WebServerManager::loadFromLittleFS(const String& path, const String& contentType) {
    bool clientAcceptsGzip = _server.hasHeader("Accept-Encoding") && 
                             _server.header("Accept-Encoding").indexOf("gzip") >= 0;

    // 1. Try pre-compressed GZIP archive first (.gz) if client supports gzip
    if (clientAcceptsGzip) {
        String gzPath = path + ".gz";
        if (LittleFS.exists(gzPath)) {
            File file = LittleFS.open(gzPath, "r");
            _server.sendHeader("Cache-Control", "public, max-age=86400");
            // DO NOT manually call sendHeader("Content-Encoding", "gzip")!
            // WebServer::streamFile automatically checks if file.name() ends with ".gz"
            // and adds "Content-Encoding: gzip". Adding it manually causes duplicate headers
            // and ERR_CONTENT_DECODING_FAILED (black screen in Chrome)!
            size_t sent = _server.streamFile(file, contentType);
            file.close();
            Serial.printf("[HTTP] 200 %s (gzip %u B) -> %s\n", gzPath.c_str(), (unsigned)sent, _server.client().remoteIP().toString().c_str());
            return true;
        }
    }

    // 2. Fallback to uncompressed file
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        _server.sendHeader("Cache-Control", "public, max-age=86400");
        size_t sent = _server.streamFile(file, contentType);
        file.close();
        Serial.printf("[HTTP] 200 %s (raw %u B) -> %s\n", path.c_str(), (unsigned)sent, _server.client().remoteIP().toString().c_str());
        return true;
    }

    Serial.printf("[HTTP] 404 %s -> %s\n", path.c_str(), _server.client().remoteIP().toString().c_str());
    return false;
}

void WebServerManager::handleRoot() {
    bool auth = isAuthenticated();
    Serial.printf("[HTTP] GET / from %s (auth=%d)\n", _server.client().remoteIP().toString().c_str(), auth ? 1 : 0);
    if (!auth) {
        _server.sendHeader("Location", "/login.html", true);
        _server.send(302, "text/plain", "");
        return;
    }

    if (_server.hasArg("token")) {
        String token = _server.arg("token");
        token.trim();
        _server.sendHeader("Set-Cookie", "session=" + token + "; Path=/; Max-Age=28800");
    }

    if (!loadFromLittleFS("/index.html", "text/html")) {
        // Fallback minimal HTML in case LittleFS data is not uploaded yet
        String html = "<!DOCTYPE html><html><head><title>ESP32 Bell System</title>"
                      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                      "<style>body{font-family:sans-serif;background:#111;color:#eee;text-align:center;padding:20px;}"
                      ".btn{background:#00d2ff;color:#000;padding:12px 24px;border:none;border-radius:6px;font-size:18px;cursor:pointer;margin:10px;}"
                      "</style></head><body>"
                      "<h1>ESP32 Bell & Schedule System</h1>"
                      "<p>Please upload the <code>data/</code> folder using LittleFS to access the full UI.</p>"
                      "<button class='btn' onclick=\"fetch('/api/ring',{method:'POST'}).then(()=>alert('Bell Ringing!'))\">🔔 Test Bell Ring</button>"
                      "</body></html>";
        _server.send(200, "text/html", html);
    }
}

void WebServerManager::handleStatus() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    SystemDateTime dt = gpsManager.getCurrentDateTime();
    CurrentPeriodInfo current = scheduleManager.getCurrentPeriod(dt);
    NextPeriodInfo next = scheduleManager.getNextPeriod(dt);
    
    JsonDocument doc;
    doc["time"] = gpsManager.getFormattedTime(true);
    doc["date"] = gpsManager.getFormattedDate();
    doc["day"] = gpsManager.getDayName();
    doc["timeValid"] = dt.valid;
    doc["lastSyncAgeMs"] = gpsManager.getLastSyncAgeMs();
    doc["gpsFix"] = gpsManager.hasFix();
    doc["satellites"] = gpsManager.getSatellites();
    doc["hdop"] = gpsManager.getHdop();
    doc["timezone"] = gpsManager.getTimezoneOffset();
    
    doc["isRinging"] = relayManager.isRinging();
    doc["holidayMode"] = scheduleManager.isHolidayMode();
    doc["brightness"] = displayManager.getBrightness();
    
    JsonObject curObj = doc["current"].to<JsonObject>();
    curObj["hasActive"] = current.hasActivePeriod;
    curObj["subject"] = current.subjectName;
    curObj["remMinutes"] = current.remainingMinutes;
    curObj["remSeconds"] = current.remainingSeconds;
    curObj["progress"] = current.progressPercent;
    
    JsonObject nextObj = doc["next"].to<JsonObject>();
    nextObj["hasNext"] = next.hasNextPeriod;
    nextObj["subject"] = next.subjectName;
    nextObj["startHour"] = next.startHour;
    nextObj["startMinute"] = next.startMinute;
    nextObj["minsUntil"] = next.minutesUntilStart;
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void WebServerManager::handleGetSchedule() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    String jsonStr = scheduleManager.getSchedulesJson();
    _server.send(200, "application/json", jsonStr);
}

void WebServerManager::handleGetActivity() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    String jsonStr = activityLog.getEventsJson();
    _server.send(200, "application/json", jsonStr);
}

void WebServerManager::handleSaveSchedule() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing body\"}");
        return;
    }
    
    String body = _server.arg("plain");
    if (scheduleManager.saveSchedulesJson(body)) {
        _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Schedule saved successfully\"}");
    } else {
        JsonDocument errDoc;
        errDoc["status"] = "error";
        errDoc["message"] = scheduleManager.getLastError();
        String errResponse;
        serializeJson(errDoc, errResponse);
        _server.send(400, "application/json", errResponse);
    }
}

void WebServerManager::handleRing() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    relayManager.ring();
    activityLog.addEvent("Bell Triggered Manually (Web)");
    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Bell triggered\"}");
}

void WebServerManager::handleStop() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    relayManager.stop();
    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Bell stopped\"}");
}

void WebServerManager::handleConfig() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing body\"}");
        return;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, _server.arg("plain"));
    if (err) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc["timezone"].is<float>()) {
        gpsManager.setTimezoneOffset(doc["timezone"].as<float>());
    }
    if (doc["brightness"].is<uint8_t>()) {
        displayManager.setBrightness(doc["brightness"].as<uint8_t>());
    }

    // Persist timezone/brightness together so a reboot restores exactly
    // what's configured, regardless of which field(s) changed.
    configManager.save(gpsManager.getTimezoneOffset(), displayManager.getBrightness());

    if (doc["holidayMode"].is<bool>()) {
        // setHolidayMode() persists to NVS immediately, independent of the
        // timetable file.
        scheduleManager.setHolidayMode(doc["holidayMode"].as<bool>());
    }

    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Settings updated\"}");
}

void WebServerManager::handleTimeSync() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing body\"}");
        return;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, _server.arg("plain"));
    if (err) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    long epochSeconds = doc["utcEpochSeconds"] | 0;
    if (epochSeconds <= 0) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing or invalid utcEpochSeconds\"}");
        return;
    }

    gpsManager.setManualTimeUtc((time_t)epochSeconds);
    activityLog.addEvent("Clock Manually Synced from Browser");
    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Manual time set\"}");
}

void WebServerManager::handleLoginPage() {
    Serial.printf("[HTTP] GET /login.html from %s\n", _server.client().remoteIP().toString().c_str());
    if (!loadFromLittleFS("/login.html", "text/html")) {
        // Safe emergency fallback so users NEVER get a blank screen or 404
        _server.send(200, "text/html", 
            "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Sign In - Bell System</title>"
            "<style>body{font-family:system-ui,sans-serif;background:#0f2c54;color:#fff;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px;box-sizing:border-box;}"
            ".card{background:#fff;color:#1e293b;padding:32px;border-radius:12px;width:100%;max-width:380px;box-shadow:0 20px 40px rgba(0,0,0,0.3);text-align:center;}"
            ".card h2{margin:0 0 8px;font-size:18px;color:#0f2c54;}"
            ".card p{margin:0 0 20px;font-size:13px;color:#64748b;}"
            "label{display:block;text-align:left;font-size:13px;font-weight:600;margin-bottom:6px;color:#334155;}"
            "input{width:100%;padding:12px;margin-bottom:16px;border:1px solid #cbd5e1;border-radius:8px;box-sizing:border-box;font-size:15px;}"
            "button{width:100%;padding:13px;background:#2f6fed;color:#fff;border:none;border-radius:8px;font-size:15px;font-weight:700;cursor:pointer;}"
            ".err{color:#dc2626;background:#fef2f2;border:1px solid #fecaca;padding:10px;border-radius:8px;font-size:13px;margin-bottom:14px;display:none;}"
            "</style></head><body><div class='card'>"
            "<div style='font-size:40px;margin-bottom:10px;'>🔔</div>"
            "<h2>SMART BELL SYSTEM</h2><p>Panaguintingan Elementary School</p>"
            "<div id='err' class='err'></div>"
            "<form id='f'>"
            "<label>Username</label><input id='u' value='admin' required>"
            "<label>Password</label><input id='p' type='password' value='admin' required>"
            "<button type='submit' id='b'>Sign In</button>"
            "</form></div>"
            "<script>document.getElementById('f').onsubmit=async e=>{"
            "e.preventDefault();const u=document.getElementById('u').value,p=document.getElementById('p').value,b=document.getElementById('b'),er=document.getElementById('err');"
            "b.disabled=true;b.innerText='Signing In...';er.style.display='none';"
            "try{"
            "const fd=new URLSearchParams();fd.append('username',u);fd.append('password',p);"
            "const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd.toString()});"
            "const d=await r.json().catch(()=>null);"
            "if(r.ok&&d&&d.token){localStorage.setItem('session_token',d.token);location.href='/?token='+encodeURIComponent(d.token);return;}"
            "er.innerText=(d&&d.message)?d.message:'Invalid credentials';er.style.display='block';"
            "}catch(x){er.innerText='Network error reaching ESP32';er.style.display='block';}"
            "finally{b.disabled=false;b.innerText='Sign In';}};</script></body></html>"
        );
    }
}

void WebServerManager::handleLoginPost() {
    String username = "";
    String password = "";

    if (_server.hasArg("username")) {
        username = _server.arg("username");
        password = _server.hasArg("password") ? _server.arg("password") : "";
    } else if (_server.hasArg("plain")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (!err) {
            username = doc["username"] | "";
            password = doc["password"] | "";
        }
    }

    username.trim();
    password.trim();

    _server.sendHeader("Access-Control-Allow-Origin", "*");

    if (username.length() == 0 || password.length() == 0) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing username or password\"}");
        return;
    }

    if (!authManager.verifyCredentials(username, password)) {
        Serial.printf("[HTTP] Login FAILED for '%s' from %s\n", username.c_str(), _server.client().remoteIP().toString().c_str());
        _server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Incorrect username or password\"}");
        return;
    }

    String token = authManager.login();
    String cookie = "session=" + token + "; Path=/; Max-Age=28800";
    _server.sendHeader("Set-Cookie", cookie);
    Serial.printf("[HTTP] Login SUCCESS for '%s' from %s (token: %s)\n", username.c_str(), _server.client().remoteIP().toString().c_str(), token.c_str());
    String response = "{\"status\":\"ok\",\"message\":\"Logged in\",\"token\":\"" + token + "\"}";
    _server.send(200, "application/json", response);
}

void WebServerManager::handleLogout() {
    authManager.logout();
    _server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Logged out\"}");
}

void WebServerManager::handleChangeCredentials() {
    if (!isAuthenticated()) { sendUnauthorized(); return; }

    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, _server.arg("plain"));
    if (err) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* curPwC = doc["currentPassword"] | "";
    const char* newUserC = doc["newUsername"] | "";
    const char* newPwC = doc["newPassword"] | "";
    String currentPassword = String(curPwC);
    String newUsername = String(newUserC);
    newUsername.trim();
    String newPassword = String(newPwC);

    if (newPassword.length() > 0 && newPassword.length() < 4) {
        _server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"New password must be at least 4 characters\"}");
        return;
    }

    if (!authManager.changeCredentials(currentPassword, newUsername, newPassword)) {
        _server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Current password is incorrect\"}");
        return;
    }

    _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Credentials updated\"}");
}

void WebServerManager::handleOptions() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    _server.sendHeader("Access-Control-Max-Age", "86400");
    _server.sendHeader("Connection", "close");
    _server.send(204);
}

void WebServerManager::handleNotFound() {
    if (_server.method() == HTTP_OPTIONS) {
        handleOptions();
        return;
    }

    String uri = _server.uri();
    String contentType = "text/plain";
    if (uri.endsWith(".html")) contentType = "text/html";
    else if (uri.endsWith(".css")) contentType = "text/css";
    else if (uri.endsWith(".js")) contentType = "application/javascript";
    else if (uri.endsWith(".json")) contentType = "application/json";
    else if (uri.endsWith(".ico")) contentType = "image/x-icon";
    else if (uri.endsWith(".svg")) contentType = "image/svg+xml";

    if (loadFromLittleFS(uri, contentType)) {
        return;
    }

    // Redirect unknown queries to root for captive portal
    _server.sendHeader("Connection", "close");
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
}

void WebServerManager::update() {
    for (int i = 0; i < 4; i++) {
        _dnsServer.processNextRequest();
    }
    _server.handleClient();
}
