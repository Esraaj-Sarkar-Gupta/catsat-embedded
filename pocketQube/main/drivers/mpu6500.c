#include "mpu6500.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MPU6500";

static int16_t combine_bytes(uint8_t high, uint8_t low) //mpu stores 16 bits as two 8 bit regs so we need to cvt it to one 16 bit val
{
    uint16_t value = ((uint16_t)high << 8) | low;

    return (int16_t)value;
}

static esp_err_t write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value){ //wrapper for i2c reg write
    return i2c_reg_write(device, reg, &value, 1); //1 bec we always write 1 byte to mpu ever
}
                        //addr = 0x98 or 0x69 depending on sensors AD0 pin config
esp_err_t mpu6500_init(i2c_master_dev_handle_t *device, uint8_t address){ //inits and configs the sensor
    
    if (device == NULL || (address != MPU6500_ADDR_LOW && address != MPU6500_ADDR_HIGH)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    err = i2c_bus_add_device(address, device); //register the device

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s",
        esp_err_to_name(err));
        return err;
    }

    uint8_t who_am_i;

    err = i2c_reg_read(*device, MPU6500_REG_WHO_AM_I, &who_am_i, 1); // verify id
    if (who_am_i != MPU6500_WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "Unexpected WHO_AM_I: 0x%02X", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }

    err = write_register(*device, MPU6500_REG_PWR_MGMT_1, 0x80); //reset sensor
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    err = write_register(*device, MPU6500_REG_PWR_MGMT_1, 0x01); // wake up sensor after reset
    err = write_register(*device, MPU6500_REG_SMPLRT_DIV, 9); //config sample rate fsample = fbase{1000}/(1+SMPLRT_DIV{9})

    err = write_register(*device, MPU6500_REG_CONFIG, 0x03); //config gyro low pass filter to 41hz
    err = write_register(*device, MPU6500_REG_ACCEL_CONFIG2, 0x03); //config acc low pass filter to 41hz
    err = write_register(*device, MPU6500_REG_GYRO_CONFIG, 0x18); //config gyro range
    err = write_register(*device, MPU6500_REG_ACCEL_CONFIG, 0x18); //config acc range

    ESP_LOGI(TAG, "MPU6500 initialization successful");
    return ESP_OK;
}

esp_err_t mpu6500_read(i2c_master_dev_handle_t device, mpu6500_data_t *data){
    
    if (device == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[14]; //2 bytes per field acc + gyro + temp

    esp_err_t err = i2c_reg_read(device, MPU6500_REG_ACCEL_XOUT_H, buffer, sizeof(buffer)); //read all measurments

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor read failed: %s",
        esp_err_to_name(err));
        return err;
    }

    for (int i = 0; i < 3; i++) {
        data->accel_raw[i] = combine_bytes(buffer[i * 2], buffer[i * 2 + 1]); //extract acc data
        data->accel_m_s2[i] = ((float)data->accel_raw[i] / MPU6500_ACCEL_SCALE) * MPU6500_GRAVITY; //cvt acc readings
    }

    int16_t temp_raw = combine_bytes(buffer[6], buffer[7]); //get temp
    data->temperature_c = ((float)temp_raw / 333.87f) + 21.0f;

    for (int i = 0; i < 3; i++) {
        data->gyro_raw[i] = combine_bytes(buffer[8 + i * 2], buffer[9 + i * 2]); //ext gyro
        data->gyro_dps[i] = (float)data->gyro_raw[i] / MPU6500_GYRO_SCALE; //cvt gyro
    }
    
    return ESP_OK;

}
