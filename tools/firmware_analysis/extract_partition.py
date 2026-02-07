#!/usr/bin/env python3
"""
ESP32 Firmware Partition Extractor
Extracts and analyzes partition table and factory app from ESP32 firmware backup
"""
import struct
from pathlib import Path


def decode_partition_table(data):
    """Parse ESP32 partition table entries"""
    entries = []
    for i in range(0, len(data), 32):
        entry_data = data[i : i + 32]

        # Check for end marker (all 0xFF)
        if entry_data[:2] == b"\xff\xff":
            break

        # Parse entry
        magic = struct.unpack("<H", entry_data[0:2])[0]
        if magic != 0x50AA:  # ESP32 partition magic
            continue

        typ = entry_data[2]
        subtype = entry_data[3]
        offset = struct.unpack("<I", entry_data[4:8])[0]
        size = struct.unpack("<I", entry_data[8:12])[0]
        label = entry_data[12:28].split(b"\x00", 1)[0].decode("ascii", "ignore")
        flags = struct.unpack("<I", entry_data[28:32])[0]

        # Decode type/subtype
        type_names = {0x00: "app", 0x01: "data"}
        subtype_names = {
            0x00: {0x00: "factory", 0x10: "ota_0", 0x11: "ota_1", 0x20: "test"},
            0x01: {
                0x00: "ota",
                0x01: "phy",
                0x02: "nvs",
                0x80: "esphttpd",
                0x81: "fat",
                0x82: "spiffs",
            },
        }

        type_str = type_names.get(typ, f"0x{typ:02X}")
        subtype_str = subtype_names.get(typ, {}).get(subtype, f"0x{subtype:02X}")

        entries.append(
            {
                "label": label,
                "type": type_str,
                "subtype": subtype_str,
                "offset": offset,
                "size": size,
                "flags": flags,
            }
        )

    return entries


def main(backup_file="esp32_fullflash_4mb.bin"):
    """Extract and display partition information"""
    backup_path = Path(backup_file)

    if not backup_path.exists():
        print(f"✗ Backup file not found: {backup_path}")
        return

    raw = backup_path.read_bytes()
    part_data = raw[0x8000 : 0x8000 + 0xC00]  # 3KB partition table at 0x8000

    print("=" * 70)
    print("ESP32 Firmware - Partition Table Analysis")
    print("=" * 70)

    entries = decode_partition_table(part_data)

    if not entries:
        print("\n✗ No valid partitions found")
        return

    print(f"\nFound {len(entries)} partition(s):\n")
    print(f"{'Name':<16} {'Type':<8} {'Subtype':<12} {'Offset':<12} {'Size':<12} Flags")
    print("-" * 70)

    for e in entries:
        size_kb = e["size"] // 1024
        print(
            f"{e['label']:<16} {e['type']:<8} {e['subtype']:<12} "
            f"0x{e['offset']:08X}  {size_kb:>6} KB    0x{e['flags']:08X}"
        )

    # Find factory app for extraction
    factory_app = next((e for e in entries if e["subtype"] == "factory"), None)
    if factory_app:
        print(f"\n✓ Factory app found at offset 0x{factory_app['offset']:08X}")
        print(f"  Size: {factory_app['size']//1024} KB")

        # Extract factory app
        app_data = raw[
            factory_app["offset"] : factory_app["offset"] + factory_app["size"]
        ]
        output_path = Path("factory_app0.bin")
        output_path.write_bytes(app_data)
        print(f"  Extracted to: {output_path.name}")


if __name__ == "__main__":
    main()
