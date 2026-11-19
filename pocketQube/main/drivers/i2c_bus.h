#pragma once

#include <stdint.h>
#include <stddef.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_PORT       I2C_NUM_0 //which controller (0 or 1)
#define I2C_MASTER_SDA_IO     8 //sda pin
#define I2C_MASTER_SCL_IO     9 //scl pin
#define I2C_MASTER_FREQ_HZ    400000 //i2c clock freq

#define I2C_TIMEOUT_MS        1000 //maxm wait time for i2c response
#define I2C_MAX_WRITE_SIZE    32 //This is a limit we impose on the maximum number of data bytes our write function accepts.

esp_err_t i2c_bus_init(void);  //init i2c controller
                                //(i2c dev addr, pointer to device handle)
esp_err_t i2c_bus_add_device(uint8_t address, i2c_master_dev_handle_t *device); //registers an i2c device with the controller

esp_err_t i2c_bus_scan(void); //scan bus for registered devices

esp_err_t i2c_reg_read(     //reads data from sensor register
    i2c_master_dev_handle_t device, //which sensor to communicate with
    uint8_t reg, //starting reg address
    uint8_t *data, //mem where recieved bytes will be stored
    size_t len //num of bytes to read
);

esp_err_t i2c_reg_write(        //writes data to sensor register 
    i2c_master_dev_handle_t device, //which device to write to
    uint8_t reg, //starting reg address
    const uint8_t *data, //mem where data to write is stored
    size_t len //len of data to write
);