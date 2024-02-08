#include "esp_system.h"
#include "esp_timer.h"
#include "math.h"

int8_t joystick_tick(int64_t *movement_start, uint16_t adc_value);
void joystick_end(int64_t *movement_start);
