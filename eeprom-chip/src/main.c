// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha

#include "wokwi-api.h"
#include <stdint.h>
#include <stdbool.h>

#include "eeprom_data.h"

#define EEPROM_SIZE 4096u
#define EEPROM_MASK (EEPROM_SIZE - 1u)

typedef struct {
  uint16_t addr;
  uint8_t addr_stage;
} chip_state_t;

static uint8_t memory[EEPROM_SIZE];
static chip_state_t chip_state = {0, 0};

static void load_memory(void) {
  uint16_t i = 0;
  for (; i < EEPROM_DATA_SIZE && i < EEPROM_SIZE; ++i) {
    memory[i] = kEepromData[i];
  }
  for (; i < EEPROM_SIZE; ++i) {
    memory[i] = 0;
  }
}

static inline uint16_t wrap_addr(uint16_t addr) {
  return (uint16_t)(addr & EEPROM_MASK);
}

static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  (void)user_data;
  (void)address;
  if (!read) {
    chip_state.addr_stage = 0;
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  (void)user_data;
  uint16_t addr = wrap_addr(chip_state.addr);
  uint8_t value = memory[addr];
  chip_state.addr = wrap_addr(chip_state.addr + 1u);
  return value;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  (void)user_data;
  if (chip_state.addr_stage == 0) {
    chip_state.addr = (uint16_t)data << 8;
    chip_state.addr_stage = 1;
    return true;
  }
  if (chip_state.addr_stage == 1) {
    chip_state.addr |= data;
    chip_state.addr_stage = 2;
    return true;
  }
  uint16_t addr = wrap_addr(chip_state.addr);
  memory[addr] = data;
  chip_state.addr = wrap_addr(chip_state.addr + 1u);
  return true;
}

void chip_init(void) {
  load_memory();

  i2c_config_t i2c1 = {
    .address = 0x50,
    .scl = pin_init("SCL", INPUT_PULLUP),
    .sda = pin_init("SDA", INPUT_PULLUP),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = 0,
    .user_data = &chip_state,
  };

  i2c_init(&i2c1);
}
