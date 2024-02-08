#include "continuous_adc.h"
#include "esp_log.h"

adc_continuous_state_t state = {
    .adc_result = {0},
    .adc_pointer_i = 0,
    .should_reset = false,
};

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &state.handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(state.handle, &dig_cfg));
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(state.s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

void start_adc(adc_channel_t* channel_p, uint8_t channel_num) {
    state.channel = channel_p;
    memset(state.adc_result, 0xcc, EXAMPLE_READ_LEN);

    state.s_task_handle = xTaskGetCurrentTaskHandle();

    continuous_adc_init(channel_p, channel_num);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb
    };

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(state.handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(state.handle));
}

esp_err_t adc_read_next_err() {
    return state.adc_error;
}

iterator_state adc_read_next(uint32_t* chan_num, uint32_t* data) {
    if (state.should_reset) {
        state.should_reset = false;
        state.adc_pointer_i = 0;
        state.adc_error = adc_continuous_read(state.handle, state.adc_result, EXAMPLE_READ_LEN, &state.ret_num, 0);
        if (state.adc_error != ESP_OK) {
            state.should_reset = true;
        }
    }

    if (state.adc_error != 0) {
        return ERROR;
    }

    if (state.adc_pointer_i >= state.ret_num) {
        state.should_reset = true;
        return NO_DATA;
    }

    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&state.adc_result[state.adc_pointer_i];
    *chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
    *data = EXAMPLE_ADC_GET_DATA(p);

    state.adc_pointer_i += SOC_ADC_DIGI_RESULT_BYTES;

    return *chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT) ? OK : ERROR;
}