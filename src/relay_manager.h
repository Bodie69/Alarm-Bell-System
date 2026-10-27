#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>
#include "activity_log.h"

// Drives the bell relay for a single, fixed BELL_RING_DURATION_MS pulse —
// every trigger (scheduled, manual web, or the physical button) rings for
// the same duration. There is no pattern/duration selection; this matches
// the physical bell hardware, which only ever needs one ring length.
class RelayManager {
public:
    RelayManager();
    void begin(int relayPin, bool activeHigh, int manualButtonPin);
    void update();

    void ring();
    void stop();

    bool isRinging();

private:
    int _relayPin;
    bool _activeHigh;
    int _manualButtonPin;

    bool _isRinging;
    unsigned long _ringStartTime;

    // Button debounce
    int _lastButtonState;
    unsigned long _lastDebounceTime;
    int _stableButtonState;
    bool _pressHandled;

    void applyRelayState(bool state);
};

extern RelayManager relayManager;

#endif // RELAY_MANAGER_H
