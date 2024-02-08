#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define EXAMPLE_READ_LEN                    256
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_11
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH
#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#define ADC_MAX_VALUE                       4096

typedef struct {
    TaskHandle_t s_task_handle;
    adc_channel_t* channel;
    uint8_t adc_result[EXAMPLE_READ_LEN];
    uint32_t ret_num;
    uint32_t adc_pointer_i;
    esp_err_t adc_error;
    bool should_reset;
    adc_continuous_handle_t handle;
} adc_continuous_state_t;

typedef enum {
    OK,
    NO_DATA,
    ERROR
} iterator_state;

esp_err_t adc_read_next_error();
iterator_state adc_read_next(uint32_t* channel_number, uint32_t* data);