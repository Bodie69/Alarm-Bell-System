#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <cstring>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "config.h"
#include "gps_manager.h"
#include "relay_manager.h"
#include "activity_log.h"

struct ScheduleEntry {
    int id;
    int dayOfWeek; // 0=Sun, 1=Mon, ..., 6=Sat, -1=Weekdays (Mon-Fri)
    int startHour;
    int startMinute;
    int endHour;
    int endMinute;
    char subjectName[32];
    bool ringOnStart;
    bool ringOnEnd;
    bool enabled;
};

struct CurrentPeriodInfo {
    bool hasActivePeriod;
    char subjectName[32];
    int remainingMinutes;
    int remainingSeconds;
    float progressPercent;
    int startHour;
    int startMinute;
    int endHour;
    int endMinute;
};

struct NextPeriodInfo {
    bool hasNextPeriod;
    char subjectName[32];
    int startHour;
    int startMinute;
    int minutesUntilStart;
};

class ScheduleManager {
public:
    ScheduleManager();
    bool begin();
    void update(const SystemDateTime& dt);
    
    CurrentPeriodInfo getCurrentPeriod(const SystemDateTime& dt);
    NextPeriodInfo getNextPeriod(const SystemDateTime& dt);
    
    String getSchedulesJson();
    // persistHolidayMode: false only for the boot-time replay of the file
    // already on disk, so that a stale holidayMode field baked into an old
    // schedules.json can never overwrite the authoritative NVS value.
    bool saveSchedulesJson(const String& jsonStr, bool persistHolidayMode = true);
    String getLastError();

    void loadDefaultSchedules();
    int getScheduleCount();

    bool isHolidayMode();
    void setHolidayMode(bool holiday);

private:
    static const int MAX_SCHEDULES = 64;
    ScheduleEntry _entries[MAX_SCHEDULES];
    int _entryCount;
    bool _holidayMode;
    String _lastError;
    Preferences _prefs; // NVS namespace "schedule" — holds holidayMode only;
                         // the timetable itself stays in schedules.json on
                         // LittleFS since it's a variable-length array.

    int _lastCheckedMinute;
    int _lastCheckedDay;

    void checkBellTriggers(const SystemDateTime& dt);
    bool isDayMatching(int scheduleDay, int currentDay);
    int timeToMinutes(int hour, int minute);
    bool isValidTimeRange(int startHour, int startMinute, int endHour, int endMinute);
    void loadHolidayModeFromNvs();
    void saveHolidayModeToNvs();
};

extern ScheduleManager scheduleManager;

#endif // SCHEDULE_MANAGER_H
