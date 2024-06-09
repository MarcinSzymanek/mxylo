/*
* This header file includes methods for interfacing SparkFun's SerLCD display
* SerLCD is interfaced via Mega2560T microcontroller running OpenLCD
*/

#pragma once
#include "stdint.h"
#include "esp_err.h"
#include "esp_log.h"
#include <queue>

// #define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL      /*!< GPIO number used for I2C master clock */
// #define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA      /*!< GPIO number used for I2C master data  */
// #define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
// #define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
// #define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_TIMEOUT_MS       1000

#define MAX_LINE_WIDTH 20
// Interface for OpenLCD display using I2C on ESP32Vroom
class OpenLCD{
public:
    OpenLCD();
    esp_err_t init_openLCD(int&& pin_scl_p, int&& pin_sda_p);
    esp_err_t write(const char* str, size_t size);
    esp_err_t clear();
    bool update_line(const char* str, size_t line); // Update buffer line with text
    bool update_display(); // Display buffer contents

private:

    void clear_buffer_();
    bool switch_line_(size_t line); // Switch cursor position
    const char i2c_addr = 0x72;
    const char *TAG = "OpenLCD";
    char lcd_buffer[2][MAX_LINE_WIDTH];
};