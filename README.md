# ESP32-2432S028 TFT Display Project

ESP-IDF native project for the ESP32-2432S028 development board with 2.8" ILI9341 TFT display.

## Hardware Specifications

- **Board**: ESP32-2432S028
- **Display**: 2.8" ILI9341 TFT (320×240)
- **Touch**: Resistive touch controller (XPT2046)
- **Interface**: 4-wire SPI

## Confirmed Pinout

Pinout verified via logic analyzer and firmware dump:

| Signal | GPIO | Notes              |
|--------|------|--------------------|
| MOSI   | 23   | SPI data out       |
| SCLK   | 18   | SPI clock          |
| CS     | 15   | Chip select (low)  |
| D/C    | 2    | Data/Command       |
| RST    | 4    | Reset              |
| BL     | 21   | Backlight control  |
| MISO   | 19   | Optional/unused    |

## SPI Configuration

- **Mode**: 0 (CPOL=0, CPHA=0)
- **Speed**: 12 MHz (conservative start, can increase to 40 MHz)
- **CS**: Active low, manual control
- **D/C**: Separate pin (4-wire SPI)

## Project Structure

```
esp32-2432s028-espidf/
├── platformio.ini          # PlatformIO configuration
├── include/
│   └── TftDisplay.hpp      # TFT display class header
├── src/
│   ├── main.cpp            # Main application
│   └── TftDisplay.cpp      # TFT display implementation
└── README.md               # This file
```

## Building and Flashing

### Using PlatformIO

```bash
# Build
pio run -e esp32dev

# Flash
pio run -e esp32dev -t upload

# Monitor
pio device monitor -b 115200
```

### Using ESP-IDF Extension

1. Open project in VS Code
2. Click "ESP-IDF: Build Project"
3. Click "ESP-IDF: Flash Device"
4. Click "ESP-IDF: Monitor Device"

## TftDisplay Class API

### Initialization

```cpp
TftDisplay tft;
tft.begin();        // Initialize SPI and GPIO
tft.initMinimal();  // Minimal init sequence
// or
tft.initFull();     // Full ILI9341 init
```

### Basic Operations

```cpp
tft.reset();                    // Hardware reset
tft.backlight(true);            // Backlight on/off
tft.fillScreen(0xF800);         // Fill with red (RGB565)
tft.setWindow(0, 0, 319, 239);  // Set drawing area
```

### Low-Level Commands

```cpp
tft.sendCommand(0x11);          // Send command byte
tft.sendData(data, len);        // Send data bytes
tft.sendCommandData(0x2A, data, 4); // Command + data
```

## Current Test Program

The `main.cpp` demonstrates:
- SPI initialization
- Display initialization (minimal sequence)
- Color cycling test (fills screen with different colors)

## Next Steps

- [ ] Implement text rendering
- [ ] Add graphics primitives (lines, rectangles, circles)
- [ ] Integrate XPT2046 touch controller
- [ ] Add LVGL support for GUI
- [ ] Performance optimization (DMA, higher SPI clock)

## Notes

- This project uses ESP-IDF framework (not Arduino)
- All commands confirmed from logic analyzer captures
- Conservative 12 MHz SPI speed for stability (can increase)
- Manual CS control for better performance

## Differences from Arduino Version

Unlike the `esp32-2-8-tft` project (Arduino + TFT_eSPI), this uses:
- Native ESP-IDF APIs
- Manual SPI management
- No Arduino abstractions
- Direct hardware control
- Better performance potential

## Known Issues

None currently - fresh project starting point.

---
**Last Updated**: 2026-01-09
**Framework**: ESP-IDF 5.x
**Status**: Initial setup complete ✅
