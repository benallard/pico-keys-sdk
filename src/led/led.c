    /*
    * This file is part of the Pico Keys SDK distribution (https://github.com/polhenarejos/pico-keys-sdk).
    * Copyright (c) 2022 Pol Henarejos.
    *
    * This program is free software: you can redistribute it and/or modify
    * it under the terms of the GNU General Public License as published by
    * the Free Software Foundation, version 3.
    *
    * This program is distributed in the hope that it will be useful, but
    * WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    * General Public License for more details.
    *
    * You should have received a copy of the GNU General Public License
    * along with this program. If not, see <http://www.gnu.org/licenses/>.
    */

#include <stdio.h>
#include <stdlib.h>
#include "pico_keys.h"
#ifdef PICO_PLATFORM
#include "bsp/board.h"
#elif defined(ESP_PLATFORM)
#include "esp_compat.h"
#elif defined(ENABLE_EMULATION)
#include "emulation.h"
#endif

extern void led_driver_init();
extern void led_driver_color(uint8_t, float);

static uint32_t led_mode = MODE_NOT_MOUNTED;

void led_set_mode(uint32_t mode) {
    led_mode = mode;
}

void led_blinking_task() {
#ifndef ENABLE_EMULATION
    static uint32_t start_ms = 0;
    static uint32_t stop_ms = 0;
    static uint8_t led_state = false;
    uint8_t state = led_state;
#ifdef PICO_DEFAULT_LED_PIN_INVERTED
    state = !state;
#endif
    uint8_t led_fade = (led_mode & LED_FADE_MASK) >> LED_FADE_SHIFT;
    uint32_t led_max_brightness = (led_mode & LED_BTNESS_MASK) >> LED_BTNESS_SHIFT;
    uint32_t led_color = (led_mode & LED_COLOR_MASK) >> LED_COLOR_SHIFT;
    uint32_t led_off = (led_mode & LED_OFF_MASK) >> LED_OFF_SHIFT;
    uint32_t led_on = (led_mode & LED_ON_MASK) >> LED_ON_SHIFT;

    // how far in the current state from 0 - 1
    float progress = (float)(board_millis() - start_ms) / (stop_ms - start_ms);

    if (!state){
        // fading down so 1 -> 0
        progress = 1 - progress;
    }

    // maybe quick return if progress didn't changed much ?

    // current one from 0 - 1 
    float led_brightness = ((float)led_max_brightness / MAX_BTNESS) * progress;
    if (!led_fade){
        led_brightness = led_max_brightness;
    }

    led_driver_color(led_color, led_brightness);

    if (board_millis() >= stop_ms){
        start_ms = stop_ms;
        led_state ^= 1; // toggle
        stop_ms = start_ms + (led_state ? led_on : led_off);
    }
#endif
}

void led_off_all() {
#ifndef ENABLE_EMULATION
    led_driver_color(LED_COLOR_OFF, 0);
#endif
}

void led_init() {
#ifndef ENABLE_EMULATION
    led_driver_init();
    led_set_mode(MODE_NOT_MOUNTED);
#endif
}
