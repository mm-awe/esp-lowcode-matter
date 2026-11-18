#include <hal/i2c_types.h>
#include <ulp_lp_core_utils.h>
#include <esp_err.h>
#include <i2c_master.h>

#include "sensor_bme280.h"

static const char *TAG = "sensor_bme280";

#include "BME280_SensorAPI/bme280_defs.h"
#include "BME280_SensorAPI/bme280.h"


int read_bme_register(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    int i2c_port = *((int*)intf_ptr);
    return 0;
}

int write_bme_register(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    int i2c_port = *((int*)intf_ptr);
    return 0;
}

void delay_bme(uint32_t period, void *intf_ptr) {
    ulp_lp_core_delay_us(period);
}

const static struct bme280_dev default_dev = {
    .chip_id = 0x60,
    .bme280_intf = BME280_I2C_INTF,
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


//// copied from common.c in BME280 API, see submodule for license info ////

void bme280_error_codes_print_result(const char api_name[], int8_t rslt)
{
    if (rslt != BME280_OK)
    {
        printf("%s\t", api_name);

        switch (rslt)
        {
            case BME280_E_NULL_PTR:
                printf("Error [%d] : Null pointer error.", rslt);
                printf(
                    "It occurs when the user tries to assign value (not address) to a pointer, which has been initialized to NULL.\r\n");
                break;

            case BME280_E_COMM_FAIL:
                printf("Error [%d] : Communication failure error.", rslt);
                printf(
                    "It occurs due to read/write operation failure and also due to power failure during communication\r\n");
                break;

            case BME280_E_DEV_NOT_FOUND:
                printf("Error [%d] : Device not found error. It occurs when the device chip id is incorrectly read\r\n",
                       rslt);
                break;

            case BME280_E_INVALID_LEN:
                printf("Error [%d] : Invalid length error. It occurs when write is done with invalid length\r\n", rslt);
                break;

            default:
                printf("Error [%d] : Unknown error code\r\n", rslt);
                break;
        }
    }
}


//// Public interface ////

int sensor_bme280_init(int i2c_port) {
    struct bme280_dev dev = default_dev;
    dev.intf_ptr = &i2c_port;

    int8_t rslt;
    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &default_settings, &dev);
    bme280_error_codes_print_result("bme280_set_sensor_settings", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    bme280_error_codes_print_result("bme280_set_power_mode", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;

    return ESP_OK;
}

int sensor_bme280_get_measurements(int i2c_port, float *temperature, float *humidity, float *pressure) {
    struct bme280_dev dev = default_dev;
    dev.intf_ptr = &i2c_port;

    int8_t rslt;
    //TODO check if measurement is valid
    struct bme280_data measurement;
    rslt = bme280_get_sensor_data(BME280_TEMP, &measurement, &dev);
    bme280_error_codes_print_result("bme280_get_sensor_data", rslt);
    if (rslt != 0) return ESP_ERR_INVALID_RESPONSE;

    if (temperature) *temperature = measurement.temperature;
    if (humidity) *humidity = measurement.humidity;
    if (pressure) *pressure = measurement.pressure;

    return ESP_OK;
}

