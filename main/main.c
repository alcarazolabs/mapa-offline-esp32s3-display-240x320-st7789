#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "st7789.h"
#include "esp_spiffs.h"
#include <dirent.h>
#include "map_view.h"
#include "driver/gpio.h"

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 320

// VCC -> 3.3V
// BL -> 3.3v

#define PIN_MOSI 11  //(SDA Pin)
#define PIN_SCLK 12 // (SLC Pin)
#define PIN_CS 10   // (CS Pin)
#define PIN_DC 9   // (D/C Pin)
#define PIN_RST 14 // (RES Pin)

#define BTN1_GPIO 4 // Navegar para arriba.
#define BTN2_GPIO 5 // Navegar para abajo.
#define BTN3_GPIO 6 // Navegar a la derecha.
#define BTN4_GPIO 7 // Navegar a la izquierda.

static const char *TAG = "app-main";
SemaphoreHandle_t lvgl_mutex = NULL;

void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 10,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    ESP_LOGI(TAG, "Mounting SPIFFS... %s", esp_err_to_name(ret));

    size_t total = 0;
    size_t used = 0;

    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS info: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SPIFFS total: %d bytes, used: %d bytes", total, used);

    DIR *dir = opendir("/spiffs");
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            ESP_LOGW(TAG, "File: %s", entry->d_name);
        }

        closedir(dir);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open /spiffs/");
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void lvgl_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;

    uint16_t w = x2 - x1 + 1;
    uint16_t h = y2 - y1 + 1;

    st7789_set_window(x1, y1, x2, y2);
    st7789_push_color(px_map, w * h * 2);

    lv_display_flush_ready(disp);
}

static void setup_lvgl()
{
    lv_init();

    const esp_timer_create_args_t lvgl_tick = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"};

    esp_timer_handle_t lvgl_timer;
    esp_timer_create(&lvgl_tick, &lvgl_timer);
    esp_timer_start_periodic(lvgl_timer, 1000);

    lv_display_t *display =
        lv_display_create(DISPLAY_HEIGHT, DISPLAY_WIDTH);

    static lv_color_t buf[DISPLAY_WIDTH * 32];
    lv_display_set_buffers(display, buf, NULL,
                           sizeof(buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(display, lvgl_flush_cb);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    while (true)
    {
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        lv_timer_handler();
        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void buttons_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN1_GPIO) |
                        (1ULL << BTN2_GPIO) |
                        (1ULL << BTN3_GPIO) |
                        (1ULL << BTN4_GPIO),
        //.pull_down_en = GPIO_PULLDOWN_ENABLE,
        //.pull_up_en = GPIO_PULLUP_DISABLE
        .pull_up_en = GPIO_PULLUP_ENABLE,     // ACTIVAR PULL-UP
        .pull_down_en = GPIO_PULLDOWN_DISABLE}; // DESACTIVAR PULL-DOWN

    gpio_config(&io_conf);
}

static void buttons_task(void *arg)
{
    buttons_init();

    while (1)
    {

        if (gpio_get_level(BTN1_GPIO) == 0)
        {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
            map_move_up();
            xSemaphoreGive(lvgl_mutex);
            ESP_LOGI(TAG, "Move Up");
        }
        else if (gpio_get_level(BTN2_GPIO) == 0)
        {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
            map_move_down();
            xSemaphoreGive(lvgl_mutex);
            ESP_LOGI(TAG, "Move Down");
        }
        else if (gpio_get_level(BTN3_GPIO) == 0)
        {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
            map_move_right();
            xSemaphoreGive(lvgl_mutex);
            ESP_LOGI(TAG, "Move Right");
        }
        else if (gpio_get_level(BTN4_GPIO) == 0)
        {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
            map_move_left();
            xSemaphoreGive(lvgl_mutex);
            ESP_LOGI(TAG, "Move Left");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    lvgl_mutex = xSemaphoreCreateMutex();

    init_spiffs();
    st7789_config_t lcd_cfg = {
        .spi_host = SPI2_HOST,
        .pin_mosi = PIN_MOSI,
        .pin_sclk = PIN_SCLK,
        .pin_cs = PIN_CS,
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .colstart = 0,
        .rowstart = 0,
        .spi_clock_hz = 80 * 1000 * 1000};

    st7789_init(&lcd_cfg);
    st7789_set_rotation(1);

    setup_lvgl();

    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    lv_obj_t *scr = lv_scr_act();
    map_view_create(scr, 16, 24278, 37181);
    xSemaphoreGive(lvgl_mutex);

    xTaskCreate(lvgl_task, "lvgl_task ", 16384, NULL, 5, NULL);
    xTaskCreate(buttons_task, "buttons_task", 16384, NULL, 5, NULL);
}
