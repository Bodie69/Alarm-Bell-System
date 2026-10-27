#include "relay_manager.h"
#include "config.h"

RelayManager relayManager;

RelayManager::RelayManager()
    : _relayPin(22), _activeHigh(false), _manualButtonPin(21),
      _isRinging(false), _ringStartTime(0),
      _lastButtonState(HIGH), _lastDebounceTime(0),
      _stableButtonState(HIGH), _pressHandled(false) {
}

void RelayManager::begin(int relayPin, bool activeHigh, int manualButtonPin) {
    _relayPin = relayPin;
    _activeHigh = activeHigh;
    _manualButtonPin = manualButtonPin;

    // Latch the OFF level before switching the pin to OUTPUT, otherwise the
    // ESP32's default output-register value (LOW) is briefly driven onto the
    // pin as soon as pinMode(OUTPUT) runs, which clicks an active-low relay
    // on for an instant at every boot.
    digitalWrite(_relayPin, _activeHigh ? LOW : HIGH);
    pinMode(_relayPin, OUTPUT);
    applyRelayState(false); // Initially OFF

    if (_manualButtonPin >= 0) {
        pinMode(_manualButtonPin, INPUT_PULLUP);
    }
}

void RelayManager::applyRelayState(bool state) {
    if (_activeHigh) {
        digitalWrite(_relayPin, state ? HIGH : LOW);
    } else {
        digitalWrite(_relayPin, state ? LOW : HIGH);
    }
}

void RelayManager::ring() {
    _isRinging = true;
    _ringStartTime = millis();
    applyRelayState(true);
}

void RelayManager::stop() {
    _isRinging = false;
    applyRelayState(false);
}

bool RelayManager::isRinging() {
    return _isRinging;
}

void RelayManager::update() {
    // 1. Check physical manual bell button with debounce
    // Edge-triggered: a ring is fired once per LOW-going press, and requires
    // the button to be released (HIGH) again before it can re-fire. Without
    // this latch, a button held down (or a stuck/shorted button line) would
    // re-trigger ring() the instant each 5s ring finished, cycling the relay
    // on/off forever.
    if (_manualButtonPin >= 0) {
        int reading = digitalRead(_manualButtonPin);
        if (reading != _lastButtonState) {
            _lastDebounceTime = millis();
        }
        if ((millis() - _lastDebounceTime) > 50 && reading != _stableButtonState) {
            _stableButtonState = reading;
            if (_stableButtonState == LOW && !_pressHandled) {
                _pressHandled = true;
                if (!_isRinging) {
                    ring();
                    activityLog.addEvent("Manual Bell Triggered (Physical Button)");
                }
            } else if (_stableButtonState == HIGH) {
                _pressHandled = false;
            }
        }
        _lastButtonState = reading;
    }

    // 2. Non-blocking ring timer
    if (_isRinging && (millis() - _ringStartTime >= BELL_RING_DURATION_MS)) {
        stop();
    }
}
