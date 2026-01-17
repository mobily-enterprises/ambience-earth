#pragma once

#include "app_state.h"
#include "app_utils.h"

/*
 * update_number_label
 * Updates the on-screen label for a number selector using its binding settings.
 * Example:
 *   NumberBinding *b = alloc_binding();
 *   b->value = &g_sim.max_daily_ml;
 *   b->label = value_label;
 *   b->fmt = "%d";
 *   update_number_label(b);
 */
static void update_number_label(NumberBinding *binding) {
  if (!binding || !binding->label) return;
  if (binding->fmt) lv_label_set_text_fmt(binding->label, binding->fmt, *binding->value);
  else lv_label_set_text_fmt(binding->label, "%d", *binding->value);
}

/*
 * number_inc_event
 * LVGL event handler that increments a bound value and refreshes its label.
 * Example:
 *   lv_obj_add_event_cb(inc_btn, number_inc_event, LV_EVENT_CLICKED, binding);
 */
static void number_inc_event(lv_event_t *event) {
  NumberBinding *binding = static_cast<NumberBinding *>(lv_event_get_user_data(event));
  if (!binding || !binding->value) return;
  *binding->value = clamp_int(*binding->value + binding->step, binding->min, binding->max);
  update_number_label(binding);
}

/*
 * number_dec_event
 * LVGL event handler that decrements a bound value and refreshes its label.
 * Example:
 *   lv_obj_add_event_cb(dec_btn, number_dec_event, LV_EVENT_CLICKED, binding);
 */
static void number_dec_event(lv_event_t *event) {
  NumberBinding *binding = static_cast<NumberBinding *>(lv_event_get_user_data(event));
  if (!binding || !binding->value) return;
  *binding->value = clamp_int(*binding->value - binding->step, binding->min, binding->max);
  update_number_label(binding);
}

/*
 * create_screen_root
 * Creates a full-screen root object with the app's standard background, padding, and column layout.
 * Example:
 *   lv_obj_t *screen = create_screen_root();
 *   create_header(screen, "Settings", true, back_event);
 *   lv_scr_load(screen);
 */
static lv_obj_t *create_screen_root() {
  lv_obj_t *screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, kColorBg, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_white(), 0);
  lv_obj_set_style_pad_all(screen, 8, 0);
  lv_obj_set_style_pad_row(screen, 6, 0);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  return screen;
}

/*
 * create_header
 * Builds a standard header bar with a centered title and an optional Back button.
 * Example:
 *   lv_obj_t *screen = create_screen_root();
 *   create_header(screen, "Logs", true, back_event);
 */
static lv_obj_t *create_header(lv_obj_t *parent, const char *title, bool show_back, lv_event_cb_t back_cb) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, LV_PCT(100), 30);
  lv_obj_set_style_bg_color(header, kColorHeader, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 4, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_label = lv_label_create(header);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  if (show_back) {
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 54, 22);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(back_btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
  }

  return header;
}

/*
 * create_menu_list
 * Creates a vertical container sized for menu rows with standard spacing.
 * Example:
 *   lv_obj_t *list = create_menu_list(screen);
 *   add_menu_item(list, "Logs", nullptr, open_logs_event, nullptr, nullptr);
 */
static lv_obj_t *create_menu_list(lv_obj_t *parent) {
  lv_obj_t *list = lv_obj_create(parent);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(list, 1);
  return list;
}

/*
 * add_menu_item
 * Adds a clickable menu row with a left label and optional right value label.
 * Example:
 *   lv_obj_t *value = add_menu_item(list, "Max daily", "900ml", open_max_event, nullptr, nullptr);
 *   lv_label_set_text(value, "950ml");
 */
static lv_obj_t *add_menu_item(lv_obj_t *list, const char *label, const char *value,
                               lv_event_cb_t cb, void *user_data, lv_obj_t **out_label) {
  lv_obj_t *btn = lv_btn_create(list);
  lv_obj_set_width(btn, LV_PCT(100));
  lv_obj_set_height(btn, 36);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t *label_obj = lv_label_create(btn);
  lv_label_set_text(label_obj, label);
  lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 4, 0);

  if (out_label) *out_label = label_obj;

  if (value) {
    lv_obj_t *value_label = lv_label_create(btn);
    lv_label_set_text(value_label, value);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -4, 0);
    return value_label;
  }
  return nullptr;
}

/*
 * create_menu_grid
 * Creates a two-column wrapping container for menu tiles.
 * Example:
 *   lv_obj_t *grid = create_menu_grid(screen);
 *   add_menu_tile(grid, "Logs", open_logs_event, nullptr, nullptr);
 */
static lv_obj_t *create_menu_grid(lv_obj_t *parent) {
  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, 8, 0);
  lv_obj_set_style_pad_column(grid, 8, 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(grid, 1);
  return grid;
}

/*
 * add_menu_tile
 * Adds a half-width button tile to a menu grid.
 * Example:
 *   add_menu_tile(grid, "Settings", open_settings_menu_event, nullptr, nullptr);
 */
static lv_obj_t *add_menu_tile(lv_obj_t *grid, const char *label, lv_event_cb_t cb,
                               void *user_data, lv_obj_t **out_label) {
  lv_obj_t *btn = lv_btn_create(grid);
  lv_obj_set_width(btn, LV_PCT(48));
  lv_obj_set_height(btn, 50);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t *label_obj = lv_label_create(btn);
  lv_label_set_text(label_obj, label);
  lv_label_set_long_mode(label_obj, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_obj, LV_PCT(100));
  lv_obj_set_style_text_align(label_obj, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_obj);

  if (out_label) *out_label = label_obj;
  return btn;
}

/*
 * add_menu_spacer
 * Adds an invisible placeholder tile to keep grid rows aligned.
 * Example:
 *   if (count % 2 != 0) add_menu_spacer(grid);
 */
static void add_menu_spacer(lv_obj_t *grid) {
  lv_obj_t *spacer = lv_obj_create(grid);
  lv_obj_set_width(spacer, LV_PCT(48));
  lv_obj_set_height(spacer, 50);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
}

/*
 * create_number_selector
 * Builds a row with a label, "-" and "+" buttons, and a live value label bound to an int.
 * Example:
 *   create_number_selector(parent, "Max ml", &g_sim.max_daily_ml, 0, 2000, 10, "%d");
 */
static lv_obj_t *create_number_selector(lv_obj_t *parent, const char *label_text, int *value,
                                        int min_value, int max_value, int step, const char *fmt) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 30);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_gap(row, 2, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, 90);

  lv_obj_t *dec_btn = lv_btn_create(row);
  lv_obj_set_size(dec_btn, 26, 26);
  lv_obj_t *dec_label = lv_label_create(dec_btn);
  lv_label_set_text(dec_label, "-");
  lv_obj_center(dec_label);

  lv_obj_t *value_label = lv_label_create(row);
  lv_obj_set_width(value_label, 40);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *inc_btn = lv_btn_create(row);
  lv_obj_set_size(inc_btn, 26, 26);
  lv_obj_t *inc_label = lv_label_create(inc_btn);
  lv_label_set_text(inc_label, "+");
  lv_obj_center(inc_label);

  NumberBinding *binding = alloc_binding();
  if (binding) {
    binding->value = value;
    binding->min = min_value;
    binding->max = max_value;
    binding->step = step;
    binding->label = value_label;
    binding->fmt = fmt;
    update_number_label(binding);
    lv_obj_add_event_cb(inc_btn, number_inc_event, LV_EVENT_CLICKED, binding);
    lv_obj_add_event_cb(dec_btn, number_dec_event, LV_EVENT_CLICKED, binding);
  }

  return row;
}

/*
 * create_number_selector_vertical
 * Builds a label with the increment controls stacked underneath.
 * Example:
 *   create_number_selector_vertical(parent, "Hour", &g_time_date_edit.hour, 0, 23, 1, "%02d");
 */
static lv_obj_t *create_number_selector_vertical(lv_obj_t *parent, const char *label_text, int *value,
                                                 int min_value, int max_value, int step, const char *fmt) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_width(block, LV_PCT(100));
  lv_obj_set_height(block, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_set_style_pad_all(block, 0, 0);
  lv_obj_set_style_pad_row(block, 0, 0);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, label_text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);

  lv_obj_t *row = lv_obj_create(block);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 26);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dec_btn = lv_btn_create(row);
  lv_obj_set_size(dec_btn, 22, 22);
  lv_obj_t *dec_label = lv_label_create(dec_btn);
  lv_label_set_text(dec_label, "-");
  lv_obj_center(dec_label);

  lv_obj_t *value_label = lv_label_create(row);
  lv_obj_set_width(value_label, 34);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *inc_btn = lv_btn_create(row);
  lv_obj_set_size(inc_btn, 22, 22);
  lv_obj_t *inc_label = lv_label_create(inc_btn);
  lv_label_set_text(inc_label, "+");
  lv_obj_center(inc_label);

  NumberBinding *binding = alloc_binding();
  if (binding) {
    binding->value = value;
    binding->min = min_value;
    binding->max = max_value;
    binding->step = step;
    binding->label = value_label;
    binding->fmt = fmt;
    update_number_label(binding);
    lv_obj_add_event_cb(inc_btn, number_inc_event, LV_EVENT_CLICKED, binding);
    lv_obj_add_event_cb(dec_btn, number_dec_event, LV_EVENT_CLICKED, binding);
  }

  return block;
}

struct KeyboardInputRefs {
  lv_obj_t *text_area;
  lv_obj_t *keyboard;
  lv_obj_t *done_btn;
  lv_obj_t *cancel_btn;
};

/*
 * create_keyboard_input
 * Builds a compact text input row with Done/Cancel and a full-width keyboard below it.
 * Example:
 *   KeyboardInputRefs refs = create_keyboard_input(parent, "Slot 1",
 *                                                  wizard_name_done_event, wizard_name_cancel_event, nullptr);
 *   g_wizard_refs.text_area = refs.text_area;
 */
static KeyboardInputRefs create_keyboard_input(lv_obj_t *parent, const char *initial_text,
                                               lv_event_cb_t done_cb, lv_event_cb_t cancel_cb, void *user_data) {
  KeyboardInputRefs refs = {};

  lv_obj_t *input_col = lv_obj_create(parent);
  lv_obj_set_width(input_col, LV_PCT(100));
  lv_obj_set_style_bg_opa(input_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(input_col, 0, 0);
  lv_obj_set_style_pad_all(input_col, 0, 0);
  lv_obj_set_style_pad_row(input_col, 2, 0);
  lv_obj_set_flex_flow(input_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(input_col, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *text_area = lv_textarea_create(input_col);
  lv_textarea_set_one_line(text_area, true);
  lv_textarea_set_text(text_area, initial_text ? initial_text : "");
  lv_obj_set_height(text_area, 30);
  lv_obj_set_width(text_area, LV_PCT(100));
  refs.text_area = text_area;

  lv_obj_t *btn_row = lv_obj_create(input_col);
  lv_obj_set_width(btn_row, LV_PCT(100));
  lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_style_pad_gap(btn_row, 6, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *done_btn = lv_btn_create(btn_row);
  lv_obj_set_flex_grow(done_btn, 1);
  lv_obj_set_height(done_btn, 22);
  lv_obj_add_event_cb(done_btn, done_cb, LV_EVENT_CLICKED, user_data);
  lv_obj_t *done_label = lv_label_create(done_btn);
  lv_label_set_text(done_label, "Done");
  lv_obj_center(done_label);
  refs.done_btn = done_btn;

  lv_obj_t *cancel_btn = lv_btn_create(btn_row);
  lv_obj_set_flex_grow(cancel_btn, 1);
  lv_obj_set_height(cancel_btn, 22);
  lv_obj_add_event_cb(cancel_btn, cancel_cb, LV_EVENT_CLICKED, user_data);
  lv_obj_t *cancel_label = lv_label_create(cancel_btn);
  lv_label_set_text(cancel_label, "Cancel");
  lv_obj_center(cancel_label);
  refs.cancel_btn = cancel_btn;

  lv_obj_t *keyboard = lv_keyboard_create(parent);
  lv_obj_set_flex_grow(keyboard, 1);
  lv_keyboard_set_textarea(keyboard, text_area);
  refs.keyboard = keyboard;

  return refs;
}

/*
 * create_time_input_block
 * Creates a labeled block with hour and minute selectors.
 * Example:
 *   create_time_input_block(parent, "Lights on", &g_sim.lights_on_hour, &g_sim.lights_on_min);
 */
static lv_obj_t *create_time_input_block(lv_obj_t *parent, const char *title, int *hour, int *minute) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_width(block, LV_PCT(100));
  lv_obj_set_height(block, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_set_style_pad_all(block, 0, 0);
  lv_obj_set_style_pad_row(block, 1, 0);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, kColorMuted, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);

  lv_obj_t *row = lv_obj_create(block);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 4, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *hour_row = create_number_selector_vertical(row, "Hour", hour, 0, 23, 1, "%02d");
  lv_obj_set_width(hour_row, LV_PCT(30));
  lv_obj_t *min_row = create_number_selector_vertical(row, "Minute", minute, 0, 59, 5, "%02d");
  lv_obj_set_width(min_row, LV_PCT(30));

  return block;
}

/*
 * create_date_input_block
 * Creates a labeled block with month/day/year selectors.
 * Example:
 *   create_date_input_block(parent, "Set date", &g_time_date_edit.month, &g_time_date_edit.day,
 *                           &g_time_date_edit.year);
 */
static lv_obj_t *create_date_input_block(lv_obj_t *parent, const char *title, int *month, int *day, int *year) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_width(block, LV_PCT(100));
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_set_style_pad_all(block, 0, 0);
  lv_obj_set_style_pad_row(block, 4, 0);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, kColorMuted, 0);

  create_number_selector(block, "Month", month, 1, 12, 1, "%02d");
  create_number_selector(block, "Day", day, 1, 31, 1, "%02d");
  create_number_selector(block, "Year", year, 2024, 2035, 1, "%d");

  return block;
}

/*
 * prompt_close
 * Removes the active prompt overlay (if any) and clears its state.
 * Example:
 *   prompt_close();
 *   pop_screen();
 */
static void prompt_close() {
  if (g_prompt.overlay) {
    lv_obj_del(g_prompt.overlay);
    g_prompt.overlay = nullptr;
  }
}

/*
 * prompt_button_event
 * Handles a prompt button click by closing the prompt and invoking its handler.
 * Example:
 *   lv_obj_add_event_cb(btn, prompt_button_event, LV_EVENT_CLICKED,
 *                       reinterpret_cast<void *>(static_cast<intptr_t>(0)));
 */
static void prompt_button_event(lv_event_t *event) {
  int option_id = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  PromptHandler handler = g_prompt.handler;
  int context = g_prompt.context;
  prompt_close();
  if (handler) handler(option_id, context);
}

/*
 * show_prompt
 * Displays a modal dialog with a title, message, and up to a few option buttons.
 * Example:
 *   const char *opts[] = {"No", "Yes"};
 *   show_prompt("Reset", "Factory reset?", opts, 2, reset_factory_handler, 0);
 */
static void show_prompt(const char *title, const char *message, const char **options, int option_count,
                        PromptHandler handler, int context) {
  prompt_close();
  g_prompt.handler = handler;
  g_prompt.context = context;

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(overlay, kScreenWidth, kScreenHeight);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  g_prompt.overlay = overlay;

  lv_obj_t *box = lv_obj_create(overlay);
  lv_obj_set_size(box, 200, 160);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, kColorHeader, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 8, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(box, 6, 0);

  lv_obj_t *title_label = lv_label_create(box);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);

  lv_obj_t *message_label = lv_label_create(box);
  lv_label_set_text(message_label, message);
  lv_obj_set_style_text_color(message_label, kColorMuted, 0);

  lv_obj_t *button_row = lv_obj_create(box);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_height(button_row, 40);
  lv_obj_set_style_bg_opa(button_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(button_row, 0, 0);
  lv_obj_set_style_pad_all(button_row, 0, 0);
  lv_obj_set_style_pad_gap(button_row, 6, 0);
  lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < option_count; ++i) {
    lv_obj_t *btn = lv_btn_create(button_row);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_add_event_cb(btn, prompt_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, options[i]);
    lv_obj_center(label);
  }
}
