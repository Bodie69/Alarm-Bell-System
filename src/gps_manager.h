#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <sys/time.h>
#include <time.h>

struct SystemDateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int dayOfWeek; // 0=Sunday, 1=Monday, ..., 6=Saturday
    bool valid;
};

class GPSManager {
public:
    GPSManager();
    void begin(int rxPin, int txPin, uint32_t baudRate, float timezoneOffsetHours);
    void update();
    
    bool hasFix();
    int getSatellites();
    float getHdop();
    unsigned long getLastSyncAgeMs();
    
    void setTimezoneOffset(float offsetHours);
    float getTimezoneOffset();
    
    SystemDateTime getCurrentDateTime();
    String getFormattedTime(bool includeSeconds = true);
    String getFormattedDate();
    String getDayName();
    
    // utcEpochSeconds must be true UTC (e.g. from Date.now()/1000 in the
    // browser) — never a caller's local wall-clock fields, which would
    // silently assume the browser and the ESP32 are configured to the same
    // timezone and set the clock wrong if they aren't.
    void setManualTimeUtc(time_t utcEpochSeconds);

private:
    TinyGPSPlus _gps;
    HardwareSerial _gpsSerial;
    float _timezoneOffsetHours;
    unsigned long _lastRtcSync;
    bool _hasFix;

    void syncInternalRtc();
    bool isRtcSet();
};

extern GPSManager gpsManager;

#endif // GPS_MANAGER_H
