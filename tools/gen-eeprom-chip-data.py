#!/usr/bin/env python3
import re
import sys
from pathlib import Path


def decode_c_string(s: str) -> str:
    return bytes(s, "utf-8").decode("unicode_escape")


def main() -> int:
    base = Path(__file__).resolve().parents[1]
    src = base / "main" / "messages_def.h"
    out = base / "eeprom-chip" / "src" / "eeprom_data.h"
    pattern = re.compile(r'X\([^,]+,\s*"((?:\\.|[^"\\])*)"\)')

    strings = []
    for line in src.read_text().splitlines():
        match = pattern.search(line)
        if match:
            strings.append(match.group(1))

    if not strings:
        print("No messages found in messages_def.h", file=sys.stderr)
        return 1

    data = bytearray()
    for s in strings:
        decoded = decode_c_string(s)
        data.extend(decoded.encode("utf-8"))
        data.append(0)

    per_line = 16
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        line = "  " + ", ".join(f"0x{b:02x}" for b in chunk)
        if i + per_line < len(data):
            line += ","
        lines.append(line)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "\n".join(
            [
                "#pragma once",
                "#include <stdint.h>",
                "",
                f"#define EEPROM_DATA_SIZE {len(data)}",
                f"static const uint8_t kEepromData[EEPROM_DATA_SIZE] = {{",
                *lines,
                "};",
                "",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
