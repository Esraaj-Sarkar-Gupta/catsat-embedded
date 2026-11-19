#include "i2c_bus.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "I2C";

static i2c_master_bus_handle_t bus_handle = NULL;

esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, //filter to reject short noisy pulses
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle); //init the bus handle
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s",
        esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "I2C master initialized");
    ESP_LOGI(TAG, "SDA: GPIO%d, SCL: GPIO%d",
    I2C_MASTER_SDA_IO,
    I2C_MASTER_SCL_IO);

    return ESP_OK;
}

esp_err_t i2c_bus_add_device(uint8_t address, i2c_master_dev_handle_t *device){
    
    if (bus_handle == NULL || device == NULL) { //make sure bus is init and a device handle is given
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, //sensor reg addressing used
        .device_address = address, //sensor addr
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
                                    //(i2c bus handle, device config, pointer to device handle that idf will init)
    return i2c_master_bus_add_device(bus_handle,&device_config, device); //registers the device
}

esp_err_t i2c_bus_scan(void)
{
    if (bus_handle == NULL) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Starting I2C scan...");
    int found = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) { //itr over all common i2c addrses
        
        esp_err_t err = i2c_master_probe(bus_handle, addr, I2C_TIMEOUT_MS); //probe the specific i2c addr
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address 0x%02X", addr);
            found++;
        }
    }

    ESP_LOGI(TAG, "I2C scan completed. Devices found: %d", found);
    return found > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;

}

                    //(which dev, which reg to read, ptr to mem where we want to store data, len of data to read)
esp_err_t i2c_reg_read(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data, size_t len){

    if (device == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(device, &reg, 1, data, len, I2C_TIMEOUT_MS); //performs read transaction
}

esp_err_t i2c_reg_write(i2c_master_dev_handle_t device, uint8_t reg, const uint8_t *data, size_t len){

    if (device == NULL || data == NULL || len == 0 || len > I2C_MAX_WRITE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[I2C_MAX_WRITE_SIZE + 1]; //create temp buffer for reg addr + data to write (1 + 32) resp
    buffer[0] = reg;
    memcpy(&buffer[1], data, len);

    return i2c_master_transmit(device, buffer, len + 1, I2C_TIMEOUT_MS);
}