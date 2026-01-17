#include "platform_display.h"

#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

static const uint8_t kTftSclk = 12;
static const uint8_t kTftMosi = 11;
static const uint8_t kTftMiso = 13;
static const uint8_t kTftCs = 15;
static const uint8_t kTftDc = 2;
static const uint8_t kTftRst = 4;
static const uint8_t kTftLed = 6;
static const uint8_t kTftRotation = 1;
#ifdef WOKWI_SIM
static const uint32_t kTftSpiHz = 80000000;
#else
static const uint32_t kTftSpiHz = 40000000;
#endif

static const uint8_t kTouchSda = 10;
static const uint8_t kTouchScl = 8;

static Adafruit_ILI9341 tft(kTftCs, kTftDc, kTftRst);
static Adafruit_FT6206 ctp;

LV_DRAW_BUF_DEFINE_STATIC(draw_buf, kScreenWidth, kBufferLines, LV_COLOR_FORMAT_RGB565);

static bool touchReady = false;

/*
 * disp_flush
 * LVGL display flush callback that pushes pixel data to the ILI9341.
 * Example:
 *   lv_display_set_flush_cb(disp, disp_flush);
 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels(reinterpret_cast<uint16_t *>(px_map), w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

/*
 * touch_read
 * LVGL input read callback that maps FT6206 touches into display coords.
 * Example:
 *   lv_indev_set_read_cb(indev, touch_read);
 */
static void touch_read(lv_indev_t *, lv_indev_data_t *data) {
  if (!touchReady || !ctp.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  TS_Point p = ctp.getPoint();
  int32_t px = map(p.x, 0, kTouchWidth, kTouchWidth, 0);
  int32_t py = map(p.y, 0, kTouchHeight, kTouchHeight, 0);
  int32_t x = 0;
  int32_t y = 0;

  switch (kTftRotation & 3) {
    case 1:
      x = kScreenWidth - 1 - py;
      y = px;
      break;
    case 2:
      x = kScreenWidth - 1 - px;
      y = kScreenHeight - 1 - py;
      break;
    case 3:
      x = py;
      y = kScreenHeight - 1 - px;
      break;
    default:
      x = px;
      y = py;
      break;
  }

  if (x < 0) x = 0;
  else if (x >= kScreenWidth) x = kScreenWidth - 1;

  if (y < 0) y = 0;
  else if (y >= kScreenHeight) y = kScreenHeight - 1;

  data->point.x = static_cast<int16_t>(x);
  data->point.y = static_cast<int16_t>(y);
  data->state = LV_INDEV_STATE_PRESSED;

  g_last_touch_ms = millis();
  g_screensaver_active = false;
}

/*
 * platform_display_init
 * Initializes SPI/I2C, TFT, touch, and the LVGL display/indev bindings.
 * Example:
 *   lv_display_t *disp = platform_display_init();
 */
lv_display_t *platform_display_init() {
  SPI.begin(kTftSclk, kTftMiso, kTftMosi, kTftCs);
  Wire.begin(kTouchSda, kTouchScl);

  pinMode(kTftLed, OUTPUT);
  digitalWrite(kTftLed, HIGH);

  tft.begin();
  tft.setRotation(kTftRotation);
  tft.setSPISpeed(kTftSpiHz);

  touchReady = ctp.begin(40);

  lv_init();
  lv_tick_set_cb(millis);

  lv_display_t *disp = lv_display_create(kScreenWidth, kScreenHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, disp_flush);
  LV_DRAW_BUF_INIT_STATIC(draw_buf);
  lv_display_set_draw_buffers(disp, &draw_buf, nullptr);
  lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(disp);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);
  lv_indev_set_display(indev, disp);

  return disp;
}
