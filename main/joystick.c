#include "joystick.h"
#include "continuous_adc.h"
#include "esp_log.h"

#define JOYSTICK_DEADZONE_MIN 2250
#define JOYSTICK_DEADZONE_MAX 2450
#define JOYSTICK_SCALE_FACTOR 4.5
#define MIN_CORRECTING_FACTOR 1.4
#define SCALE_COEFFICIENT 1.002

int8_t adc_value_to_mouse_movement(uint16_t adc_value) {
    if (adc_value >= JOYSTICK_DEADZONE_MIN && adc_value <= JOYSTICK_DEADZONE_MAX) {
        return 0;
    }

    int distance_from_dead_zone = 0;
    if (adc_value < JOYSTICK_DEADZONE_MIN) {
        distance_from_dead_zone = adc_value - JOYSTICK_DEADZONE_MIN;
    } else {
        distance_from_dead_zone = (adc_value - JOYSTICK_DEADZONE_MAX) * 1.4;
    }

    float normalized_value = distance_from_dead_zone / (float)(ADC_MAX_VALUE - JOYSTICK_DEADZONE_MAX);
    float scaled_value = (normalized_value * normalized_value) * (distance_from_dead_zone > 0 ? 1 : -1);

    return (int8_t) scaled_value * JOYSTICK_SCALE_FACTOR;
}

int8_t joystick_tick(int64_t *movement_start, uint16_t adc_value) {
    if (*movement_start == 0) {
        *movement_start = esp_timer_get_time();
    }

    int64_t movement_elapsed_milliseconds = (esp_timer_get_time() - *movement_start) / 1000;
    double mouse_movement_acceleration = pow(SCALE_COEFFICIENT, fmin(movement_elapsed_milliseconds, 1500));
    int8_t mouse_movement = adc_value_to_mouse_movement(adc_value);

    int64_t result = mouse_movement * mouse_movement_acceleration;
    return result > 255 ? 255 : result;
}

void joystick_end(int64_t *movement_start) {
    *movement_start = 0;
}
