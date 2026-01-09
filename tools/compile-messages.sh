#!/bin/bash
set -e

python3 - <<'PY'
import re
import sys

def decode_c_string(s):
    return bytes(s, "utf-8").decode("unicode_escape")

path = "main/messages_def.h"
pat = re.compile(r'X\([^,]+,\s*"((?:\\.|[^"\\])*)"\)')
strings = []
with open(path, "r") as f:
    for line in f:
        m = pat.search(line)
        if m:
            strings.append(m.group(1))

if not strings:
    sys.exit("No messages found in messages_def.h")

offsets = []
cur = 0
for s in strings:
    decoded = decode_c_string(s)
    offsets.append(cur)
    cur += len(decoded) + 1

per_line = 16
lines = []
for i in range(0, len(offsets), per_line):
    chunk = offsets[i:i+per_line]
    line = "  " + ", ".join(str(x) for x in chunk)
    if i + per_line < len(offsets):
        line += ","
    lines.append(line)

out_lines = [
    '#include "messages.h"',
    '#include <avr/pgmspace.h>',
    '',
    f"const uint16_t kMsgDataSize = {cur};",
    'const uint16_t kMsgOffsets[MSG_COUNT] PROGMEM = {',
    *lines,
    '};',
    '',
]

with open("main/messages_offsets.cpp", "w") as f:
    f.write("\n".join(out_lines))
PY

python3 tools/gen-eeprom-chip-data.py

echo "Writing..."
cp main/messages_def.h tools/strings_writer/messages_def.h
cp main/LiquidCrystal_I2C.h tools/strings_writer/LiquidCrystal_I2C.h
cp main/LiquidCrystal_I2C.cpp tools/strings_writer/LiquidCrystal_I2C.cpp
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 -e tools/strings_writer

echo "Done!"
