#include "activity_log.h"
#include "gps_manager.h"

ActivityLogManager activityLog;

ActivityLogManager::ActivityLogManager() : _count(0), _head(0) {
}

void ActivityLogManager::begin() {
    // Intentionally RAM-only — the log resets on reboot, and "System Started"
    // (logged separately in main.cpp) is meant to be the first entry.
}

String ActivityLogManager::formatTimeNow() {
    SystemDateTime dt = gpsManager.getCurrentDateTime();
    int hour12 = dt.hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampm = (dt.hour < 12) ? "AM" : "PM";

    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", hour12, dt.minute, dt.second, ampm);
    return String(buf);
}

void ActivityLogManager::addEvent(const String& description) {
    int writeIdx = (_head + _count) % MAX_EVENTS;
    _entries[writeIdx].time = formatTimeNow();
    _entries[writeIdx].description = description;

    if (_count < MAX_EVENTS) {
        _count++;
    } else {
        _head = (_head + 1) % MAX_EVENTS; // buffer full — the write above just overwrote the oldest slot
    }
}

String ActivityLogManager::getEventsJson() {
    JsonDocument doc;
    JsonArray arr = doc["events"].to<JsonArray>();

    for (int i = _count - 1; i >= 0; i--) {
        int idx = (_head + i) % MAX_EVENTS;
        JsonObject obj = arr.add<JsonObject>();
        obj["time"] = _entries[idx].time;
        obj["activity"] = _entries[idx].description;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
