#include "map_view.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#define MAP_WIDTH 320
#define MAP_HEIGHT 240
#define TILE_SIZE 256
#define TILE_PIXELS (TILE_SIZE * TILE_SIZE)
#define TILE_BYTES (TILE_PIXELS * sizeof(uint16_t))

#define MOVE_STEP 4
#define TILE_PATH_FMT "/spiffs/%d_%d_%d.rgb565"

static const char *TAG = "MAP_VIEW";

typedef struct
{
    int zoom;
    int tile_x;
    int tile_y;
    int offset_x;
    int offset_y;
} map_state_t;

static map_state_t map = {0};

static lv_obj_t *canvas = NULL;
static uint16_t *canvas_buf = NULL;

#define CACHE_W 3
#define CACHE_H 2

static uint16_t *tile_cache[CACHE_W][CACHE_H] = {0};
static int cache_tile_x[CACHE_W][CACHE_H];
static int cache_tile_y[CACHE_W][CACHE_H];

static void get_tile_path(char *path, size_t size, int zoom, int x, int y)
{
    snprintf(path, size, TILE_PATH_FMT, zoom, x, y);
}

static bool load_tile_into(uint16_t *buf, int zoom, int x, int y)
{
    char path[64];
    get_tile_path(path, sizeof(path), zoom, x, y);

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        memset(buf, 0xFF, TILE_BYTES);
        return false;
    }

    size_t br = fread(buf, 1, TILE_BYTES, f);
    fclose(f);

    if (br != TILE_BYTES)
    {
        memset(buf, 0xFF, TILE_BYTES);
        return false;
    }

    return true;
}

static void ensure_tile_cached(int cx, int cy, int tile_x, int tile_y)
{
    if (cache_tile_x[cx][cy] == tile_x &&
        cache_tile_y[cx][cy] == tile_y)
        return;

    load_tile_into(tile_cache[cx][cy], map.zoom, tile_x, tile_y);
    cache_tile_x[cx][cy] = tile_x;
    cache_tile_y[cx][cy] = tile_y;
}

static void normalize_position(void)
{
    bool tile_changed = false;

    while (map.offset_x < 0)
    {
        map.tile_x--;
        map.offset_x += TILE_SIZE;
        tile_changed = true;
    }
    while (map.offset_x >= TILE_SIZE)
    {
        map.tile_x++;
        map.offset_x -= TILE_SIZE;
        tile_changed = true;
    }
    while (map.offset_y < 0)
    {
        map.tile_y--;
        map.offset_y += TILE_SIZE;
        tile_changed = true;
    }
    while (map.offset_y >= TILE_SIZE)
    {
        map.tile_y++;
        map.offset_y -= TILE_SIZE;
        tile_changed = true;
    }

    if (tile_changed)
    {
        for (int y = 0; y < CACHE_H; y++)
            for (int x = 0; x < CACHE_W; x++)
                cache_tile_x[x][y] = -999999;
    }
}

static void draw_tile_from_cache(int cx, int cy, int pos_x, int pos_y)
{
    uint16_t *tile_buf = tile_cache[cx][cy];
    if (!tile_buf)
        return;

    for (int ty = 0; ty < TILE_SIZE; ty++)
    {
        int dst_y = pos_y + ty;
        if (dst_y < 0 || dst_y >= MAP_HEIGHT)
            continue;

        int start_x = pos_x < 0 ? 0 : pos_x;
        int end_x = (pos_x + TILE_SIZE > MAP_WIDTH) ? MAP_WIDTH : pos_x + TILE_SIZE;

        int copy_width = end_x - start_x;
        if (copy_width <= 0)
            continue;

        int src_offset = start_x - pos_x;
        int dst_idx = dst_y * MAP_WIDTH + start_x;
        int src_idx = ty * TILE_SIZE + src_offset;

        memcpy(&canvas_buf[dst_idx],
               &tile_buf[src_idx],
               copy_width * sizeof(uint16_t));
    }
}

void map_redraw(void)
{
    if (!canvas_buf || !canvas)
        return;

    int cam_x = map.tile_x * TILE_SIZE + map.offset_x;
    int cam_y = map.tile_y * TILE_SIZE + map.offset_y;

    int first_tile_x = cam_x / TILE_SIZE;
    int first_tile_y = cam_y / TILE_SIZE;

    for (int ty = 0; ty < CACHE_H; ty++)
    {
        for (int tx = 0; tx < CACHE_W; tx++)
        {
            int world_x = first_tile_x + tx;
            int world_y = first_tile_y + ty;

            ensure_tile_cached(tx, ty, world_x, world_y);

            int tile_world_x = world_x * TILE_SIZE;
            int tile_world_y = world_y * TILE_SIZE;

            int draw_x = tile_world_x - cam_x;
            int draw_y = tile_world_y - cam_y;

            draw_tile_from_cache(tx, ty, draw_x, draw_y);
        }
    }

    lv_obj_invalidate(canvas);
}

void map_move_left(void)
{
    map.offset_x -= MOVE_STEP;
    normalize_position();
    map_redraw();
}

void map_move_right(void)
{
    map.offset_x += MOVE_STEP;
    normalize_position();
    map_redraw();
}

void map_move_up(void)
{
    map.offset_y -= MOVE_STEP;
    normalize_position();
    map_redraw();
}

void map_move_down(void)
{
    map.offset_y += MOVE_STEP;
    normalize_position();
    map_redraw();
}

void map_view_create(lv_obj_t *parent, int zoom,
                     int start_tile_x, int start_tile_y)
{
    map.zoom = zoom;
    map.tile_x = start_tile_x;
    map.tile_y = start_tile_y;
    map.offset_x = 0;
    map.offset_y = 0;

    canvas_buf = heap_caps_malloc(
        MAP_WIDTH * MAP_HEIGHT * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM);

    if (!canvas_buf)
        return;

    for (int y = 0; y < CACHE_H; y++)
    {
        for (int x = 0; x < CACHE_W; x++)
        {
            tile_cache[x][y] =
                heap_caps_malloc(TILE_BYTES,
                                 MALLOC_CAP_SPIRAM);

            cache_tile_x[x][y] = -999999;
            cache_tile_y[x][y] = -999999;
        }
    }

    canvas = lv_canvas_create(parent);

    lv_canvas_set_buffer(canvas,
                         canvas_buf,
                         MAP_WIDTH,
                         MAP_HEIGHT,
                         LV_COLOR_FORMAT_RGB565);

    lv_obj_set_size(canvas, MAP_WIDTH, MAP_HEIGHT);
    lv_obj_center(canvas);

    map_redraw();
}