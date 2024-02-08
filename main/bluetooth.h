#include "esp_bt.h"
#include "esp_hidd_prf_api.h"
#include "esp_gap_ble_api.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#define BLUETOOTH_TAG "Bluetooth"
#define HID_GENERIC_APPEREANCE 0x03c0
#define DEFAULT_DEVICE_NAME ""

typedef struct {
    const char* name;
    uint8_t* uuid_16bit;
} bluetooth_config_t;

typedef struct {
    bool sec_conn;
    bool is_inited;
    uint16_t hid_conn_id;
} bluetooth_t;


void start_bluetooth(bluetooth_config_t bluetooth_config);
void disable_bluetooth();
uint16_t get_bluetooth_hid_connection_id();
