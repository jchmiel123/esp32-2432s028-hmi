#!/usr/bin/env python3
"""
TFT Display Configuration Analyzer
Searches factory firmware for TFT driver info, initialization sequences, and pin configs
"""
import re
from pathlib import Path


def search_strings(data, keywords):
    """Extract and filter ASCII strings from binary data"""
    strings = re.findall(b"[\x20-\x7e]{4,}", data)
    relevant = []

    for s in strings:
        if any(kw.lower() in s.lower() for kw in keywords):
            try:
                relevant.append(s.decode("ascii"))
            except:
                pass

    return sorted(set(relevant))


def search_drivers(data):
    """Search for TFT driver strings"""
    drivers = [
        "ILI9341",
        "ILI9488",
        "ST7789",
        "ST7735",
        "ILI9163",
        "GC9A01",
        "ST7796",
        "HX8357D",
    ]

    found = {}
    for driver in drivers:
        pattern = driver.encode("ascii")
        matches = [m.start() for m in re.finditer(pattern, data, re.IGNORECASE)]
        if matches:
            found[driver] = len(matches)

    return found


def search_init_commands(data):
    """Search for common ILI9341-style initialization command bytes"""
    ili_commands = {
        0x11: "Sleep Out",
        0x29: "Display On",
        0x36: "Memory Access Control",
        0x3A: "Pixel Format Set",
        0xB1: "Frame Rate Control",
        0xB6: "Display Function Control",
        0xC0: "Power Control 1",
        0xC1: "Power Control 2",
        0xC5: "VCOM Control",
        0xE0: "Positive Gamma",
        0xE1: "Negative Gamma",
    }

    found_commands = []
    for cmd, name in ili_commands.items():
        count = data.count(bytes([cmd]))
        if count > 0:
            found_commands.append((cmd, name, count))

    return found_commands


def main(firmware_file="factory_app0.bin"):
    """Analyze factory firmware for TFT configuration"""
    firmware_path = Path(firmware_file)

    if not firmware_path.exists():
        print(f"✗ Firmware file not found: {firmware_path}")
        print("  Run extract_partition.py first to extract the factory app")
        return

    data = firmware_path.read_bytes()

    print("=" * 70)
    print("TFT Display Configuration Analysis")
    print("=" * 70)
    print(f"Analyzing: {firmware_path.name} ({len(data):,} bytes)\n")

    # Search for TFT drivers
    print("TFT Driver Strings:")
    print("-" * 70)
    drivers = search_drivers(data)
    if drivers:
        for driver, count in sorted(drivers.items()):
            print(f"  ✓ {driver}: {count} occurrence(s)")
    else:
        print("  No driver strings found")

    # Search for TFT-related strings
    print("\n" + "=" * 70)
    print("TFT Configuration Strings:")
    print("-" * 70)
    keywords = [
        b"TFT",
        b"LCD",
        b"SPI",
        b"MOSI",
        b"MISO",
        b"SCLK",
        b"CS",
        b"DC",
        b"RST",
        b"backlight",
        b"display",
        b"init",
        b"rotation",
    ]
    relevant_strings = search_strings(data, keywords)
    if relevant_strings:
        for s in relevant_strings[:20]:  # First 20 matches
            print(f"  • {s}")
        if len(relevant_strings) > 20:
            print(f"  ... and {len(relevant_strings) - 20} more")
    else:
        print("  No relevant strings found")

    # Search for initialization commands
    print("\n" + "=" * 70)
    print("Display Initialization Commands:")
    print("-" * 70)
    commands = search_init_commands(data)
    if commands:
        for cmd_byte, name, count in sorted(commands):
            print(f"  0x{cmd_byte:02X} ({name}): {count} occurrence(s)")
    else:
        print("  No init commands found")

    # Known ESP32-2432S028 pin configuration
    print("\n" + "=" * 70)
    print("Known ESP32-2432S028 Pin Configuration:")
    print("-" * 70)
    known_pins = {
        23: "MOSI (SPI Data)",
        19: "MISO (SPI Data)",
        18: "SCLK (SPI Clock)",
        15: "CS (Chip Select)",
        2: "DC (Data/Command)",
        4: "RST (Reset)",
        21: "BL (Backlight)",
        36: "Button Input",
    }

    for pin, func in sorted(known_pins.items()):
        print(f"  GPIO {pin:2d} = {func}")

    print("\n" + "=" * 70)
    print(f"✓ Analysis complete")


if __name__ == "__main__":
    main()
