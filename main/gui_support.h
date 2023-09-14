#ifndef GUI_SUPPORT_H
#define GUI_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct selector_row_t
{
	lv_obj_t * container;
	lv_obj_t * left_button;
	lv_obj_t * left_button_label;
	lv_obj_t * label;
	lv_obj_t * right_button;
	lv_obj_t * right_button_label;

	uint8_t current_index;
} selector_row_t;

typedef struct  slider_row_t
{
	lv_obj_t * container;
	lv_obj_t * label;
	lv_obj_t * slider;

	bool inhibit_change_event;
} slider_row_t;

typedef struct  message_row_t
{
	lv_obj_t * container;
	lv_obj_t * label;
} message_row_t;

typedef struct diag_row_t
{
	lv_obj_t * container;
	lv_obj_t * label;
	lv_obj_t * value_label;
} diag_row_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif