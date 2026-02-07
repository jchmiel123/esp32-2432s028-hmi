/**
 * BrewForge HMI - Espresso Machine Touch Display
 * ESP32-2432S028 "Yellow Board" (2.8" ST7789 + XPT2046 Touch)
 *
 * Now powered by ForgeUI — shared UI library for CodeLab projects.
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

#include <ForgeUI.h>
#include <drivers/TFT_eSPI_Driver.h>

// BrewStatus must be defined before BrewScreen includes it
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

#include "CalibrationScreen.h"
#include "BrewScreen.h"

// ===================== HARDWARE PINS =====================

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

// ===================== OBJECTS =====================

TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Driver gfxDriver(tft);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
HardwareSerial PicoSerial(2);

// Theme + Screen Manager
ForgeTheme theme = forgeThemeDark(240, 320);
ScreenManager screenMgr;

// Screen indices
int brewScreenIdx = -1;
int calScreenIdx  = -1;

// Screens
BrewScreen *brewScreen = nullptr;
CalibrationScreen *calScreen = nullptr;

// ===================== TOUCH =====================

TouchCal touchCal;  // Defined in CalibrationScreen.h
bool touchWasPressed = false;
unsigned long lastTouchTime = 0;
#define TOUCH_DEBOUNCE_MS 200
#define TOUCH_PRESSURE_MIN 200

struct ScreenPoint { int16_t x, y; };

ScreenPoint mapTouch(int16_t rawX, int16_t rawY) {
    ScreenPoint s;
    s.x = map(rawX, touchCal.xMin, touchCal.xMax, 0, 239);
    s.y = map(rawY, touchCal.yMin, touchCal.yMax, 0, 319);
    s.x = constrain(s.x, 0, 239);
    s.y = constrain(s.y, 0, 319);
    return s;
}

// ===================== BREW STATUS =====================

BrewStatus brew;

// ===================== UART PARSING =====================

String uartBuffer = "";

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
                uartBuffer = "";
            }
        }
    }

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

// ===================== TOUCH HANDLING =====================

void handleTouch() {
    bool pressed = touch.touched();
    unsigned long now = millis();

    if (pressed && !touchWasPressed && (now - lastTouchTime > TOUCH_DEBOUNCE_MS)) {
        TS_Point p = touch.getPoint();

        if (p.z > TOUCH_PRESSURE_MIN) {
            ScreenPoint sp = mapTouch(p.x, p.y);
            Serial.printf("Touch at (%d,%d) raw(%d,%d) z=%d\n",
                          sp.x, sp.y, p.x, p.y, p.z);

            screenMgr.handleTouch(sp.x, sp.y);
            lastTouchTime = now;
        }
    }

    touchWasPressed = pressed;
}

// ===================== SETUP =====================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== BrewForge HMI (ForgeUI) ===");

    // Backlight off during init
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    // Display init
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    delay(120);

    // Backlight on
    digitalWrite(TFT_BL, HIGH);

    // Splash
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(15, 80);
    tft.print("BrewForge");
    tft.setTextSize(2);
    tft.setTextColor(0xFD20, TFT_BLACK);
    tft.setCursor(60, 120);
    tft.print("HMI v2.0");
    tft.setTextSize(1);
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.setCursor(30, 155);
    tft.print("Powered by ForgeUI");
    tft.setCursor(30, 170);
    tft.print("Initializing touch...");

    // Touch init - SEPARATE SPI bus
    SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin();
    touch.setRotation(0);
    Serial.println("Touch initialized (VSPI: CLK=25 MISO=39 MOSI=32 CS=33 IRQ=36)");

    tft.setCursor(30, 185);
    tft.print("Touch OK");

    // UART to Pico
    PicoSerial.begin(PICO_BAUD, SERIAL_8N1, PICO_RX, PICO_TX);
    Serial.println("Pico UART ready (RX=16 TX=17)");

    tft.setCursor(30, 200);
    tft.print("UART OK");

    delay(1500);

    // ========== CREATE SCREENS ==========

    brewScreen = new BrewScreen(gfxDriver, theme, brew);
    brewScreen->setCallbacks(
        []() { sendCmd('b'); },  // Brew
        []() { sendCmd('x'); },  // Stop
        []() { sendCmd('-'); },  // Temp down
        []() { sendCmd('+'); },  // Temp up
        []() { screenMgr.deferShowScreen(calScreenIdx); }  // Calibrate
    );
    brewScreen->setup();
    brewScreenIdx = screenMgr.addScreen(brewScreen);

    calScreen = new CalibrationScreen(gfxDriver, theme, touch, touchCal);
    calScreen->setOnComplete([]() {
        screenMgr.deferShowScreen(brewScreenIdx);
    });
    calScreen->setup();
    calScreenIdx = screenMgr.addScreen(calScreen);

    // Show brew screen
    screenMgr.showScreen(brewScreenIdx);

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

    // Process deferred screen switches
    screenMgr.processDeferredActions();

    // Periodically request status from Pico
    if (now - lastStatusRequest > STATUS_REQUEST_MS) {
        sendCmd('s');
        lastStatusRequest = now;
    }

    // Update + draw active screen
    if (now - lastScreenUpdate > SCREEN_UPDATE_MS) {
        screenMgr.update();
        screenMgr.draw();
        lastScreenUpdate = now;
    }

    delay(10);
}
