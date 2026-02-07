#!/usr/bin/env python3
"""
Advanced Initialization Sequence Finder
Locates and displays TFT display initialization command sequences in firmware
"""
from pathlib import Path


def find_command_sequences(data, cmd_byte, name, nearby_cmds):
    """Find instances of a command byte and show context"""
    matches = []

    for i in range(len(data)):
        if data[i] == cmd_byte:
            # Get surrounding context
            start = max(0, i - 10)
            end = min(len(data), i + 20)
            context_before = data[start:i]
            context_after = data[i + 1 : end]

            # Check if this looks like a command sequence
            # (surrounded by other known command bytes or small parameter values)
            if any(b in nearby_cmds or b < 10 for b in context_after[:5]):
                matches.append(
                    {
                        "offset": i,
                        "before": context_before[-10:],
                        "after": context_after[:15],
                    }
                )

    return matches


def main(firmware_file="factory_app0.bin"):
    """Find TFT initialization sequences in firmware"""
    firmware_path = Path(firmware_file)

    if not firmware_path.exists():
        print(f"✗ Firmware file not found: {firmware_path}")
        print("  Run extract_partition.py first")
        return

    data = firmware_path.read_bytes()

    print("=" * 70)
    print("Display Initialization Sequence Finder")
    print("=" * 70)
    print(f"Analyzing: {firmware_path.name}\n")

    # Key commands that indicate initialization sequences
    key_commands = {
        0x11: "Sleep Out (usually first command)",
        0x3A: "Pixel Format Set (0x55=RGB565, 0x66=RGB666)",
        0x36: "Memory Access Control (rotation/mirror)",
        0x29: "Display On (usually last command)",
    }

    nearby_cmds = set(
        [0x11, 0x29, 0x36, 0x3A, 0xB1, 0xB6, 0xC0, 0xC1, 0xC5, 0xE0, 0xE1]
    )

    for cmd_byte, description in key_commands.items():
        matches = find_command_sequences(data, cmd_byte, description, nearby_cmds)

        if matches:
            print(f"0x{cmd_byte:02X} - {description}")
            print("-" * 70)

            for match in matches[:5]:  # Show first 5 matches
                before_hex = " ".join(f"{b:02x}" for b in match["before"])
                after_hex = " ".join(f"{b:02x}" for b in match["after"])
                print(f"  @0x{match['offset']:08X}")
                print(f"    Before: {before_hex}")
                print(f"    After:  {after_hex}")

            if len(matches) > 5:
                print(f"  ... and {len(matches) - 5} more occurrences")
            print()

    print("=" * 70)
    print("✓ Sequence search complete")
    print("\nNote: Look for 0x11 followed by delays, then 0x3A with 0x55,")
    print("      ending with 0x29 for a typical init sequence.")


if __name__ == "__main__":
    main()
