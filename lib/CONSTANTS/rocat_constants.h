#pragma once

#include <stdint.h>

/*
    Pin Definitions
*/

const uint8_t pin_touch_pet_sens_1 = 32;
const uint8_t pin_touch_pet_sens_2 = 33;

const uint8_t pin_aud_sig = 25;

const uint8_t pin_gio_sda = 0;
const uint8_t pin_gio_scl = 27;
// const uint8_t pin_gio_ws2812 = 15;
#define pin_gio_ws2812 15
const uint8_t pin_gio_exp_rst = 4;

const uint8_t pin_dis_ao = 14;
const uint8_t pin_dis_en = 13;
const uint8_t pin_display[] = {9, 10, 5, 18, 23, 19, 22, 21};

const uint8_t pin_rtc_int1 = 12;
const uint8_t pin_acc_int1 = 2;


const uint8_t pin_gpio_js_a_c1 = 34;
const uint8_t pin_gpio_js_b_d1 = 35;
const uint8_t pin_gpio_js_c1_2 = 36;
const uint8_t pin_gpio_js_a_c2 = 37;
const uint8_t pin_gpio_js_b_d2 = 38;
const uint8_t pin_gpio_ldr = 39;

/*
    IO Expander
*/

// SX1509 I2C address (set by ADDR1 and ADDR0 (00 by default):
const byte sx1509_address = 0x3E; // SX1509 I2C address
const byte pin_m_fault = 1;
const byte pin_m_stby = 2;
const byte pin_m_drv_en = 3;
const byte pin_m1_dir = 4;
const byte pin_m2_dir = 5;
const byte pin_m1_spd = 6;
const byte pin_m2_spd = 7;

const byte pin_chg_ind = 8;
const byte pin_stby_ind = 9;
const byte pin_dis_pwr = 10;
const byte pin_dis_bl = 11;
const byte pin_dis_rw = 12;
const byte pin_aud_en = 13;
const byte pin_mcu_keep_awake = 14; 
const byte pin_rgb_pwr = 15;

/*
    Accelerometer
*/
const uint8_t lis3dh_address = 0x18;
// const uint8_t lis3dh_address = 0x19;
// ADC is hardcoded in the battery driver

/*
    LED
*/

#define count_ws2812 6