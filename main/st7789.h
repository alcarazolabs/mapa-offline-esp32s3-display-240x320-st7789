#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include <stddef.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef struct
{
    spi_host_device_t spi_host;

    int pin_mosi;
    int pin_sclk;
    int pin_cs;
    int pin_dc;
    int pin_rst;

    uint16_t width;
    uint16_t height;

    uint16_t colstart;
    uint16_t rowstart;

    uint32_t spi_clock_hz;
} st7789_config_t;

void st7789_init(const st7789_config_t *cfg);
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void st7789_push_color(const void *data, size_t len);
void st7789_set_rotation(uint8_t rotation);

#endif /* ST7789_H */