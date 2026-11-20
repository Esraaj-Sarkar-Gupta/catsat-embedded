#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "i2c_bus.h"

/* Default I2C address */
#define MPU6500_ADDR_LOW       0x68
#define MPU6500_ADDR_HIGH      0x69

/* Register addresses */
#define MPU6500_REG_SMPLRT_DIV     0x19
#define MPU6500_REG_CONFIG         0x1A
#define MPU6500_REG_GYRO_CONFIG    0x1B
#define MPU6500_REG_ACCEL_CONFIG   0x1C
#define MPU6500_REG_ACCEL_CONFIG2  0x1D

#define MPU6500_REG_ACCEL_XOUT_H   0x3B
#define MPU6500_REG_TEMP_OUT_H     0x41
#define MPU6500_REG_GYRO_XOUT_H    0x43

#define MPU6500_REG_PWR_MGMT_1     0x6B
#define MPU6500_REG_WHO_AM_I       0x75

#define MPU6500_WHO_AM_I_VALUE     0x70

/* Sensor configuration */ 
#define MPU6500_ACCEL_SCALE        2048.0f // +- 16g sensitivity = 2048 LSB/g
#define MPU6500_GYRO_SCALE          16.4f //=- 2000 deg/s sensitivity = 16.4 LSB/(deg/s)    

#define MPU6500_GRAVITY             9.80665f

/* Sensor measurements */
typedef struct {
    int16_t accel_raw[3]; // raw vals
    int16_t gyro_raw[3];

    float accel_m_s2[3]; // cvted values
    float gyro_dps[3];

    float temperature_c;

} mpu6500_data_t;


/* Driver functions */

esp_err_t mpu6500_init(
    i2c_master_dev_handle_t *device,
    uint8_t address
);

esp_err_t mpu6500_read(
    i2c_master_dev_handle_t device,
    mpu6500_data_t *data
);