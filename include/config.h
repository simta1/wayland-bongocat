#ifndef CONFIG_H
#define CONFIG_H

#include "bongocat.h"
#include "error.h"

typedef struct {
    int screen_width;
    int bar_height;
    const char *asset_paths[NUM_FRAMES];
    const char *keyboard_device;
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
} config_t;

bongocat_error_t load_config(config_t *config);
void config_cleanup(void);
int get_screen_width(void);

#endif // CONFIG_H