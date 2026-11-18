/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BME280 sensor.
 *
 * This function sets up the sensor for communication over the given I2C port.
 *
 * @param[in] i2c_port I2C port number to which the sensor is connected.
 * @return 0 on success, non-zero on failure.
 */
int sensor_bme280_init(int i2c_port);

/**
 * @brief Read temperature in Celsius and relative humidity in percent.
 *
 * Output pointers may be NULL.
 *
 * @param[in] i2c_port I2C port number to which the sensor is connected.
 * @param[out] temperature Pointer to a float where temperature in degrees celsius will be stored.
 * @param[out] humidity Pointer to a float where relative humidity in % will be stored.
 * @param[out] pressure Pointer to a float where pressure in hectopascal will be stored.
 * @return 0 on success, non-zero on failure.
 */
int sensor_bme280_get_measurements(int i2c_port, float *temperature, float *humidity, float *pressure);

#ifdef __cplusplus
}
#endif
