# ESP32-2432S028 Pinout and Configuration

## Board Summary
- **MCU**: ESP32-D0WD-V3 rev 3.1 (Dual Core, 240MHz)
- **Flash**: 4MB (32Mbit)
- **Display**: 2.8" 320x240 TFT (ST7789/ILI9341 compatible)
- **Touch**: Resistive touchscreen controller
- **MAC Address**: d4:e9:f4:af:82:f0

## Display SPI Bus (HSPI)

| Function | GPIO | Notes |
|----------|------|-------|
| MOSI     | 13   | Display data out |
| MISO     | 12   | Display data in (read-back) |
| SCLK     | 14   | Display SPI clock |
| CS       | 15   | Display chip select (active low) |
| DC       | 2    | Data/Command select |
| RST      | 4    | Hardware Reset (active low) |
| BL       | 21   | Backlight (PWM capable, active high) |

## Touch SPI Bus (SEPARATE from display!)

The XPT2046 touch controller uses **completely different SPI pins** from the display.
This is the root cause of touch issues - it is NOT on the same bus as the display.

| Function | GPIO | Notes |
|----------|------|-------|
| MOSI     | 32   | Touch data out |
| MISO     | 39   | Touch data in (input-only GPIO) |
| CLK      | 25   | Touch SPI clock |
| CS       | 33   | Touch chip select |
| IRQ      | 36   | Touch interrupt (shared with physical button) |

**Setup:** Use default SPI (VSPI) pointed to touch pins. TFT_eSPI manages HSPI internally.
```cpp
SPI.begin(25, 39, 32, 33);  // CLK, MISO, MOSI, CS
XPT2046_Touchscreen touch(33, 36);
touch.begin();
```

## UART to BrewForge Pico (HMI)

| Function | GPIO | Notes |
|----------|------|-------|
| RX       | 16   | Serial2 RX (from Pico GP8/TX1) |
| TX       | 17   | Serial2 TX (to Pico GP9/RX1) |

**Wiring:** ESP32 GPIO16 <-> Pico GP8, ESP32 GPIO17 <-> Pico GP9, GND <-> GND

## Display Driver Configuration

The ESP32-2432S028 uses **ST7789** driver despite often being labeled as ILI9341.

### TFT_eSPI Configuration

```cpp
#define USER_SETUP_LOADED 1

// Driver selection
#define ST7789_DRIVER

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define CGRAM_OFFSET 1

// Pin definitions (HSPI bus)
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// SPI speeds
#define SPI_FREQUENCY       27000000  // 27MHz write
#define SPI_READ_FREQUENCY  16000000  // 16MHz read
```

## Hardware Initialization Timing

**Critical**: The display requires specific timing delays for proper initialization:

1. **Hardware Reset Sequence**:
   ```cpp
   pinMode(TFT_RST, OUTPUT);
   digitalWrite(TFT_RST, HIGH);
   delay(10);
   digitalWrite(TFT_RST, LOW);
   delay(20);
   digitalWrite(TFT_RST, HIGH);
   delay(150);  // CRITICAL: 150ms minimum after reset
   ```

2. **Display Initialization**:
   ```cpp
   tft.init();
   delay(120);  // CRITICAL: 120ms minimum after init (Sleep Out command)
   ```

3. **Backlight Control**:
   ```cpp
   // Keep backlight OFF during init
   pinMode(TFT_BL, OUTPUT);
   digitalWrite(TFT_BL, LOW);

   // ... perform reset and init ...

   // Turn backlight ON after init complete
   digitalWrite(TFT_BL, HIGH);
   ```

## Serial Configuration

- **Upload Speed**: 921600 baud
- **Monitor Speed**: 115200 baud
- **Port**: COM10 (Windows)

## Factory Firmware Analysis

The factory firmware was analyzed from backup at:
- **Partition Offset**: 0x10000 (app0)
- **Size**: 1,310,720 bytes (1280KB)
- **Framework**: Arduino + TFT_eSPI library
- **Build Path**: `C:\Users\Administrator\Desktop\JYC\demo\libraries\TFT_eSPI\`

### Partition Table

| Name    | Type  | Subtype | Offset     | Size    |
|---------|-------|---------|------------|---------|
| nvs     | data  | nvs     | 0x00009000 | 20 KB   |
| otadata | data  | ota     | 0x0000E000 | 8 KB    |
| app0    | app   | ota_0   | 0x00010000 | 1280 KB |
| app1    | app   | ota_1   | 0x00150000 | 1280 KB |
| spiffs  | data  | spiffs  | 0x00290000 | 1472 KB |

## Display Characteristics

- **Resolution**: 320x240 pixels
- **Color Depth**: RGB565 (16-bit)
- **Interface**: SPI
- **Orientation**: Landscape (rotation=1) for 320x240 wide
- **Default Rotation**: Portrait is 240x320

## Known Issues

1. **Driver Confusion**: Board is often labeled ILI9341 but uses ST7789
2. **Display SPI Pins**: Many references say GPIO 23/19/18 but actual working pins are 13/12/14 (HSPI)
3. **Touch on SEPARATE bus**: Touch XPT2046 is NOT on the display SPI bus - uses GPIO 32/39/25/33
4. **Color Order**: Must use TFT_BGR (not TFT_RGB) for correct colors
5. **Reset Timing**: Must wait 150ms after hardware reset
6. **Init Timing**: Must wait 120ms after tft.init() (Sleep Out command)
7. **Backlight Timing**: Turn ON backlight only after init complete
8. **GPIO 36 dual-use**: Touch IRQ and physical button share GPIO 36

## Resources

- **PlatformIO Configuration**: `platformio.ini`
- **Main Application**: `src/main.cpp`
- **Factory Backup**: `device_backup_20260106_030510/`
- **Analysis Scripts**:
  - `device_backup_20260106_030510/decode_partition.py`
  - `device_backup_20260106_030510/extract_and_analyze.py`
  - `device_backup_20260106_030510/detailed_tft_analysis.py`

## Additional Notes

- Flash encryption: **DISABLED** (FLASH_CRYPT_CNT=0)
- Chip revision: ESP32-D0WD-V3 (production)
- Crystal: 40MHz
- CPU Frequency: 240MHz
- PSRAM: Not present
