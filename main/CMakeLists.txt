idf_component_register(SRCS "main.c" "st7789.c" "map_view.c"
                    INCLUDE_DIRS ".")

#spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)
