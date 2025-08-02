#ifndef CONFIG_H
#define CONFIG_H

#include "core/bongocat.h"
#include "utils/error.h"

typedef enum {
    POSITION_TOP = 0,
    POSITION_BOTTOM = 1
} overlay_position_t;

typedef enum {
    LAYER_TOP = 0,
    LAYER_OVERLAY = 1
} layer_type_t;

typedef struct {
    int screen_width;
    int bar_height;
    const char *asset_paths[NUM_FRAMES];
    char **keyboard_devices;
    int num_keyboard_devices;
    int cat_x_offset;
    int cat_y_offset;
    int cat_height;
    int overlay_height;
    int idle_frame;
    int keypress_duration;
    int test_animation_duration;
    int test_animation_interval;
    int fps;
    int overlay_opacity;
    int enable_debug;
    int hide_on_fullscreen;
    layer_type_t layer;
    overlay_position_t overlay_position;
} config_t;

bongocat_error_t load_config(config_t *config, const char *config_file_path);
void config_cleanup(void);
int get_screen_width(void);

#endif // CONFIG_H