#include "schedule_manager.h"

static const char* HOLIDAY_NVS_NAMESPACE = "schedule";

ScheduleManager scheduleManager;

void ScheduleManager::loadHolidayModeFromNvs() {
    if (!_prefs.begin(HOLIDAY_NVS_NAMESPACE, true)) {
        _holidayMode = false;
        return;
    }
    _holidayMode = _prefs.getBool("holiday", false);
    _prefs.end();
}

void ScheduleManager::saveHolidayModeToNvs() {
    if (!_prefs.begin(HOLIDAY_NVS_NAMESPACE, false)) {
        Serial.println("[Schedule] Failed to open NVS namespace for holiday mode!");
        return;
    }
    _prefs.putBool("holiday", _holidayMode);
    _prefs.end();
}

ScheduleManager::ScheduleManager()
    : _entryCount(0), _holidayMode(false), _lastError(""), _lastCheckedMinute(-1), _lastCheckedDay(-1) {
}

int ScheduleManager::timeToMinutes(int hour, int minute) {
    return (hour * 60) + minute;
}

bool ScheduleManager::isValidTimeRange(int startHour, int startMinute, int endHour, int endMinute) {
    if (startHour < 0 || startHour > 23 || endHour < 0 || endHour > 23) return false;
    if (startMinute < 0 || startMinute > 59 || endMinute < 0 || endMinute > 59) return false;
    return timeToMinutes(endHour, endMinute) > timeToMinutes(startHour, startMinute);
}

String ScheduleManager::getLastError() {
    return _lastError;
}

bool ScheduleManager::isDayMatching(int scheduleDay, int currentDay) {
    if (scheduleDay == -1) {
        // Mon (1) to Fri (5)
        return (currentDay >= 1 && currentDay <= 5);
    }
    if (scheduleDay == -2) {
        // Everyday
        return true;
    }
    return (scheduleDay == currentDay);
}

void ScheduleManager::loadDefaultSchedules() {
    _entryCount = 0;
    
    // Default standard school schedule (Monday to Friday: -1)
    struct DefaultItem {
        int sh, sm, eh, em;
        const char* name;
        bool rStart, rEnd;
    } defaults[] = {
        { 7, 30,  7, 45, "MORNING ASSEMBLY", true,  false},
        { 7, 45,  8, 30, "PERIOD 1: MATHEMATICS", true, false},
        { 8, 30,  9, 15, "PERIOD 2: ENGLISH",     true, false},
        { 9, 15, 10, 00, "PERIOD 3: SCIENCE",     true, false},
        {10, 00, 10, 30, "RECESS & SNACKS",      true, false},
        {10, 30, 11, 15, "PERIOD 4: HISTORY",     true, false},
        {11, 15, 12, 00, "PERIOD 5: COMPUTER LAB",true, false},
        {12, 00, 13, 00, "LUNCH BREAK",          true, false},
        {13, 00, 13, 45, "PERIOD 6: ART & CRAFT", true, false},
        {13, 45, 14, 30, "PERIOD 7: PHYSICAL ED", true, true}
    };
    
    int numItems = sizeof(defaults) / sizeof(defaults[0]);
    for (int i = 0; i < numItems && i < MAX_SCHEDULES; i++) {
        ScheduleEntry& e = _entries[_entryCount];
        e.id = i + 1;
        e.dayOfWeek = -1; // Weekdays
        e.startHour = defaults[i].sh;
        e.startMinute = defaults[i].sm;
        e.endHour = defaults[i].eh;
        e.endMinute = defaults[i].em;
        strncpy(e.subjectName, defaults[i].name, sizeof(e.subjectName) - 1);
        e.subjectName[sizeof(e.subjectName) - 1] = '\0';
        e.ringOnStart = defaults[i].rStart;
        e.ringOnEnd = defaults[i].rEnd;
        e.enabled = true;
        _entryCount++;
    }
    
    // Save to LittleFS
    saveSchedulesJson(getSchedulesJson());
}

bool ScheduleManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS Mount Failed! Formatting...");
        return false;
    }

    bool result = true;
    if (!LittleFS.exists(SCHEDULES_FILE_PATH)) {
        Serial.println("[Schedule] No existing schedule found. Loading defaults...");
        loadDefaultSchedules();
    } else {
        File file = LittleFS.open(SCHEDULES_FILE_PATH, "r");
        if (!file) {
            loadDefaultSchedules();
        } else {
            String content = file.readString();
            file.close();

            if (!saveSchedulesJson(content, false)) {
                loadDefaultSchedules();
            }
        }
    }

    // NVS is the authoritative store for holiday mode; it always overrides
    // whatever value the timetable file's parse may have set above.
    loadHolidayModeFromNvs();

    return result;
}

String ScheduleManager::getSchedulesJson() {
    JsonDocument doc;
    doc["holidayMode"] = _holidayMode;
    JsonArray arr = doc["schedules"].to<JsonArray>();
    
    for (int i = 0; i < _entryCount; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = _entries[i].id;
        obj["dayOfWeek"] = _entries[i].dayOfWeek;
        obj["startHour"] = _entries[i].startHour;
        obj["startMinute"] = _entries[i].startMinute;
        obj["endHour"] = _entries[i].endHour;
        obj["endMinute"] = _entries[i].endMinute;
        obj["subjectName"] = _entries[i].subjectName;
        obj["ringOnStart"] = _entries[i].ringOnStart;
        obj["ringOnEnd"] = _entries[i].ringOnEnd;
        obj["enabled"] = _entries[i].enabled;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool ScheduleManager::saveSchedulesJson(const String& jsonStr, bool persistHolidayMode) {
    _lastError = "";

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        _lastError = String("Invalid JSON: ") + error.c_str();
        Serial.printf("[Schedule] JSON Parse Error: %s\n", error.c_str());
        return false;
    }

    JsonArray arr = doc["schedules"].as<JsonArray>();
    if (arr.isNull()) {
        _lastError = "Missing 'schedules' array";
        return false;
    }

    // Validate into a scratch buffer first so a bad entry never corrupts the
    // live, already-working timetable (e.g. mid-save power loss or a typo).
    static ScheduleEntry scratch[MAX_SCHEDULES];
    int scratchCount = 0;
    int rowNum = 0;

    for (JsonObject obj : arr) {
        rowNum++;
        if (scratchCount >= MAX_SCHEDULES) {
            _lastError = "Too many schedule rows (max " + String(MAX_SCHEDULES) + ")";
            return false;
        }

        int sh = obj["startHour"] | 0;
        int sm = obj["startMinute"] | 0;
        int eh = obj["endHour"] | 0;
        int em = obj["endMinute"] | 0;
        const char* name = obj["subjectName"] | "";

        String trimmedName = String(name);
        trimmedName.trim();
        if (trimmedName.length() == 0) {
            _lastError = "Row " + String(rowNum) + ": Subject name cannot be empty";
            return false;
        }

        if (!isValidTimeRange(sh, sm, eh, em)) {
            _lastError = "Row " + String(rowNum) + " (" + trimmedName + "): End time must be after start time";
            return false;
        }

        ScheduleEntry& e = scratch[scratchCount];
        e.id = obj["id"] | (scratchCount + 1);
        e.dayOfWeek = obj["dayOfWeek"] | -1;
        e.startHour = sh;
        e.startMinute = sm;
        e.endHour = eh;
        e.endMinute = em;

        strncpy(e.subjectName, trimmedName.c_str(), sizeof(e.subjectName) - 1);
        e.subjectName[sizeof(e.subjectName) - 1] = '\0';

        e.ringOnStart = obj["ringOnStart"] | true;
        e.ringOnEnd = obj["ringOnEnd"] | false;
        e.enabled = obj["enabled"] | true;

        scratchCount++;
    }

    // All rows valid — commit.
    memcpy(_entries, scratch, sizeof(ScheduleEntry) * scratchCount);
    _entryCount = scratchCount;
    if (doc["holidayMode"].is<bool>()) {
        if (persistHolidayMode) {
            setHolidayMode(doc["holidayMode"].as<bool>());
        } else {
            _holidayMode = doc["holidayMode"].as<bool>();
        }
    }

    // Re-serialize from the validated in-memory state (not the raw request
    // body) so the file on flash always reflects exactly what was applied.
    File file = LittleFS.open(SCHEDULES_FILE_PATH, "w");
    if (!file) {
        _lastError = "Failed to open schedules file for writing";
        return false;
    }
    file.print(getSchedulesJson());
    file.close();
    return true;
}

void ScheduleManager::update(const SystemDateTime& dt) {
    if (!dt.valid) return;
    
    // Check if new minute arrived
    if (dt.minute != _lastCheckedMinute || dt.day != _lastCheckedDay) {
        _lastCheckedMinute = dt.minute;
        _lastCheckedDay = dt.day;
        checkBellTriggers(dt);
    }
}

void ScheduleManager::checkBellTriggers(const SystemDateTime& dt) {
    if (_holidayMode) return;
    
    for (int i = 0; i < _entryCount; i++) {
        const ScheduleEntry& e = _entries[i];
        if (!e.enabled) continue;
        if (!isDayMatching(e.dayOfWeek, dt.dayOfWeek)) continue;
        
        // Start time trigger
        if (e.ringOnStart && dt.hour == e.startHour && dt.minute == e.startMinute) {
            Serial.printf("[Bell] Triggering START ring for '%s'\n", e.subjectName);
            relayManager.ring();
            activityLog.addEvent(String("Scheduled Bell: ") + e.subjectName + " (Start)");
            return;
        }

        // End time trigger
        if (e.ringOnEnd && dt.hour == e.endHour && dt.minute == e.endMinute) {
            Serial.printf("[Bell] Triggering END ring for '%s'\n", e.subjectName);
            relayManager.ring();
            activityLog.addEvent(String("Scheduled Bell: ") + e.subjectName + " (End)");
            return;
        }
    }
}

CurrentPeriodInfo ScheduleManager::getCurrentPeriod(const SystemDateTime& dt) {
    CurrentPeriodInfo info;
    info.hasActivePeriod = false;
    info.remainingMinutes = 0;
    info.remainingSeconds = 0;
    info.progressPercent = 0.0f;
    strncpy(info.subjectName, "NO ACTIVE CLASS", sizeof(info.subjectName));
    
    if (!dt.valid || _holidayMode) {
        if (_holidayMode) strncpy(info.subjectName, "HOLIDAY MODE", sizeof(info.subjectName));
        return info;
    }
    
    int nowTotalSec = (dt.hour * 3600) + (dt.minute * 60) + dt.second;
    
    for (int i = 0; i < _entryCount; i++) {
        const ScheduleEntry& e = _entries[i];
        if (!e.enabled) continue;
        if (!isDayMatching(e.dayOfWeek, dt.dayOfWeek)) continue;
        
        int startSec = (e.startHour * 3600) + (e.startMinute * 60);
        int endSec   = (e.endHour * 3600) + (e.endMinute * 60);
        
        if (nowTotalSec >= startSec && nowTotalSec < endSec) {
            info.hasActivePeriod = true;
            strncpy(info.subjectName, e.subjectName, sizeof(info.subjectName) - 1);
            info.subjectName[sizeof(info.subjectName) - 1] = '\0';
            
            int diffSec = endSec - nowTotalSec;
            info.remainingMinutes = diffSec / 60;
            info.remainingSeconds = diffSec % 60;
            
            int totalPeriodSec = endSec - startSec;
            if (totalPeriodSec > 0) {
                info.progressPercent = ((float)(nowTotalSec - startSec) / (float)totalPeriodSec) * 100.0f;
            }
            info.startHour = e.startHour;
            info.startMinute = e.startMinute;
            info.endHour = e.endHour;
            info.endMinute = e.endMinute;
            return info;
        }
    }
    return info;
}

NextPeriodInfo ScheduleManager::getNextPeriod(const SystemDateTime& dt) {
    NextPeriodInfo info;
    info.hasNextPeriod = false;
    info.minutesUntilStart = 9999;
    strncpy(info.subjectName, "NONE", sizeof(info.subjectName));
    
    if (!dt.valid || _holidayMode) return info;
    
    int nowMinutes = timeToMinutes(dt.hour, dt.minute);
    int closestDiff = 99999;
    
    for (int i = 0; i < _entryCount; i++) {
        const ScheduleEntry& e = _entries[i];
        if (!e.enabled) continue;
        if (!isDayMatching(e.dayOfWeek, dt.dayOfWeek)) continue;
        
        int startMin = timeToMinutes(e.startHour, e.startMinute);
        if (startMin > nowMinutes) {
            int diff = startMin - nowMinutes;
            if (diff < closestDiff) {
                closestDiff = diff;
                info.hasNextPeriod = true;
                info.startHour = e.startHour;
                info.startMinute = e.startMinute;
                info.minutesUntilStart = diff;
                strncpy(info.subjectName, e.subjectName, sizeof(info.subjectName) - 1);
                info.subjectName[sizeof(info.subjectName) - 1] = '\0';
            }
        }
    }
    return info;
}

int ScheduleManager::getScheduleCount() {
    return _entryCount;
}

bool ScheduleManager::isHolidayMode() {
    return _holidayMode;
}

void ScheduleManager::setHolidayMode(bool holiday) {
    _holidayMode = holiday;
    saveHolidayModeToNvs();
}
