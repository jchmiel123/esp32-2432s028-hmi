# ESP32-2432S028 Firmware Analysis Tools

This directory contains Python scripts for analyzing the factory firmware extracted from the ESP32-2432S028 display board. These tools help identify TFT display drivers, initialization sequences, and pin configurations.

## Scripts

### 1. extract_partition.py
**Purpose:** Extract and analyze the partition table from ESP32 firmware backup

**Usage:**
```bash
python extract_partition.py
```

**Requirements:**
- `esp32_fullflash_4mb.bin` in the same directory

**Output:**
- Displays partition table information
- Extracts `factory_app0.bin` (factory application partition)

---

### 2. analyze_tft_config.py
**Purpose:** Comprehensive TFT display configuration analysis

**Features:**
- Searches for TFT driver strings (ILI9341, ST7789, etc.)
- Extracts TFT-related configuration strings
- Identifies initialization command bytes
- Lists known ESP32-2432S028 pin mappings

**Usage:**
```bash
python analyze_tft_config.py
```

**Requirements:**
- `factory_app0.bin` (run `extract_partition.py` first)

---

### 3. find_init_sequences.py
**Purpose:** Locate TFT initialization command sequences

**Features:**
- Finds key initialization commands with context
- Identifies Sleep Out (0x11), Pixel Format (0x3A), Memory Access (0x36), and Display On (0x29)
- Shows hex context around each command

**Usage:**
```bash
python find_init_sequences.py
```

**Requirements:**
- `factory_app0.bin` (run `extract_partition.py` first)

---

## Workflow

1. Place your firmware backup file (`esp32_fullflash_4mb.bin`) in this directory
2. Run extraction: `python extract_partition.py`
3. Analyze configuration: `python analyze_tft_config.py`
4. Find init sequences: `python find_init_sequences.py`

## Known ESP32-2432S028 Configuration

**Display:** ST7789 240x320 TFT
**SPI Pins:**
- GPIO 23: MOSI (SPI Data Out)
- GPIO 19: MISO (SPI Data In)
- GPIO 18: SCLK (SPI Clock)
- GPIO 15: CS (Chip Select)
- GPIO 2: DC (Data/Command)
- GPIO 4: RST (Reset)
- GPIO 21: BL (Backlight)

**Other:**
- GPIO 36: Button Input (INPUT_PULLUP)

## Notes

These tools are for educational and debugging purposes. They help understand how the factory firmware configures the display, which can be useful for custom firmware development.
