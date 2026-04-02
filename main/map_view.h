#ifndef MAP_VIEW_H
#define MAP_VIEW_H

#include "lvgl.h"
#include <stdbool.h>

// Inicializa o mapa
void map_view_create(lv_obj_t *parent,
                     int zoom,
                     int start_tile_x,
                     int start_tile_y);

void map_move_left(void);
void map_move_right(void);
void map_move_up(void);
void map_move_down(void);

void map_redraw(void);

#endif