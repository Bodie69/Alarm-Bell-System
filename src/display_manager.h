#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"
#include "gps_manager.h"
#include "schedule_manager.h"
#include "relay_manager.h"

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void update();
    
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();
    
    void showBootSplash(const char* statusText);

private:
    MatrixPanel_I2S_DMA* _matrix;
    uint8_t _brightness;
    
    // Animation & Scroll states
    int _schoolScrollX;
    unsigned long _lastSchoolScrollTime;
    int _subjectScrollX;
    unsigned long _lastSubjectScrollTime;
    unsigned long _lastFrameTime;
    int _ringAnimFrame;
    unsigned long _lastRingAnimTime;
    
    // Colors
    uint16_t _colBlack;
    uint16_t _colWhite;
    uint16_t _colCyan;
    uint16_t _colYellow;
    uint16_t _colGreen;
    uint16_t _colRed;
    uint16_t _colOrange;
    uint16_t _colBlue;
    uint16_t _colPurple;
    uint16_t _colDarkGray;
    uint16_t _colAccentBg;

    void initColors();
    void renderTopSchoolBanner();
    void renderMidTime(const SystemDateTime& dt);
    void renderBottomDateAndDay(const SystemDateTime& dt);
    void renderRingingAlert();
    void drawGpsIcon(int x, int y, bool fix, int satellites);
};

extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
