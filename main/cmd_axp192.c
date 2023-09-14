#include <stdio.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_axp192.h"
#include "m5core2_axp192.h"

static const char* TAG = "AXP192_CMD";

/** Arguments used by 'axp192' command */
static struct
{
    struct arg_int *bl_mv;
    struct arg_lit *poweroff;
    struct arg_end *end;
} axp192_args;

void print_rail_info(const char * name, axp192_rail_t rail)
{
  bool enabled = false;
  uint16_t mv = 0;

  m5core2_get_rail_state(rail, &enabled);
  m5core2_get_rail_mv(rail, &mv);

  printf("Rail %s: %sabled, %umv\n", name, enabled ? "en" : "dis", mv);
}

static int axp192_cmd_func(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &axp192_args);
  if (nerrors != 0)
  {
    arg_print_errors(stderr, axp192_args.end, argv[0]);
    return 1;
  }

  if (axp192_args.bl_mv->count != 0)
  {
    if (axp192_args.bl_mv->ival[0] < 0 || axp192_args.bl_mv->ival[0] > 2800)
    {
      ESP_LOGE(__func__, "Invalid voltage. (range: 0..2800)");
      return 1;
    }
    m5core2_set_rail_mv(LCD_BACKLIGHT, (uint16_t)axp192_args.bl_mv->ival[0]);
  }

  if(axp192_args.poweroff->count != 0)
  {
    //void m5core2_power::power_off(void) { Write1Byte(0x32, Read8bit(0x32) | 0b10000000); }
    //m5core2_axp_twiddle(uint8_t reg, uint8_t affect, uint8_t value);
    m5core2_axp_twiddle(AXP192_SHUTDOWN_BATTERY_CHGLED_CONTROL, 0b10000000, 0b10000000);
  }

  print_rail_info("ESP_POWER", ESP_POWER);
  print_rail_info("LCD_BACKLIGHT", LCD_BACKLIGHT);
  print_rail_info("LOGIC_AND_SD", LOGIC_AND_SD);
  print_rail_info("VIBRATOR", VIBRATOR);

  float temp;
  m5core2_axp_read(AXP192_ACIN_VOLTAGE, &temp);
  printf("AXP192_ACIN_VOLTAGE: %f\n", temp);
  m5core2_axp_read(AXP192_ACIN_CURRENT, &temp);
  printf("AXP192_ACIN_CURRENT: %f\n", temp);

  m5core2_axp_read(AXP192_VBUS_VOLTAGE, &temp);
  printf("AXP192_VBUS_VOLTAGE: %f\n", temp);
  m5core2_axp_read(AXP192_VBUS_CURRENT, &temp);
  printf("AXP192_VBUS_CURRENT: %f\n", temp);

  m5core2_axp_read(AXP192_TEMP, &temp);
  printf("AXP192_TEMP: %f\n", temp);

  m5core2_axp_read(AXP192_BATTERY_POWER, &temp);
  printf("AXP192_BATTERY_POWER: %f\n", temp);

  m5core2_axp_read(AXP192_BATTERY_VOLTAGE, &temp);
  printf("AXP192_BATTERY_VOLTAGE: %f\n", temp);

  m5core2_axp_read(AXP192_CHARGE_CURRENT, &temp);
  printf("AXP192_CHARGE_CURRENT: %f\n", temp);

  m5core2_axp_read(AXP192_DISCHARGE_CURRENT, &temp);
  printf("AXP192_DISCHARGE_CURRENT: %f\n", temp);

  m5core2_axp_read(AXP192_APS_VOLTAGE, &temp);
  printf("AXP192_APS_VOLTAGE: %f\n", temp);

  return 0;
}

void register_axp192_cmds(void)
{
   // AXP192 command
  axp192_args.bl_mv = arg_int0("b", "backlight", "<mv>", "Backlight millivoltage (0-2800)" );
  axp192_args.poweroff = arg_lit0(NULL, "poweroff", "Power-off the device");
  axp192_args.end = arg_end(2);

  const esp_console_cmd_t axp192_cmd = {
    .command = "axp192",
    .help = "AXP192 status",
    .hint = NULL,
    .func = &axp192_cmd_func,
    .argtable = &axp192_args
  };
  ESP_ERROR_CHECK( esp_console_cmd_register(&axp192_cmd) );

}
