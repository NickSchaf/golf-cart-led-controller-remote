// SPDX: MIT
// Copyright 2021 Brian Starkey <stark3y@gmail.com>
// Portions from lvgl example: https://github.com/lvgl/lv_port_esp32/blob/master/main/main.c
//
// 

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_log.h>

#include "i2c_manager.h"
#include "m5core2_axp192.h"

#include "lvgl.h"
#include "lvgl_helpers.h"

#include "serial_console.h"
#include "cmd_system.h"
#include "cmd_nvs.h"
#include "cmd_ble.h"
#include "cmd_axp192.h"

#include "example_ble_sec_gattc_demo.h"
#include "gui_support.h"

#include <string>
#include <vector>

#define LV_TICK_PERIOD_MS                  1
#define DISCONNECTED_POWEROFF_TIMEOUT_MS   40000
#define SCREEN_DIM_TIMEOUT_MS              20000
#define DIAG_UPDATE_PERIOD_MS              500

#define MILLIS() (unsigned long) (esp_timer_get_time() / 1000ULL)

std::vector<std::string> patternsList;
std::vector<std::string> colorsList;

lv_style_t container_style = {};
message_row_t message_row = {};
selector_row_t selector_pattern = {};
selector_row_t selector_color = {};
slider_row_t slider_brightness = {};
slider_row_t slider_speed = {};

diag_row_t diag_bat_power = {};
diag_row_t diag_bat_voltage = {};
diag_row_t diag_temp = {};
diag_row_t diag_ac_voltage = {};
diag_row_t diag_charge_current = {};
// lv_obj_t * battery_bar = NULL;

bool isConnected = false;
bool screenDimmed = false;
uint32_t disconnectTimestamp = DISCONNECTED_POWEROFF_TIMEOUT_MS;
uint32_t lastUiInteractionTimestamp = SCREEN_DIM_TIMEOUT_MS;

uint32_t lastDiagUpdateTimestamp = 3000;  // Give some settle time after startup before diagnostic updates start

// static void toggle_event_cb(lv_obj_t *toggle, lv_event_t event)
// {
// 	if(event == LV_EVENT_VALUE_CHANGED) {
// 		bool state = lv_switch_get_state(toggle);
// 		enum toggle_id *id = lv_obj_get_user_data(toggle);

// 		// Note: This is running in the GUI thread, so prolonged i2c
// 		// comms might cause some jank
// 		switch (*id) {
// 		case TOGGLE_LED:
// 			m5core2_led(state);
// 			break;
// 		case TOGGLE_VIB:
// 			m5core2_vibration(state);
// 			break;
// 		case TOGGLE_5V:
// 			m5core2_int_5v(state);
// 			break;
// 		}
// 	}
// }

extern "C" {
  void app_main();
}

static void gui_timer_tick(void *arg)
{
	// Unused
	(void) arg;

	lv_tick_inc(LV_TICK_PERIOD_MS);
}

void update_selector_label(selector_row_t * row, const char * text)
{
	if (row == NULL || row->label == NULL) return;

	lv_label_set_text(row->label, text);

	// Need to re-center the label for the new size
	lv_obj_align(row->label, NULL, LV_ALIGN_CENTER, 0, 0);
}

void update_message_row(const char * text)
{
	lv_label_set_text(message_row.label, text);
	lv_obj_align(message_row.label, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_hidden(message_row.container, false);
}

void set_diag_value_text(diag_row_t * row, const char * text)
{
	lv_label_set_text(row->value_label, text);
	lv_obj_align(row->value_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

void hide_message_row()
{
	lv_obj_set_hidden(message_row.container, true);
}

void dimScreen(bool dim)
{
	screenDimmed = dim;
    m5core2_set_rail_mv(LCD_BACKLIGHT, dim ? 2500 : 2800);
}

void powerOff()
{
    m5core2_axp_twiddle(AXP192_SHUTDOWN_BATTERY_CHGLED_CONTROL, 0b10000000, 0b10000000);
}

void btn_selector_event_cb(lv_obj_t * btn, lv_event_t event)

{
	// printf("btn_selector_event_cb: %u\n", event);

	switch (event)
	{
		case LV_EVENT_CLICKED:
			if (selector_color.container == btn->parent)
			{
				// printf("Adjust color - current: %u, size: %u, incr: %u\n", selector_color.current_index, colorsList.size(), ((btn == selector_color.left_button) ? -1 : 1));
				selector_color.current_index = (selector_color.current_index + colorsList.size() + ((btn == selector_color.left_button) ? -1 : 1)) % colorsList.size();
				update_selector_label(&selector_color, colorsList[selector_color.current_index].c_str());
				SetColorIdx(selector_color.current_index);
			}
			if (selector_pattern.container == btn->parent)
			{
				// printf("Adjust pattern - current: %u, size: %u, incr: %u\n", selector_pattern.current_index, patternsList.size(), ((btn == selector_pattern.left_button) ? -1 : 1));
				selector_pattern.current_index = (selector_pattern.current_index + patternsList.size() + ((btn == selector_pattern.left_button) ? -1 : 1)) % patternsList.size();
				update_selector_label(&selector_pattern, patternsList[selector_pattern.current_index].c_str());
				SetPatternIdx(selector_pattern.current_index);
			}
			break;
		default:
			break;
	}
}

void slider_event_cb(lv_obj_t * slider, lv_event_t event)
{
	// printf("slider_event_cb: %u\n", event);

	switch (event)
	{
		case LV_EVENT_RELEASED:
			if (slider_brightness.container == slider->parent)
			{
				SetBrightness(lv_slider_get_value(slider));
			}
			if (slider_speed.container == slider->parent)
			{
				SetSpeed(lv_slider_get_value(slider));
			}
		break;
		default:
		break;
	}
}

lv_obj_t * create_base_row_container(lv_coord_t height, lv_obj_t * parent)
{
	lv_obj_t * result = lv_cont_create(parent, NULL);
	lv_cont_set_layout(result, LV_LAYOUT_OFF);
	lv_obj_set_size(result, 320, height);
	lv_obj_set_click(result, false);
	lv_obj_add_style(result, LV_OBJ_PART_MAIN, &container_style);

	return result;
}

void create_selector_row(selector_row_t * row, lv_obj_t * parent)
{
	if (row == NULL) return;

	row->container = create_base_row_container(60, parent);

	row->left_button = lv_btn_create(row->container, NULL);              /*Add a button to the pattern container*/
	lv_obj_set_size(row->left_button, 70, 55);                           /*Set its size*/
	lv_obj_align(row->left_button, NULL, LV_ALIGN_IN_LEFT_MID, 0, 0);
	lv_obj_set_event_cb(row->left_button, btn_selector_event_cb);

	row->left_button_label = lv_label_create(row->left_button, NULL);    /*Add a label to the button*/
	lv_label_set_text(row->left_button_label, "<");                      /*Set the labels text*/
	lv_obj_align(row->left_button_label, NULL, LV_ALIGN_CENTER, 0, 0);

	row->label = lv_label_create(row->container, NULL);                  /*Add a pattern label*/
	lv_label_set_text(row->label, "Row");                                /*Set the labels text*/
	lv_obj_align(row->label, NULL, LV_ALIGN_CENTER, 0, 0);

	row->right_button = lv_btn_create(row->container, NULL);             /*Add a button to the current screen*/
	lv_obj_set_size(row->right_button, 70, 55);                          /*Set its size*/
	lv_obj_align(row->right_button, NULL, LV_ALIGN_IN_RIGHT_MID, 0, 0);
	lv_obj_set_event_cb(row->right_button, btn_selector_event_cb);

	row->right_button_label = lv_label_create(row->right_button, NULL);  /*Add a label to the button*/
	lv_label_set_text(row->right_button_label, ">");                     /*Set the labels text*/
	lv_obj_align(row->right_button_label, NULL, LV_ALIGN_CENTER, 0, 0);
}

void create_silder_row(slider_row_t * row, const char * text, lv_obj_t * parent)
{
	if (row == NULL) return;

	row->container = create_base_row_container(55, parent);

	row->label = lv_label_create(row->container, NULL);
	lv_label_set_text(row->label, text);
	lv_obj_align(row->label, NULL, LV_ALIGN_IN_LEFT_MID, 0, 0);

	row->slider = lv_slider_create(row->container, NULL);
	lv_slider_set_value(row->slider, 70, LV_ANIM_OFF);
	lv_slider_set_range(row->slider, 0, 255);
	lv_obj_set_size(row->slider, 200, 10);
	lv_obj_align(row->slider, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);

	lv_obj_set_event_cb(row->slider, slider_event_cb);
}

void create_diag_row(diag_row_t * row, const char * label_text, lv_obj_t * parent)
{
	if (row == NULL) return;

	row->container = create_base_row_container(30, parent);

	row->label = lv_label_create(row->container, NULL);
	lv_label_set_text(row->label, label_text);
	lv_obj_align(row->label, NULL, LV_ALIGN_IN_LEFT_MID, 0, 0);

	row->value_label = lv_label_create(row->container, NULL);
	lv_label_set_text(row->value_label, "");
	lv_obj_align(row->value_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

bool local_touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
// enum { LV_INDEV_STATE_REL = 0, LV_INDEV_STATE_PR };
	static lv_indev_state_t last_state = LV_INDEV_STATE_REL;
	bool result = touch_driver_read(drv, data);
	// printf("touch_driver: (%d,%d), btn=%u, state=%u\n", data->point.x, data->point.y, data->btn_id, data->state);

	if (data->state != last_state)
	{
		// printf("touch_driver: state changed: %u\n", data->state);
		last_state = data->state;

		// Note the last screen interaction time
		lastUiInteractionTimestamp = MILLIS();

		if (screenDimmed)
		{
			printf("Dimming screen.\n");
			dimScreen(false);
		}
	}

	return result;
}

static void check_timers()
{
	if ((!isConnected && (MILLIS() >= (disconnectTimestamp + DISCONNECTED_POWEROFF_TIMEOUT_MS))))
	{
		printf("Disconnected too long.  Powering off...\n");
		powerOff();
	}
	if ((!screenDimmed && (MILLIS() >= (lastUiInteractionTimestamp + SCREEN_DIM_TIMEOUT_MS))))
	{
		printf("Dimming screen.\n");
		dimScreen(true);
	}
	if (MILLIS() >= (lastDiagUpdateTimestamp + DIAG_UPDATE_PERIOD_MS))
	{
		char temp_str[128] = {};
		float temp;

		m5core2_axp_read(AXP192_BATTERY_POWER, &temp);
		snprintf(temp_str, sizeof(temp_str), "%.2f mA", temp);
		set_diag_value_text(&diag_bat_power, temp_str);

		m5core2_axp_read(AXP192_BATTERY_VOLTAGE, &temp);
		snprintf(temp_str, sizeof(temp_str), "%.2f V", temp);
		set_diag_value_text(&diag_bat_voltage, temp_str);

		m5core2_axp_read(AXP192_TEMP, &temp);
		snprintf(temp_str, sizeof(temp_str), "%.1f C", temp);
		set_diag_value_text(&diag_temp, temp_str);

		m5core2_axp_read(AXP192_ACIN_VOLTAGE, &temp);
		snprintf(temp_str, sizeof(temp_str), "%.1f V", temp);
		set_diag_value_text(&diag_ac_voltage, temp_str);

		m5core2_axp_read(AXP192_CHARGE_CURRENT, &temp);
		snprintf(temp_str, sizeof(temp_str), "%.1f mA", temp);
		set_diag_value_text(&diag_charge_current, temp_str);

		lastDiagUpdateTimestamp = MILLIS();
	}
}

static void gui_thread(void *pvParameter)
{
	(void) pvParameter;

	static lv_color_t bufs[2][DISP_BUF_SIZE];
	static lv_disp_buf_t disp_buf;
	uint32_t size_in_px = DISP_BUF_SIZE;

	// Set up the frame buffers
	lv_disp_buf_init(&disp_buf, &bufs[0], &bufs[1], size_in_px);

	// Set up the display driver
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.flush_cb = disp_driver_flush;
	disp_drv.buffer = &disp_buf;
	lv_disp_drv_register(&disp_drv);

	// Register the touch screen. All of the properties of it
	// are set via the build config
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	// indev_drv.read_cb = touch_driver_read;
	indev_drv.read_cb = local_touch_driver_read;
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	lv_indev_drv_register(&indev_drv);

	// Timer to drive the main lvgl tick
	const esp_timer_create_args_t periodic_timer_args = {
		.callback = &gui_timer_tick,
		.name = "periodic_gui"
	};
	esp_timer_handle_t periodic_timer;
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

	// Setup general styles
	lv_style_set_border_width(&container_style, LV_STATE_DEFAULT, 0);
	lv_style_set_pad_all(&container_style, LV_STATE_DEFAULT, 0);
	lv_style_set_pad_inner(&container_style, LV_STATE_DEFAULT, 0);
	lv_style_set_margin_all(&container_style, LV_STATE_DEFAULT, 0);

	// Full screen root container
#if 1
	lv_obj_t *page = lv_page_create(lv_scr_act(), NULL);
	lv_obj_set_size(page, 320, 240);
	lv_obj_add_style(page, LV_OBJ_PART_MAIN, &container_style);
	// lv_obj_add_style(page, LV_PAGE_PART_SCRL, &container_style);
	lv_page_set_scrlbar_mode(page, LV_SCRLBAR_MODE_DRAG);

	lv_obj_t *root = lv_cont_create(page, NULL);
#else
	lv_obj_t *root = lv_cont_create(lv_scr_act(), NULL);
#endif

	lv_obj_set_size(root, 320, 240);
	lv_cont_set_layout(root, LV_LAYOUT_COLUMN_MID);

	// Don't let the containers be clicked on
	lv_obj_set_click(root, false);

	lv_obj_add_style(root, LV_OBJ_PART_MAIN, &container_style);

	// Row for displaying messages
	message_row.container = lv_cont_create(root, NULL);
	lv_cont_set_layout(message_row.container, LV_LAYOUT_OFF);
	lv_obj_set_size(message_row.container, 320, 55);
	lv_obj_set_click(message_row.container, false);
	lv_obj_add_style(message_row.container, LV_OBJ_PART_MAIN, &container_style);

	message_row.label = lv_label_create(message_row.container, NULL);
	update_message_row("Connecting...");


	// Sliders for brightness and speed
	create_silder_row(&slider_brightness, "Brightness", root);
	lv_slider_set_range(slider_brightness.slider, 10, 250);
	create_silder_row(&slider_speed, "Speed", root);
	lv_slider_set_range(slider_speed.slider, 1, 100);


	// Selectors for indexed values
	create_selector_row(&selector_color, root);
	create_selector_row(&selector_pattern, root);

	// Diagnostic Output
	create_diag_row(&diag_temp, "Temperature:", root);
	create_diag_row(&diag_ac_voltage, "AC Voltage:", root);
	create_diag_row(&diag_charge_current, "Charge Current:", root);
	create_diag_row(&diag_bat_voltage, "Battery Voltage:", root);
	create_diag_row(&diag_bat_power, "Battery Power:", root);

	// // Battery level indicator
	// battery_bar = lv_bar_create(root, NULL);

	// Adjust the root coontainer to fit all the things added to it
	lv_cont_set_fit2(root, LV_FIT_NONE, LV_FIT_TIGHT);

	while (1) {
		vTaskDelay(10 / portTICK_PERIOD_MS);

		lv_task_handler();
		check_timers();
	}

	// Never returns
}

void patternChangedCb(uint8_t val)
{
	printf("%s - val: %u\n", __func__, val);
	if (val < patternsList.size())
	{
		selector_pattern.current_index = val;
		update_selector_label(&selector_pattern, patternsList[selector_pattern.current_index].c_str());
	}
}

void colorChangedCb(uint8_t val)
{
	printf("%s - val: %u\n", __func__, val);
	if (val < colorsList.size())
	{
		selector_color.current_index = val;
		update_selector_label(&selector_color, colorsList[selector_color.current_index].c_str());
	}
}

void brightnessChangedCb(uint8_t val)
{
	printf("%s - val: %u\n", __func__, val);
	lv_slider_set_value(slider_brightness.slider, val, LV_ANIM_OFF);
}

void speedChangedCb(uint8_t val)
{
	printf("%s - val: %u\n", __func__, val);
	lv_slider_set_value(slider_speed.slider, val, LV_ANIM_OFF);
}

void linesToList(const char * str, std::vector<std::string> * list)
{
	list->clear();

	// Simple method, but statically allocates too much memory to fit on ESP32:
	// std::istringstream sstream(str);
	// std::string tmp;
	// while( getline(sstream, tmp) ) list->push_back(tmp);

	// Substring method:
	const std::string strTemp(str);
	uint32_t pos = 0;
	uint32_t next = 0;
	while ((next = strTemp.find_first_of('\n', pos)) != std::string::npos)
	{
		list->push_back(strTemp.substr(pos, next - pos));
		pos = next + 1;
	}
	if (pos < strTemp.size())
	{
		list->push_back(strTemp.substr(pos, strTemp.size() - pos));
	}
}

void patternListCb(const char * str, int strlen)
{
	printf("%s\n", __func__);
	if (str != NULL && strlen > 0 && str[strlen - 1] == 0)
	{
		// printf("  str: %s\n", str);
	}
	else
	{
		// printf("  str: %p, strlen: %d\n", (const void*)str, strlen);
		return;
	}

	linesToList(str, &patternsList);
	BeginReadCurPattern();
}

void colorListCb(const char * str, int strlen)
{
	printf("%s\n", __func__);
	if (str != NULL && strlen > 0 && str[strlen - 1] == 0)
	{
		// printf("  str: %s\n", str);
	}
	else
	{
		// printf("  str: %p, strlen: %d\n", (const void*)str, strlen);
		return;
	}

	linesToList(str, &colorsList);
	BeginReadCurColor();
}

void connectChangeCb(bool connected)
{
	printf("%s - connected: %s\n", __func__, connected ? "TRUE" : "FALSE");
	if (connected)
	{
		BeginReadPatterns();
		BeginReadColors();

		BeginReadBrightness();
		BeginReadSpeed();

		hide_message_row();
	}
	else
	{
		update_message_row("Connecting...");
		disconnectTimestamp = MILLIS();
	}
	
	isConnected = connected;
}

void setupBle()
{
	SetPatternChangedCallback(patternChangedCb);
	SetColorChangedCallback(colorChangedCb);
	SetBrightnessChangedCallback(brightnessChangedCb);
	SetSpeedChangedCallback(speedChangedCb);
	SetPatternListCallback(patternListCb);
	SetColorListCallback(colorListCb);
	SetConnectChangedCallback(connectChangeCb);
	
	init_gatt_client();
}

void app_main(void)
{
	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	printf("silicon revision %d, ", chip_info.revision);

	printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	printf("Free heap: %d\n", esp_get_free_heap_size());
	
    init_serial_console();
    register_system();
    register_nvs();
    register_ble_cmds();
    register_axp192_cmds();

	m5core2_init();

	lvgl_i2c_locking(i2c_manager_locking());

	lv_init();
	lvgl_driver_init();

	setupBle();
	
	// Needs to be pinned to a core
	xTaskCreatePinnedToCore(gui_thread, "gui", 4096*2, NULL, 0, NULL, 1);

    // This should never return
    run_serial_console();
}
