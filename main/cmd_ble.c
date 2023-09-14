#include <stdio.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_ble.h"
#include "esp_gap_ble_api.h"
#include "esp_bt.h"

static const char* TAG = "BLE_CMD";

void print_bonded_devices()
{
  int dev_num = esp_ble_get_bond_device_num();

  esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  esp_ble_get_bond_device_list(&dev_num, dev_list);
  printf("BLE Bonded devices: %d\n", dev_num);
  for (int i = 0; i < dev_num; i++)
  {
    printf("\t");
    for (int b = 0; b < sizeof(esp_bd_addr_t); b++)
      printf("%02X ", dev_list[i].bd_addr[b]);
    printf("\r\n");
  }

  free(dev_list);
}

void clear_bonded_devices()
{
  int device_list_size = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t *device_list = (esp_ble_bond_dev_t *) malloc(sizeof(esp_ble_bond_dev_t) * device_list_size);
  esp_err_t get_bonded_device_result = esp_ble_get_bond_device_list(&device_list_size, device_list);

  if (get_bonded_device_result == ESP_OK)
  {
    esp_err_t remove_bonded_device_result = ESP_OK;
    for (int i = 0; i < device_list_size; i++)
    {
      remove_bonded_device_result = esp_ble_remove_bond_device(device_list[i].bd_addr);
      if (remove_bonded_device_result != ESP_OK)
      {
        free(device_list);
        ESP_LOGE(TAG, "Error clearing bond list: %s", esp_err_to_name(remove_bonded_device_result));
      }
    }
    ESP_LOGI(TAG, "BLE bond list cleared");
  }

  free(device_list);
}

/** Arguments used by 'ble' command */
static struct
{
    struct arg_lit *clear_bonds;
    struct arg_end *end;
} ble_args;

static int ble_cmd_func(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &ble_args);
  if (nerrors != 0)
  {
      arg_print_errors(stderr, ble_args.end, argv[0]);
      return 1;
  }

  if (ble_args.clear_bonds-> count > 0)
  {
    clear_bonded_devices();
  }

  uint8_t mac[6] = {0};
  ESP_ERROR_CHECK_WITHOUT_ABORT( esp_read_mac( mac, ESP_MAC_BT ) );
  printf("BLE MAC: " MACSTR "\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  print_bonded_devices();
  return 0;
}

void register_ble_cmds(void)
{
   // BLE command
  ble_args.clear_bonds = arg_lit0("c", "clear", "Clear BLE bonds");
  ble_args.end = arg_end(2);

  const esp_console_cmd_t ble_cmd = {
    .command = "ble",
    .help = "BLE status and configuration",
    .hint = NULL,
    .func = &ble_cmd_func,
    .argtable = &ble_args
 };
  ESP_ERROR_CHECK( esp_console_cmd_register(&ble_cmd) );

}
