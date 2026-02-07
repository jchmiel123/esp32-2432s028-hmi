/**
 * BrewForge HMI - Espresso Machine Touch Display
 * ESP32-2432S028 "Yellow Board" (2.8" ST7789 + XPT2046 Touch)
 *
 * Communicates with BrewForge Pico 2W via UART:
 *   - Receives JSON status updates
 *   - Sends single-char commands (brew, stop, next, etc.)
 *
 * Hardware:
 *   Display (HSPI): MOSI=13, MISO=12, CLK=14, CS=15, DC=2, RST=4, BL=21
 *   Touch (VSPI):   MOSI=32, MISO=39, CLK=25, CS=33, IRQ=36
 *   UART to Pico:   RX=16, TX=17 (Serial2, 115200 baud)
 *
 * Wiring to Pico:
 *   ESP32 GPIO16 (RX) <-> Pico GP8 (TX1)
 *   ESP32 GPIO17 (TX) <-> Pico GP9 (RX1)
 *   GND              <-> GND
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ===================== HARDWARE PINS =====================

// Display (configured via platformio.ini TFT_eSPI defines)
#define TFT_BL 21

// Touch controller - SEPARATE SPI bus from display
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33
#define TOUCH_IRQ  36

// UART to BrewForge Pico
#define PICO_RX 16
#define PICO_TX 17
#define PICO_BAUD 115200

// ===================== DISPLAY CONSTANTS =====================

#define SCREEN_W 240
#define SCREEN_H 320

// Layout regions (Y coordinates)
#define TITLE_Y      0
#define TITLE_H      25
#define TEMP_Y       25
#define TEMP_H       70
#define STATE_Y      95
#define STATE_H      30
#define BUTTONS_Y    125
#define BUTTONS_H    55
#define FLOW_Y       180
#define FLOW_H       45
#define TEMPADJ_Y    225
#define TEMPADJ_H    45
#define NAV_Y        270
#define NAV_H        50

// Colors
#define C_BG         TFT_BLACK
#define C_TITLE_BG   0x18E3  // Dark blue-grey
#define C_TEXT       TFT_WHITE
#define C_ACCENT     TFT_CYAN
#define C_TEMP       0xFD20  // Orange
#define C_TARGET     TFT_CYAN
#define C_DIM        0x4208  // Grey
#define C_GREEN      0x07E0
#define C_RED        0xF800
#define C_YELLOW     TFT_YELLOW
#define C_BLUE       0x001F
#define C_DARKGREY   0x2104
#define C_BTN_BREW   0x07E0  // Green
#define C_BTN_STOP   0xF800  // Red

// ===================== OBJECTS =====================

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
HardwareSerial PicoSerial(2);

// ===================== TOUCH =====================

struct TouchCal {
    int16_t xMin = 300;
    int16_t xMax = 3800;
    int16_t yMin = 300;
    int16_t yMax = 3800;
    bool valid = false;
};

TouchCal touchCal;
bool touchWasPressed = false;
unsigned long lastTouchTime = 0;
#define TOUCH_DEBOUNCE_MS 200
#define TOUCH_PRESSURE_MIN 200

struct ScreenPoint {
    int16_t x, y;
};

ScreenPoint mapTouch(int16_t rawX, int16_t rawY) {
    ScreenPoint s;
    s.x = map(rawX, touchCal.xMin, touchCal.xMax, 0, SCREEN_W - 1);
    s.y = map(rawY, touchCal.yMin, touchCal.yMax, 0, SCREEN_H - 1);
    s.x = constrain(s.x, 0, SCREEN_W - 1);
    s.y = constrain(s.y, 0, SCREEN_H - 1);
    return s;
}

// ===================== UI BUTTONS =====================

typedef void (*ButtonAction)();

struct Button {
    int16_t x, y, w, h;
    const char* label;
    uint16_t color;
    uint16_t textColor;
    ButtonAction action;
    uint8_t textSize;
};

// Forward declarations for button actions
void cmdBrew();
void cmdStop();
void cmdTempDown();
void cmdTempUp();
void cmdSettings();
void runCalibration();

// Main screen buttons
Button mainButtons[] = {
    {5,   BUTTONS_Y,     112, 50, "BREW",   C_BTN_BREW, TFT_BLACK, cmdBrew,     3},
    {123, BUTTONS_Y,     112, 50, "STOP",   C_BTN_STOP, C_TEXT,    cmdStop,     3},
    {5,   TEMPADJ_Y,     55,  40, "-5",     C_DARKGREY, C_TEXT,    cmdTempDown, 2},
    {65,  TEMPADJ_Y,     55,  40, "+5",     C_DARKGREY, C_TEXT,    cmdTempUp,   2},
    {130, TEMPADJ_Y,     105, 40, "CAL",    C_DARKGREY, C_ACCENT,  runCalibration, 2},
};
const int NUM_MAIN_BUTTONS = sizeof(mainButtons) / sizeof(mainButtons[0]);

// ===================== BREW STATUS =====================

struct BrewStatus {
    float temp = 0;
    float tempF = 0;
    float target = 93.0;
    float flow = 0;
    float volume = 0;
    char state[16] = "IDLE";
    int step = 0;
    int stepElapsed = 0;
    int stepTime = 0;
    bool pump = false;
    bool boiler = false;
    bool solenoid = false;
    bool warmer = false;
    float tempRate = 0;
    bool connected = false;
    unsigned long lastUpdate = 0;
};

BrewStatus brew;

// ===================== UART PARSING =====================

String uartBuffer = "";

// Simple JSON value extractors (no ArduinoJson needed)
float jsonFloat(const String& json, const char* key) {
    String search = String("\"") + key + "\":";
    int idx = json.indexOf(search);
    if (idx == -1) return 0;
    idx += search.length();
    int end = idx;
    while (end < (int)json.length() &&
           (isDigit(json[end]) || json[end] == '.' || json[end] == '-'))
        end++;
    return json.substring(idx, end).toFloat();
}

int jsonInt(const String& json, const char* key) {
    return (int)jsonFloat(json, key);
}

bool jsonBool(const String& json, const char* key) {
    String search = String("\"") + key + "\":";
    int idx = json.indexOf(search);
    if (idx == -1) return false;
    idx += search.length();
    return json.substring(idx, idx + 4) == "true";
}

void jsonString(const String& json, const char* key, char* out, int maxLen) {
    String search = String("\"") + key + "\":\"";
    int idx = json.indexOf(search);
    if (idx == -1) { out[0] = '\0'; return; }
    idx += search.length();
    int end = json.indexOf("\"", idx);
    if (end == -1) { out[0] = '\0'; return; }
    String val = json.substring(idx, end);
    val.toCharArray(out, maxLen);
}

void parseBrewJson(const String& json) {
    brew.temp       = jsonFloat(json, "temp");
    brew.tempF      = jsonFloat(json, "tempF");
    brew.target     = jsonFloat(json, "target");
    brew.flow       = jsonFloat(json, "flow");
    brew.volume     = jsonFloat(json, "volume");
    brew.step       = jsonInt(json, "step");
    brew.stepElapsed = jsonInt(json, "stepElapsed");
    brew.stepTime   = jsonInt(json, "stepTime");
    brew.pump       = jsonBool(json, "pump");
    brew.boiler     = jsonBool(json, "boiler");
    brew.solenoid   = jsonBool(json, "solenoid");
    brew.warmer     = jsonBool(json, "warmer");
    brew.tempRate   = jsonFloat(json, "tempRate");
    jsonString(json, "state", brew.state, 16);
    brew.connected  = true;
    brew.lastUpdate = millis();
}

void updateUART() {
    while (PicoSerial.available()) {
        char c = PicoSerial.read();
        if (c == '\n') {
            uartBuffer.trim();
            if (uartBuffer.startsWith("{") && uartBuffer.endsWith("}")) {
                parseBrewJson(uartBuffer);
            }
            uartBuffer = "";
        } else if (c >= 32 && c < 127) {
            uartBuffer += c;
            if (uartBuffer.length() > 1024) {
                uartBuffer = "";  // Overflow protection
            }
        }
    }

    // Connection timeout
    if (brew.connected && (millis() - brew.lastUpdate > 3000)) {
        brew.connected = false;
    }
}

// ===================== COMMANDS TO PICO =====================

void sendCmd(char cmd) {
    PicoSerial.write(cmd);
    PicoSerial.write('\n');
    Serial.printf("[HMI->Pico] %c\n", cmd);
}

void cmdBrew()     { sendCmd('b'); }
void cmdStop()     { sendCmd('x'); }
void cmdTempDown() { sendCmd('-'); }
void cmdTempUp()   { sendCmd('+'); }
void cmdNext()     { sendCmd('n'); }
void cmdPrev()     { sendCmd('v'); }
void cmdSettings() { /* TODO: settings screen */ }

// ===================== DRAWING FUNCTIONS =====================

void drawButton(const Button& btn, bool pressed = false) {
    uint16_t bg = pressed ? C_TEXT : btn.color;
    uint16_t fg = pressed ? TFT_BLACK : btn.textColor;

    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 6, bg);
    tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 6, fg);

    tft.setTextSize(btn.textSize);
    tft.setTextColor(fg, bg);

    // Center text
    int charW = btn.textSize * 6;
    int charH = btn.textSize * 8;
    int textW = strlen(btn.label) * charW;
    int tx = btn.x + (btn.w - textW) / 2;
    int ty = btn.y + (btn.h - charH) / 2;
    tft.setCursor(tx, ty);
    tft.print(btn.label);
}

void drawAllButtons() {
    for (int i = 0; i < NUM_MAIN_BUTTONS; i++) {
        drawButton(mainButtons[i]);
    }
}

void drawTitleBar() {
    tft.fillRect(0, TITLE_Y, SCREEN_W, TITLE_H, C_TITLE_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_ACCENT, C_TITLE_BG);
    tft.setCursor(5, 5);
    tft.print("BrewForge");

    // Connection indicator
    uint16_t dotColor = brew.connected ? C_GREEN : C_RED;
    tft.fillCircle(SCREEN_W - 15, TITLE_H / 2, 5, dotColor);
}

void drawTemperature() {
    // Clear temp region
    tft.fillRect(0, TEMP_Y, SCREEN_W, TEMP_H, C_BG);

    // Large temperature
    tft.setTextSize(4);
    tft.setTextColor(C_TEMP, C_BG);
    char tempStr[16];
    sprintf(tempStr, "%.1fC", brew.temp);
    int textW = strlen(tempStr) * 24;
    tft.setCursor((SCREEN_W - textW) / 2, TEMP_Y + 2);
    tft.print(tempStr);

    // Target temp
    tft.setTextSize(1);
    tft.setTextColor(C_TARGET, C_BG);
    sprintf(tempStr, "Target: %.0fC", brew.target);
    textW = strlen(tempStr) * 6;
    tft.setCursor((SCREEN_W - textW) / 2, TEMP_Y + 38);
    tft.print(tempStr);

    // Rate of change
    if (brew.step >= 1 && brew.step <= 7 && brew.tempRate != 0) {
        tft.setTextColor(C_DIM, C_BG);
        char rateStr[16];
        sprintf(rateStr, "%+.1f/s", brew.tempRate);
        tft.setCursor((SCREEN_W - strlen(rateStr) * 6) / 2, TEMP_Y + 50);
        tft.print(rateStr);
    }

    // Temperature bar
    int barX = 20;
    int barW = SCREEN_W - 40;
    int barY = TEMP_Y + 62;
    int barH = 6;
    tft.drawRect(barX, barY, barW, barH, C_DIM);

    float ratio = (brew.target > 0) ? (brew.temp / brew.target) : 0;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < 0) ratio = 0;
    int fillW = (int)(barW * ratio);

    uint16_t barColor = C_GREEN;
    if (brew.temp < brew.target - 5) barColor = C_RED;
    else if (brew.temp < brew.target - 2) barColor = C_YELLOW;

    if (fillW > 0)
        tft.fillRect(barX + 1, barY + 1, fillW - 1, barH - 2, barColor);
}

void drawBrewState() {
    tft.fillRect(0, STATE_Y, SCREEN_W, STATE_H, C_BG);

    // State badge
    tft.setTextSize(2);
    uint16_t stateColor = C_DIM;
    if (strcmp(brew.state, "IDLE") == 0) stateColor = C_GREEN;
    else if (strcmp(brew.state, "BREW") == 0) stateColor = C_TEMP;
    else if (strcmp(brew.state, "PREHEAT") == 0) stateColor = C_YELLOW;
    else if (strcmp(brew.state, "DONE") == 0) stateColor = C_GREEN;
    else stateColor = C_ACCENT;

    tft.setTextColor(stateColor, C_BG);
    tft.setCursor(5, STATE_Y + 2);
    tft.printf("[%d]%s", brew.step, brew.state);

    // Step timer
    if (brew.stepTime > 0) {
        tft.setTextSize(2);
        tft.setTextColor(C_TEXT, C_BG);
        char timeStr[12];
        sprintf(timeStr, "%d/%ds", brew.stepElapsed, brew.stepTime);
        int tw = strlen(timeStr) * 12;
        tft.setCursor(SCREEN_W - tw - 5, STATE_Y + 2);
        tft.print(timeStr);
    }

    // Relay indicators (bottom of state area)
    int dotY = STATE_Y + 22;
    int dotX = SCREEN_W - 70;
    int dotR = 4;
    int dotSpace = 14;

    // P B S W
    tft.fillCircle(dotX,              dotY, dotR, brew.pump     ? C_GREEN : C_DARKGREY);
    tft.fillCircle(dotX + dotSpace,   dotY, dotR, brew.boiler   ? C_RED   : C_DARKGREY);
    tft.fillCircle(dotX + dotSpace*2, dotY, dotR, brew.solenoid ? C_BLUE  : C_DARKGREY);
    tft.fillCircle(dotX + dotSpace*3, dotY, dotR, brew.warmer   ? C_YELLOW: C_DARKGREY);

    // Labels
    tft.setTextSize(1);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(dotX - 2,              dotY + 7);  tft.print("P");
    tft.setCursor(dotX + dotSpace - 2,   dotY + 7);  tft.print("B");
    tft.setCursor(dotX + dotSpace*2 - 2, dotY + 7);  tft.print("S");
    tft.setCursor(dotX + dotSpace*3 - 2, dotY + 7);  tft.print("W");
}

void drawFlowInfo() {
    tft.fillRect(0, FLOW_Y, SCREEN_W, FLOW_H, C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_ACCENT, C_BG);
    tft.setCursor(5, FLOW_Y + 2);
    tft.printf("Flow %.1f mL/s", brew.flow);
    tft.setCursor(5, FLOW_Y + 22);
    tft.printf("Vol  %.1f mL", brew.volume);
}

void drawFullScreen() {
    tft.fillScreen(C_BG);
    drawTitleBar();
    drawTemperature();
    drawBrewState();
    drawAllButtons();
    drawFlowInfo();
}

// ===================== TOUCH CALIBRATION =====================

void runCalibration() {
    struct CalPoint { int16_t sx, sy, tx, ty; };
    CalPoint pts[4] = {
        {20,  20,  0, 0},    // Top-left
        {220, 20,  0, 0},    // Top-right
        {220, 300, 0, 0},    // Bottom-right
        {20,  300, 0, 0}     // Bottom-left
    };

    for (int i = 0; i < 4; i++) {
        tft.fillScreen(C_BG);
        tft.setTextSize(2);
        tft.setTextColor(C_TEXT, C_BG);
        tft.setCursor(30, 140);
        tft.printf("Touch point %d/4", i + 1);

        // Draw crosshair
        tft.drawCircle(pts[i].sx, pts[i].sy, 10, C_RED);
        tft.drawCircle(pts[i].sx, pts[i].sy, 3, C_RED);
        tft.drawFastHLine(pts[i].sx - 15, pts[i].sy, 30, C_RED);
        tft.drawFastVLine(pts[i].sx, pts[i].sy - 15, 30, C_RED);

        // Wait for touch
        while (!touch.touched()) delay(10);
        delay(50);  // Settle
        TS_Point p = touch.getPoint();
        pts[i].tx = p.x;
        pts[i].ty = p.y;

        Serial.printf("Cal[%d] screen(%d,%d) raw(%d,%d) z=%d\n",
                      i, pts[i].sx, pts[i].sy, p.x, p.y, p.z);

        // Wait for release
        while (touch.touched()) delay(10);
        delay(300);
    }

    // Compute calibration
    touchCal.xMin = (pts[0].tx + pts[3].tx) / 2;
    touchCal.xMax = (pts[1].tx + pts[2].tx) / 2;
    touchCal.yMin = (pts[0].ty + pts[1].ty) / 2;
    touchCal.yMax = (pts[2].ty + pts[3].ty) / 2;
    touchCal.valid = true;

    Serial.printf("Calibration: X(%d->%d) Y(%d->%d)\n",
                  touchCal.xMin, touchCal.xMax,
                  touchCal.yMin, touchCal.yMax);

    // Show result
    tft.fillScreen(C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setCursor(20, 100);
    tft.print("Calibration done!");
    tft.setTextSize(1);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(20, 130);
    tft.printf("X: %d -> %d", touchCal.xMin, touchCal.xMax);
    tft.setCursor(20, 145);
    tft.printf("Y: %d -> %d", touchCal.yMin, touchCal.yMax);
    tft.setCursor(20, 170);
    tft.print("Touch to continue...");

    while (!touch.touched()) delay(10);
    while (touch.touched()) delay(10);

    drawFullScreen();
}

// ===================== TOUCH HANDLING =====================

Button* hitTest(ScreenPoint pos) {
    for (int i = 0; i < NUM_MAIN_BUTTONS; i++) {
        Button& b = mainButtons[i];
        if (pos.x >= b.x && pos.x < b.x + b.w &&
            pos.y >= b.y && pos.y < b.y + b.h) {
            return &mainButtons[i];
        }
    }
    return nullptr;
}

void handleTouch() {
    bool pressed = touch.touched();
    unsigned long now = millis();

    if (pressed && !touchWasPressed && (now - lastTouchTime > TOUCH_DEBOUNCE_MS)) {
        TS_Point p = touch.getPoint();

        if (p.z > TOUCH_PRESSURE_MIN) {
            ScreenPoint sp = mapTouch(p.x, p.y);
            Serial.printf("Touch at (%d,%d) raw(%d,%d) z=%d\n",
                          sp.x, sp.y, p.x, p.y, p.z);

            Button* btn = hitTest(sp);
            if (btn && btn->action) {
                // Visual feedback
                drawButton(*btn, true);
                delay(100);
                drawButton(*btn, false);
                btn->action();
            }

            lastTouchTime = now;
        }
    }

    touchWasPressed = pressed;
}

// ===================== SETUP =====================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== BrewForge HMI ===");

    // Backlight off during init
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    // Display init (uses HSPI pins from platformio.ini)
    tft.init();
    tft.setRotation(0);  // Portrait 240x320
    tft.fillScreen(C_BG);
    delay(120);

    // Backlight on
    digitalWrite(TFT_BL, HIGH);

    // Splash
    tft.setTextSize(3);
    tft.setTextColor(C_ACCENT, C_BG);
    tft.setCursor(15, 80);
    tft.print("BrewForge");
    tft.setTextSize(2);
    tft.setTextColor(C_TEMP, C_BG);
    tft.setCursor(60, 120);
    tft.print("HMI v1.0");
    tft.setTextSize(1);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(30, 160);
    tft.print("Initializing touch...");

    // Touch init - SEPARATE SPI bus from display
    // XPT2046 library uses the default SPI object.
    // TFT_eSPI manages its own HSPI internally, so we can safely
    // point the default SPI (VSPI) to the touch controller pins.
    SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin();
    touch.setRotation(0);

    Serial.println("Touch initialized (VSPI: CLK=25 MISO=39 MOSI=32 CS=33 IRQ=36)");

    // Test touch quickly
    tft.setCursor(30, 175);
    tft.print("Touch OK");

    // UART to Pico
    PicoSerial.begin(PICO_BAUD, SERIAL_8N1, PICO_RX, PICO_TX);
    Serial.println("Pico UART ready (RX=16 TX=17)");

    tft.setCursor(30, 190);
    tft.print("UART OK");

    delay(1500);

    // Draw main UI
    drawFullScreen();

    Serial.println("HMI ready. Waiting for Pico...");
}

// ===================== MAIN LOOP =====================

unsigned long lastScreenUpdate = 0;
unsigned long lastStatusRequest = 0;
#define SCREEN_UPDATE_MS 250
#define STATUS_REQUEST_MS 500

void loop() {
    unsigned long now = millis();

    // Read UART data from Pico
    updateUART();

    // Handle touch input
    handleTouch();

    // Periodically request status from Pico
    if (now - lastStatusRequest > STATUS_REQUEST_MS) {
        sendCmd('s');
        lastStatusRequest = now;
    }

    // Update display
    if (now - lastScreenUpdate > SCREEN_UPDATE_MS) {
        drawTitleBar();
        drawTemperature();
        drawBrewState();
        drawFlowInfo();
        lastScreenUpdate = now;
    }

    delay(10);
}
