#ifndef ACTIVITY_LOG_H
#define ACTIVITY_LOG_H

#include <Arduino.h>
#include <ArduinoJson.h>

// A small in-memory (RAM-only, resets on reboot) ring buffer of recent
// system/bell events, shown on the dashboard's "Recent Bell Activity" table.
class ActivityLogManager {
public:
    ActivityLogManager();
    void begin();

    void addEvent(const String& description);
    String getEventsJson(); // newest-first

private:
    static const int MAX_EVENTS = 20;
    struct LogEntry {
        String time;
        String description;
    };
    LogEntry _entries[MAX_EVENTS];
    int _count;
    int _head; // index of the oldest valid entry

    String formatTimeNow();
};

extern ActivityLogManager activityLog;

#endif // ACTIVITY_LOG_H
