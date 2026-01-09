# SPDX-FileCopyrightText: © 2025 ambience-earth
# SPDX-License-Identifier: MIT

SOURCES = src/main.c
TARGET  = dist/chip.wasm

CC = clang
CFLAGS = --target=wasm32-unknown-unknown -O2 -nostdlib -fno-builtin \
  -Wl,--import-memory -Wl,--export-table -Wl,--no-entry -Wl,--allow-undefined

.PHONY: all
all: $(TARGET) dist/chip.json

.PHONY: clean
clean:
		rm -rf dist

dist:
		mkdir -p dist

$(TARGET): dist $(SOURCES) src/wokwi-api.h src/eeprom_data.h
		$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

dist/chip.json: dist chip.json
		cp chip.json dist
