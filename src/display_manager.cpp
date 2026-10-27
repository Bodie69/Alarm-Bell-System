#include "display_manager.h"

DisplayManager displayManager;

DisplayManager::DisplayManager()
    : _matrix(nullptr), _brightness(DEFAULT_BRIGHTNESS),
      _schoolScrollX(TOTAL_SCREEN_WIDTH), _lastSchoolScrollTime(0),
      _subjectScrollX(TOTAL_SCREEN_WIDTH), _lastSubjectScrollTime(0),
      _lastFrameTime(0), _ringAnimFrame(0), _lastRingAnimTime(0) {}

bool DisplayManager::begin() {
  HUB75_I2S_CFG mxconfig(
      PANEL_RES_X, // Module width (64)
      PANEL_RES_Y, // Module height (64)
      PANEL_CHAIN  // Total panels chained (1 -> 64 total width)
  );

  // Assign custom pins
  mxconfig.gpio.r1 = PIN_R1;
  mxconfig.gpio.g1 = PIN_G1;
  mxconfig.gpio.b1 = PIN_B1;
  mxconfig.gpio.r2 = PIN_R2;
  mxconfig.gpio.g2 = PIN_G2;
  mxconfig.gpio.b2 = PIN_B2;
  mxconfig.gpio.a = PIN_A;
  mxconfig.gpio.b = PIN_B;
  mxconfig.gpio.c = PIN_C;
  mxconfig.gpio.d = PIN_D;
  mxconfig.gpio.e = PIN_E;
  mxconfig.gpio.clk = PIN_CLK;
  mxconfig.gpio.lat = PIN_LAT;
  mxconfig.gpio.oe = PIN_OE;

  // Set double buffering & clock speed for stable DMA
  mxconfig.double_buff = true;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;

  // Anti-ghosting: keep OE blanked longer around each row latch so the
  // previous row's data doesn't bleed into the next row
  mxconfig.latch_blanking = 4;

  _matrix = new MatrixPanel_I2S_DMA(mxconfig);
  if (!_matrix->begin()) {
    Serial.println("[Display] Failed to initialize HUB75 Matrix!");
    return false;
  }

  _matrix->setBrightness8(_brightness);
  _matrix->setTextWrap(false);
  initColors();
  _matrix->clearScreen();

  return true;
}

void DisplayManager::initColors() {
  if (!_matrix)
    return;
  _colBlack = _matrix->color565(0, 0, 0);
  _colWhite = _matrix->color565(255, 255, 255);
  _colCyan = _matrix->color565(0, 255, 255);
  _colYellow = _matrix->color565(255, 255, 0);
  _colGreen = _matrix->color565(0, 255, 0);
  _colRed = _matrix->color565(255, 30, 30);
  _colOrange = _matrix->color565(255, 140, 0);
  _colBlue = _matrix->color565(40, 120, 255);
  _colPurple = _matrix->color565(200, 60, 255);
  _colDarkGray = _matrix->color565(40, 45, 55);
  _colAccentBg = _matrix->color565(15, 20, 30);
}

void DisplayManager::setBrightness(uint8_t brightness) {
  _brightness = brightness;
  if (_matrix) {
    _matrix->setBrightness8(_brightness);
  }
}

uint8_t DisplayManager::getBrightness() { return _brightness; }

void DisplayManager::showBootSplash(const char *statusText) {
  if (!_matrix)
    return;
  _matrix->clearScreen();

  // Draw decorative border
  _matrix->drawRect(0, 0, TOTAL_SCREEN_WIDTH, TOTAL_SCREEN_HEIGHT, _colCyan);
  _matrix->drawRect(2, 2, TOTAL_SCREEN_WIDTH - 4, TOTAL_SCREEN_HEIGHT - 4,
                    _colDarkGray);

#if TOTAL_SCREEN_WIDTH >= 128
  // Title
  _matrix->setTextSize(1);
  _matrix->setTextColor(_colYellow);
  _matrix->setCursor(22, 12);
  _matrix->print("ESP32 SMART BELL");

  _matrix->setTextColor(_colCyan);
  _matrix->setCursor(26, 24);
  _matrix->print("& SCHEDULE SYS");

  // Status
  _matrix->setTextColor(_colGreen);
  _matrix->setCursor(10, 44);
  _matrix->print(statusText);
#else
  _matrix->setTextSize(1);
  _matrix->setTextColor(_colYellow);
  _matrix->setCursor(17, 10);
  _matrix->print("SMART");
  _matrix->setCursor(20, 20);
  _matrix->print("BELL");

  _matrix->setTextColor(_colCyan);
  _matrix->setCursor(14, 30);
  _matrix->print("SYSTEM");

  // Status
  _matrix->setTextColor(_colGreen);
  _matrix->setCursor(6, 46);
  String st = String(statusText);
  if (st.length() > 9)
    st = st.substring(0, 9);
  _matrix->print(st);
#endif

  _matrix->flipDMABuffer();
}

void DisplayManager::drawGpsIcon(int x, int y, bool fix, int satellites) {
  if (fix) {
    // Satellite dish icon in green
    _matrix->drawPixel(x + 2, y, _colGreen);
    _matrix->drawPixel(x + 1, y + 1, _colGreen);
    _matrix->drawPixel(x + 3, y + 1, _colGreen);
    _matrix->drawLine(x, y + 2, x + 4, y + 2, _colGreen);
    _matrix->drawLine(x + 2, y + 2, x + 2, y + 5, _colGreen);
    _matrix->drawPixel(x + 1, y + 5, _colGreen);
    _matrix->drawPixel(x + 3, y + 5, _colGreen);
  } else {
    // Blinking amber/red dot when searching
    bool blink = (millis() / 500) % 2;
    _matrix->drawCircle(x + 2, y + 2, 2, blink ? _colOrange : _colDarkGray);
  }
}

void DisplayManager::renderTopSchoolBanner() {
  // Top banner background (y: 0 to 15)
  _matrix->fillRect(0, 0, TOTAL_SCREEN_WIDTH, 16, _colAccentBg);

  // School Name in Yellow, Size 2
  const char *schoolName = SCHOOL_NAME;
  int textPixelWidth =
      strlen(schoolName) * 12; // 12px per char in Adafruit GFX size 2

  // Smooth horizontal marquee scrolling
  if (millis() - _lastSchoolScrollTime > 35) {
    _schoolScrollX--;
    if (_schoolScrollX < -(textPixelWidth + 10)) {
      _schoolScrollX = TOTAL_SCREEN_WIDTH;
    }
    _lastSchoolScrollTime = millis();
  }

  _matrix->setTextSize(2);
  _matrix->setTextWrap(false);
  _matrix->setTextColor(_colYellow);
  _matrix->setCursor(_schoolScrollX, 0);
  _matrix->print(schoolName);
}

void DisplayManager::renderMidTime(const SystemDateTime &dt) {
  // 12-hour time calculation
  int hour12 = dt.hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  const char *ampm = (dt.hour >= 12) ? "PM" : "AM";

  char hBuf[4];
  char mBuf[4];
  snprintf(hBuf, sizeof(hBuf), "%02d", hour12);
  snprintf(mBuf, sizeof(mBuf), "%02d", dt.minute);

  _matrix->setTextWrap(false);

#if TOTAL_SCREEN_WIDTH >= 128
  // Digits in Size 4 (32px tall, y: 16 to 47) in Green
  _matrix->setTextSize(4);
  _matrix->setTextColor(_colGreen);

  // Hours (2 digits, 24px each -> 48px)
  _matrix->setCursor(0, 18);
  _matrix->print(hBuf);

  // Colon dots (two 4x4 blocks at y=23 and y=37) in Green
  _matrix->fillRect(47, 23, 4, 4, _colGreen);
  _matrix->fillRect(47, 37, 4, 4, _colGreen);

  // Minutes (2 digits, 24px each -> 48px)
  _matrix->setCursor(54, 18);
  _matrix->print(mBuf);

  // AM/PM in Size 2 (16px tall, y: 22 to 37) in Green
  _matrix->setTextSize(2);
  _matrix->setTextColor(_colGreen);
  _matrix->setCursor(104, 18);
  _matrix->print(ampm);

  // GPS Status Indicator at bottom-right of mid area (x=118, y=40)
  drawGpsIcon(TOTAL_SCREEN_WIDTH - 10, 40, gpsManager.hasFix(),
              gpsManager.getSatellites());
#else
  _matrix->setTextSize(2);
  _matrix->setTextColor(_colGreen);
  _matrix->setCursor(2, 20);
  _matrix->print(hBuf);
  _matrix->print(":");
  _matrix->print(mBuf);
  _matrix->setTextSize(1);
  _matrix->setTextColor(_colGreen);
  _matrix->setCursor(48, 20);
  _matrix->print(ampm);
  drawGpsIcon(54, 30, gpsManager.hasFix(), gpsManager.getSatellites());
#endif
}

void DisplayManager::renderBottomDateAndDay(const SystemDateTime &dt) {
  _matrix->setTextWrap(false);

  // Format: "SEP 16 2026 | MONDAY" (Size 2 marquee scroll across y: 48 to 63)
  const char *days[] = {"SUNDAY",   "MONDAY", "TUESDAY", "WEDNESDAY",
                        "THURSDAY", "FRIDAY", "SATURDAY"};
  const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  int dIdx = constrain(dt.dayOfWeek, 0, 6);
  int mIdx = constrain(dt.month - 1, 0, 11);

  char bottomBuf[32];
  snprintf(bottomBuf, sizeof(bottomBuf), "%s %02d %04d | %s", months[mIdx],
           dt.day, dt.year, days[dIdx]);
  String bottomText = bottomBuf;
  int textPixelWidth =
      bottomText.length() * 12; // 12px per char in Adafruit GFX size 2

  // Smooth horizontal marquee scrolling across the bottom line
  if (millis() - _lastSubjectScrollTime > 35) {
    _subjectScrollX--;
    if (_subjectScrollX < -(textPixelWidth + 16)) {
      _subjectScrollX = TOTAL_SCREEN_WIDTH;
    }
    _lastSubjectScrollTime = millis();
  }

  _matrix->setTextSize(2);
  _matrix->setTextColor(_colCyan);
  _matrix->setCursor(_subjectScrollX, 48);
  _matrix->print(bottomText);
}

void DisplayManager::renderRingingAlert() {
  // Dynamic animated flashing bell alert screen
  if (millis() - _lastRingAnimTime > 120) {
    _ringAnimFrame = (_ringAnimFrame + 1) % 4;
    _lastRingAnimTime = millis();
  }

  // Invert flashing border
  uint16_t borderCol = (_ringAnimFrame % 2 == 0) ? _colRed : _colYellow;
  _matrix->drawRect(0, 0, TOTAL_SCREEN_WIDTH, TOTAL_SCREEN_HEIGHT, borderCol);
  _matrix->drawRect(1, 1, TOTAL_SCREEN_WIDTH - 2, TOTAL_SCREEN_HEIGHT - 2,
                    borderCol);

  _matrix->setTextSize(1);
  _matrix->setTextColor((_ringAnimFrame % 2 == 0) ? _colWhite : _colYellow);

#if TOTAL_SCREEN_WIDTH >= 128
  _matrix->setCursor(14, 8);
  _matrix->print("*** BELL RINGING ***");

  int cx = 64;
  int cy = 28;
  // Bell body
  _matrix->fillTriangle(cx, cy - 8, cx - 10, cy + 6, cx + 10, cy + 6,
                        _colYellow);
  _matrix->fillCircle(cx, cy + 7, 3, _colOrange);

  // Sound wave arcs
  for (int r = 14; r <= 14 + (_ringAnimFrame * 4); r += 4) {
    _matrix->drawCircle(cx, cy, r, _colRed);
  }
#else
  _matrix->setCursor(8, 6);
  _matrix->print("** RINGING **");

  int cx = TOTAL_SCREEN_WIDTH / 2;
  int cy = 34;
  // Bell body
  _matrix->fillTriangle(cx, cy - 7, cx - 8, cy + 5, cx + 8, cy + 5, _colYellow);
  _matrix->fillCircle(cx, cy + 6, 2, _colOrange);

  // Sound wave arcs
  for (int r = 10; r <= 10 + (_ringAnimFrame * 3); r += 3) {
    _matrix->drawCircle(cx, cy, r, _colRed);
  }
#endif
}

void DisplayManager::update() {
  if (!_matrix)
    return;

  // Throttle rendering to ~30 FPS
  if (millis() - _lastFrameTime < 33)
    return;
  _lastFrameTime = millis();

  _matrix->clearScreen();

  // If relay is actively ringing, show special emergency / bell ring animation
  // overlay
  if (relayManager.isRinging()) {
    renderRingingAlert();
  } else {
    SystemDateTime dt = gpsManager.getCurrentDateTime();

    renderTopSchoolBanner();
    renderMidTime(dt);
    renderBottomDateAndDay(dt);

    // Divider lines between top/mid (y=15) and mid/bottom (y=47)
    _matrix->drawFastHLine(0, 15, TOTAL_SCREEN_WIDTH, _colWhite);
    _matrix->drawFastHLine(0, 47, TOTAL_SCREEN_WIDTH, _colWhite);
  }

  _matrix->flipDMABuffer();
}
