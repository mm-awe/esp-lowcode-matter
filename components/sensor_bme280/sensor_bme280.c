/*
 * SPDX-FileCopyrightText: 2026 M&M Software GmbH
 */

#include <hal/i2c_types.h>
#include <ulp_lp_core_utils.h>
#include <esp_err.h>
#include <i2c_master.h>

#include "sensor_bme280.h"

static const char *TAG = "sensor_bme280";

#include "BME280_SensorAPI/bme280_defs.h"
#include "BME280_SensorAPI/bme280.h"

//about 1s
#define I2C_TIMEOUT 16000000

#define BME280_I2C_ADDR BME280_I2C_ADDR_PRIM

int8_t send_bme_register(const int i2c_port, const uint8_t reg_addr) {
    int res = i2c_master_write_to_device(i2c_port, BME280_I2C_ADDR, &reg_addr, 1, I2C_TIMEOUT); //send requested register
    if (res != 0) printf("Error sending register 0x%02x on addr 0x%02x on port %d!", reg_addr, BME280_I2C_ADDR, i2c_port);
    return res;
}

int8_t read_bme_register(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    int i2c_port = *((int*)intf_ptr);
    if (send_bme_register(i2c_port, reg_addr) != 0) return -1;
    int res = i2c_master_read_from_device(i2c_port, BME280_I2C_ADDR, reg_data, len, I2C_TIMEOUT);
    if (res != 0) printf("Error reading register 0x%02x on addr 0x%02x on port %d!", reg_addr, BME280_I2C_ADDR, i2c_port);
    return res;
}

int8_t write_bme_register(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    int i2c_port = *((int*)intf_ptr);
    
    uint8_t buf[len*2];
    for (uint32_t i=0; i<len*2; i+=2) {
        buf[i+0] = reg_addr;
        buf[i+1] = reg_data[i/2];
    }
    int res = i2c_master_write_to_device(i2c_port, BME280_I2C_ADDR, &buf, len*2, I2C_TIMEOUT);
    if (res != 0) { printf("Error writing register 0x%02x on addr 0x%02x on port %d!", reg_addr, BME280_I2C_ADDR, i2c_port); return res;}
    return 0;
}

void delay_bme(uint32_t period, void *intf_ptr) {
    ulp_lp_core_delay_us(period);
}

const static struct bme280_dev default_dev = {
    .chip_id = 0x60,
    .intf = BME280_I2C_INTF,
    .intf_ptr = NULL, //pointer to i2c_port
    .read = &read_bme_register,
    .write = &write_bme_register,
    .delay_us = &delay_bme
};

const static struct bme280_settings default_settings = {
    .osr_p = BME280_OVERSAMPLING_16X,
    .osr_t = BME280_OVERSAMPLING_16X,
    .osr_h = BME280_OVERSAMPLING_16X,
    .filter = BME280_FILTER_COEFF_16, //to reach >=75% of step response: 22 meas cycles = 528ms, fast enough for a sensor that reports every 10s
    .standby_time = BME280_STANDBY_TIME_0_5_MS, //with all at 16x os, 8ms for a value update, 24ms for a measurement cycle
};


//// adapted from common.c in BME280 API, see submodule for license info ////

void bme280_error_codes_print_result(const char api_name[], int8_t rslt)
{
    printf("%s ", api_name);
    if (rslt != BME280_OK)
    {
        switch (rslt)
        {
            case BME280_E_NULL_PTR:
                printf("Error [%d] : Null pointer error.\n", rslt);
                break;

            case BME280_E_COMM_FAIL:
                printf("Error [%d] : Communication failure error.\n", rslt);
                break;

            case BME280_E_DEV_NOT_FOUND:
                printf("Error [%d] : Device not found error. Chip id is incorrectly read.\n", rslt);
                break;

            case BME280_E_INVALID_LEN:
                printf("Error [%d] : Invalid length error.\n", rslt);
                break;

            default:
                printf("Error [%d] : Unknown error code\r\n", rslt);
                break;
        }
    }
    else {
        printf("OK [%d]\n", rslt);
    }
}



struct bme280_calib_data sensor_cal; //TODO, make this not a global var


//// Public interface ////

int sensor_bme280_init(int i2c_port) {
    struct bme280_dev dev = default_dev;
    dev.intf_ptr = &i2c_port;

    int8_t rslt;
    rslt = bme280_init(&dev);
    bme280_error_codes_print_result("bme280_init", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;
    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &default_settings, &dev);
    bme280_error_codes_print_result("bme280_set_sensor_settings", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;
    struct bme280_settings readback;
    rslt = bme280_get_sensor_settings(&readback, &dev);
    bme280_error_codes_print_result("bme280_get_sensor_settings", rslt);
    if (memcmp(&default_settings, &readback, sizeof(default_settings))) {
        printf("Sensor did not accept settings!\n");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    bme280_error_codes_print_result("bme280_set_power_mode", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;

    sensor_cal = dev.calib_data; //store sensor calibration data

    return ESP_OK;
}

int sensor_bme280_get_measurements(int i2c_port, float *temperature, float *humidity, float *pressure) {
    //rebuild dev
    struct bme280_dev dev = default_dev;
    dev.intf_ptr = &i2c_port;
    dev.calib_data = sensor_cal; //load sensor calibration data

    int8_t rslt;
    //TODO check if measurement is valid
    struct bme280_data measurement;
    rslt = bme280_get_sensor_data(BME280_ALL, &measurement, &dev);
    bme280_error_codes_print_result("bme280_get_sensor_data", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;

    if (temperature) *temperature = measurement.temperature;
    if (humidity) *humidity = measurement.humidity;
    if (pressure) *pressure = measurement.pressure;

    return ESP_OK;
}

