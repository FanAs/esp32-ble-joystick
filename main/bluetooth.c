#include "bluetooth.h"
static bluetooth_t bluetooth;
bluetooth_config_t bluetooth_config;

static esp_ble_adv_data_t hidd_advertising_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 6, // Slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 10, // Slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = HID_GENERIC_APPEREANCE,
    .manufacturer_len    = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 16,
    .flag                = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 32,
    .adv_int_max        = 48,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

uint16_t get_bluetooth_hid_connection_id() {
    return bluetooth.hid_conn_id;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
            esp_ble_gap_start_advertising(&hidd_adv_params);
            
            break;
        }
        case ESP_GAP_BLE_SEC_REQ_EVT: {
            for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
                ESP_LOGD(BLUETOOTH_TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
            }

            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);

            break;
        }
        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
            bluetooth.sec_conn = true;
            esp_bd_addr_t bd_addr;

            memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            ESP_LOGI(BLUETOOTH_TAG, "remote BD_ADDR: %08x%04x", (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3], (bd_addr[4] << 8) + bd_addr[5]);
            ESP_LOGI(BLUETOOTH_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
            ESP_LOGI(BLUETOOTH_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");

            if(!param->ble_security.auth_cmpl.success) {
                ESP_LOGE(BLUETOOTH_TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
            }

            break;
        }
        default: {
            break;
        }
    }
}

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                esp_ble_gap_set_device_name(bluetooth_config.name);
                esp_ble_gap_config_adv_data(&hidd_advertising_data);
            }

            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH: {
	        break;
        }
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(BLUETOOTH_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            bluetooth.hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            bluetooth.sec_conn = false;
            ESP_LOGI(BLUETOOTH_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(BLUETOOTH_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(BLUETOOTH_TAG, param->vendor_write.data, param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(BLUETOOTH_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(BLUETOOTH_TAG, param->led_write.data, param->led_write.length);
            break;
        }
        default: {
            break;
        }
    }
    return;
}

void start_bluetooth(bluetooth_config_t config) {
    bluetooth_config = config;
    hidd_advertising_data.p_service_uuid = config.uuid_16bit;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_hidd_profile_init());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_hidd_register_callbacks(hidd_event_callback));

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &(esp_ble_auth_req_t){ ESP_LE_AUTH_BOND                           }, sizeof(esp_ble_auth_req_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &(esp_ble_io_cap_t){   ESP_IO_CAP_NONE                            }, sizeof(esp_ble_io_cap_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,    &(uint8_t){            16                                         }, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,    &(uint8_t){            ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK }, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,     &(uint8_t){            ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK }, sizeof(uint8_t));
}

void disable_bluetooth() {
    esp_bluedroid_disable();
}