/* firmware: Fred the robot. Reads the AMG8833 thermal camera. */

#include <stdio.h>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AMG_SDA_GPIO    8
#define AMG_SCL_GPIO    9
#define AMG_I2C_ADDR    0x69
#define AMG_REG_PCTL    0x00   /* power control: 0x00 = normal mode */
#define AMG_REG_RST     0x01   /* reset: 0x3F = initial reset */
#define AMG_REG_FPSC    0x02   /* frame rate: 0x00 = 10 fps */
#define AMG_REG_INTC    0x03   /* interrupt control: 0x00 = disabled */
#define AMG_REG_PIXELS  0x80   /* 64 pixels x 2 bytes, little-endian */

#define SAMPLE_MS       1000

static i2c_master_dev_handle_t amg;

static esp_err_t amg_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(amg, buf, sizeof(buf), 100);
}

static void amg_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = AMG_SDA_GPIO,
        .scl_io_num = AMG_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AMG_I2C_ADDR,
        .scl_speed_hz = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &amg));

    ESP_ERROR_CHECK(amg_write_reg(AMG_REG_PCTL, 0x00));
    ESP_ERROR_CHECK(amg_write_reg(AMG_REG_RST, 0x3F));
    ESP_ERROR_CHECK(amg_write_reg(AMG_REG_INTC, 0x00));
    ESP_ERROR_CHECK(amg_write_reg(AMG_REG_FPSC, 0x00));
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* Mean of all 64 pixels in degrees C. */
static esp_err_t amg_read_mean(float *mean)
{
    uint8_t reg = AMG_REG_PIXELS;
    uint8_t raw[128];
    esp_err_t err = i2c_master_transmit_receive(amg, &reg, 1, raw, sizeof(raw), 100);
    if (err != ESP_OK) {
        return err;
    }
    int sum = 0;
    for (int i = 0; i < 64; i++) {
        int v = (raw[2 * i] | (raw[2 * i + 1] << 8)) & 0x0FFF;
        if (v & 0x800) {
            v -= 0x1000;   /* 12-bit two's complement */
        }
        sum += v;
    }
    *mean = sum * 0.25f / 64.0f;
    return ESP_OK;
}

void app_main(void)
{
    amg_init();

    while (1) {
        float mean;
        if (amg_read_mean(&mean) == ESP_OK) {
            printf("%.2f C\n", mean);
        } else {
            printf("sensor read failed\n");
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}
