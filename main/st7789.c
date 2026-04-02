#include "st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT 0x11
#define ST7789_COLMOD 0x3A
#define ST7789_MADCTL 0x36
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_DISPON 0x29

#define MADCTL_BGR 0x08
#define MADCTL_MX 0x40
#define MADCTL_MY 0x80
#define MADCTL_MV 0x20

static st7789_config_t lcd;
static spi_device_handle_t spi;
static uint16_t panel_width;
static uint16_t panel_height;
static uint8_t current_rotation = 0;

static void spi_send(const void *data, size_t len, bool dc)
{
    gpio_set_level(lcd.pin_dc, dc);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(spi, &t);
}

static inline void write_cmd(uint8_t cmd)
{
    spi_send(&cmd, 1, 0);
}

static inline void write_data(const void *data, size_t len)
{
    spi_send(data, len, 1);
}

static void reset_display(void)
{
    gpio_set_level(lcd.pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(lcd.pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

/* ================= INIT ================= */

void st7789_init(const st7789_config_t *cfg)
{
    memcpy(&lcd, cfg, sizeof(st7789_config_t));

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << lcd.pin_dc) |
                        (1ULL << lcd.pin_rst) |
                        (1ULL << lcd.pin_cs),
        .mode = GPIO_MODE_OUTPUT};
    gpio_config(&io);

    spi_bus_config_t buscfg = {
        .mosi_io_num = lcd.pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = lcd.pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = lcd.spi_clock_hz,
        .spics_io_num = lcd.pin_cs,
        .queue_size = 1,
    };

    panel_width = lcd.width;
    panel_height = lcd.height;

    spi_bus_initialize(lcd.spi_host, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(lcd.spi_host, &devcfg, &spi);

    reset_display();

    write_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    write_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // uint8_t madctl = MADCTL_MX | MADCTL_MY;
    // write_cmd(ST7789_MADCTL);
    // write_data(&madctl, 1);

    uint8_t colmod = 0x55; // RGB565
    write_cmd(ST7789_COLMOD);
    write_data(&colmod, 1);

    write_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    x0 += lcd.colstart;
    x1 += lcd.colstart;
    y0 += lcd.rowstart;
    y1 += lcd.rowstart;

    uint8_t data[4];

    write_cmd(ST7789_CASET);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    write_data(data, 4);

    write_cmd(ST7789_RASET);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    write_data(data, 4);

    write_cmd(ST7789_RAMWR);
}

void st7789_push_color(const void *data, size_t len)
{
    const uint16_t *src = (const uint16_t *)data;

    static uint16_t dma_buf[1024];

    size_t pixels = len / 2;

    while (pixels)
    {
        size_t batch = (pixels > 1024) ? 1024 : pixels;

        for (size_t i = 0; i < batch; i++)
        {
            uint16_t c = src[i];
            dma_buf[i] = (c >> 8) | (c << 8);
        }

        write_data(dma_buf, batch * 2);

        src += batch;
        pixels -= batch;
    }
}

void st7789_set_rotation(uint8_t rotation)
{
    current_rotation = rotation % 4;

    uint8_t madctl = MADCTL_BGR;

    switch (current_rotation)
    {
    case 0: // 0°
        madctl |= MADCTL_MX | MADCTL_MY;
        lcd.width = panel_width;
        lcd.height = panel_height;
        break;

    case 1: // 90°
        madctl |= MADCTL_MY | MADCTL_MV;
        lcd.width = panel_height;
        lcd.height = panel_width;
        break;

    case 2: // 180°
        // sem MX/MY
        lcd.width = panel_width;
        lcd.height = panel_height;
        break;

    case 3: // 270°
        madctl |= MADCTL_MX | MADCTL_MV;
        lcd.width = panel_height;
        lcd.height = panel_width;
        break;
    }

    write_cmd(ST7789_MADCTL);
    write_data(&madctl, 1);
}