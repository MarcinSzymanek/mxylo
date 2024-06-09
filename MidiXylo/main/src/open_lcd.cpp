#include "open_lcd.hpp"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "string.h"

// This class is an interface to OpenLCD (firmware of SerLCD) using I2C

OpenLCD::OpenLCD(){
    ESP_LOGI(TAG, "Hello from OpenLCD!");
}

esp_err_t OpenLCD::init_openLCD(int&& pin_scl_p, int&& pin_sda_p){
    // I2C vars
    // ESP32 Vroom i2c pins
    const int pin_scl = pin_scl_p;
    const int pin_sda = pin_sda_p;
    const char i2c_master_no = 0;
    const int i2c_master_freq_hz = 400000;
    const int master_port = 0;
    ESP_LOGI("LCD", "LCD init");
    // Initialize i2c
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = pin_sda;
    conf.scl_io_num = pin_scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = i2c_master_freq_hz;
    conf.clk_flags = 0;

    i2c_param_config(master_port, &conf);
    esp_err_t err = i2c_driver_install(i2c_master_no, conf.mode, 0, 0, 0);
    // For some reason i2c doesn't work without calling this
    err = i2c_set_timeout(0, 30);
    ESP_ERROR_CHECK(err);
    clear_buffer_();
    clear();

    vTaskDelay(20);
    return err;
}

esp_err_t OpenLCD::write(const char* str, size_t size){
    uint8_t buffer[size];
    for(int i = 0; i < size; i++){
        buffer[i] = (uint8_t)*str;
        str++;
    }
    esp_err_t err = i2c_master_write_to_device(0, i2c_addr, buffer, size, 20000);
    return ESP_OK;
}

bool OpenLCD::update_display(){
    clear();
    write(lcd_buffer[0], MAX_LINE_WIDTH);
    switch_line_(1);
    write(lcd_buffer[1], MAX_LINE_WIDTH);

    return true;
}

bool OpenLCD::switch_line_(size_t line){
    if(line > 4) return false;
    uint8_t line_cmd = 0;
    if(line > 1) line_cmd |= 0b00010100;
    if(line % 2 == 1) line_cmd |= 0b01000000;

    char data[2] = {254, (char)(0b10000000 | line_cmd)};
    write(data, 2);
    vTaskDelay(1);
    return true;
}

bool OpenLCD::update_line(const char* text, size_t line){
    switch_line_(line);
    write(text, MAX_LINE_WIDTH);
    return true;
}

void OpenLCD::clear_buffer_(){
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < MAX_LINE_WIDTH; j++){
            lcd_buffer[i][j] = ' ';
        }
    }
}

esp_err_t OpenLCD::clear(){
    int ret;
    char data[2] = {'|', '-'};
    return write(data, 2);
}