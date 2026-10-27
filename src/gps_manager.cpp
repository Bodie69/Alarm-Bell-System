#include "gps_manager.h"
#include "activity_log.h"

GPSManager gpsManager;

GPSManager::GPSManager() 
    : _gpsSerial(2), _timezoneOffsetHours(8.0f), _lastRtcSync(0), _hasFix(false) {
}

void GPSManager::begin(int rxPin, int txPin, uint32_t baudRate, float timezoneOffsetHours) {
    _timezoneOffsetHours = timezoneOffsetHours;
    _gpsSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

void GPSManager::setTimezoneOffset(float offsetHours) {
    if (isRtcSet()) {
        // Recover the true UTC instant using the OLD offset, then re-apply it
        // with the NEW offset — this corrects the displayed/used clock
        // immediately, regardless of whether GPS currently has a live fix.
        // Without this, changing timezone while indoors (no fix) would leave
        // the RTC silently wrong until the next GPS lock, which might never
        // come.
        time_t trueUtcNow = time(nullptr) - (time_t)(_timezoneOffsetHours * 3600.0f);
        time_t newLocalEpoch = trueUtcNow + (time_t)(offsetHours * 3600.0f);

        struct timeval tv;
        tv.tv_sec = newLocalEpoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    _timezoneOffsetHours = offsetHours;
}

float GPSManager::getTimezoneOffset() {
    return _timezoneOffsetHours;
}

void GPSManager::update() {
    while (_gpsSerial.available() > 0) {
        char c = _gpsSerial.read();
        _gps.encode(c);
    }

    bool hadFix = _hasFix;

    // Check if we have valid date, time, and location/satellites
    if (_gps.location.isValid() && _gps.date.isValid() && _gps.time.isValid() && _gps.satellites.value() >= 3) {
        _hasFix = true;
        // Sync RTC every 10 seconds or when not synced yet
        if (millis() - _lastRtcSync > 10000 || _lastRtcSync == 0) {
            syncInternalRtc();
            _lastRtcSync = millis();
        }
    } else {
        // If no GPS sentences updated in last 5 seconds
        if (_gps.location.age() > 5000) {
            _hasFix = false;
        }
    }

    // Surface fix acquired/lost transitions in the activity log — every other
    // clock-affecting event (manual browser sync, system boot) is already
    // logged, so GPS silently gaining/losing lock was the one gap.
    if (_hasFix && !hadFix) {
        activityLog.addEvent("GPS Fix Acquired (" + String(getSatellites()) + " sats)");
    } else if (!_hasFix && hadFix) {
        activityLog.addEvent("GPS Fix Lost");
    }
}

void GPSManager::syncInternalRtc() {
    if (!_gps.date.isValid() || !_gps.time.isValid()) return;
    
    struct tm t;
    t.tm_year = _gps.date.year() - 1900;
    t.tm_mon  = _gps.date.month() - 1;
    t.tm_mday = _gps.date.day();
    t.tm_hour = _gps.time.hour();
    t.tm_min  = _gps.time.minute();
    t.tm_sec  = _gps.time.second();
    t.tm_isdst = 0;
    
    // Convert GPS UTC time to epoch
    time_t utcEpoch = mktime(&t);
    if (utcEpoch == -1) return;
    
    // Apply local timezone offset (in seconds)
    time_t localEpoch = utcEpoch + (time_t)(_timezoneOffsetHours * 3600.0f);
    
    struct timeval tv;
    tv.tv_sec = localEpoch;
    tv.tv_usec = _gps.time.centisecond() * 10000;
    
    settimeofday(&tv, NULL);
}

void GPSManager::setManualTimeUtc(time_t utcEpochSeconds) {
    time_t localEpoch = utcEpochSeconds + (time_t)(_timezoneOffsetHours * 3600.0f);

    struct timeval tv;
    tv.tv_sec = localEpoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    _lastRtcSync = millis();
}

bool GPSManager::hasFix() {
    return _hasFix;
}

int GPSManager::getSatellites() {
    return _gps.satellites.isValid() ? _gps.satellites.value() : 0;
}

float GPSManager::getHdop() {
    return _gps.hdop.isValid() ? _gps.hdop.hdop() : 99.9f;
}

unsigned long GPSManager::getLastSyncAgeMs() {
    if (_lastRtcSync == 0) return 9999999;
    return millis() - _lastRtcSync;
}

bool GPSManager::isRtcSet() {
    // Fresh ESP32 boots with an internal clock near epoch 0 until something
    // (GPS fix or a manual sync) sets it. 100000s (~28h since epoch) is well
    // past any legitimate "unset" value, and far before any real GPS/manual
    // sync would land.
    return time(nullptr) > 100000;
}

SystemDateTime GPSManager::getCurrentDateTime() {
    SystemDateTime dt;
    time_t now = time(nullptr);
    struct tm timeinfo;

    if (localtime_r(&now, &timeinfo) != nullptr && isRtcSet()) {
        dt.year = timeinfo.tm_year + 1900;
        dt.month = timeinfo.tm_mon + 1;
        dt.day = timeinfo.tm_mday;
        dt.hour = timeinfo.tm_hour;
        dt.minute = timeinfo.tm_min;
        dt.second = timeinfo.tm_sec;
        dt.dayOfWeek = timeinfo.tm_wday; // 0=Sunday, 1=Monday...
        dt.valid = true;
    } else {
        dt.year = 2026;
        dt.month = 1;
        dt.day = 1;
        dt.hour = 0;
        dt.minute = 0;
        dt.second = 0;
        dt.dayOfWeek = 0;
        dt.valid = false;
    }
    return dt;
}

String GPSManager::getFormattedTime(bool includeSeconds) {
    SystemDateTime dt = getCurrentDateTime();
    char buf[16];
    if (includeSeconds) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
    }
    return String(buf);
}

String GPSManager::getFormattedDate() {
    SystemDateTime dt = getCurrentDateTime();
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char buf[20];
    int mIdx = constrain(dt.month - 1, 0, 11);
    snprintf(buf, sizeof(buf), "%02d %s %04d", dt.day, months[mIdx], dt.year);
    return String(buf);
}

String GPSManager::getDayName() {
    SystemDateTime dt = getCurrentDateTime();
    const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    int dIdx = constrain(dt.dayOfWeek, 0, 6);
    return String(days[dIdx]);
}
