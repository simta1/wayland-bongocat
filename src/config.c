#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "error.h"
#include "memory.h"
#include <limits.h>

// Configuration validation ranges
#define MIN_CAT_HEIGHT 10
#define MAX_CAT_HEIGHT 200
#define MIN_OVERLAY_HEIGHT 20
#define MAX_OVERLAY_HEIGHT 300
#define MIN_FPS 1
#define MAX_FPS 120
#define MIN_DURATION 10
#define MAX_DURATION 5000
#define MAX_INTERVAL 3600

static char *config_keyboard_device = NULL;

static bongocat_error_t validate_config(config_t *config) {
    BONGOCAT_CHECK_NULL(config, BONGOCAT_ERROR_INVALID_PARAM);
    
    // Validate cat dimensions
    if (config->cat_height < MIN_CAT_HEIGHT || config->cat_height > MAX_CAT_HEIGHT) {
        bongocat_log_warning("cat_height %d out of range [%d-%d], clamping", 
                           config->cat_height, MIN_CAT_HEIGHT, MAX_CAT_HEIGHT);
        config->cat_height = config->cat_height < MIN_CAT_HEIGHT ? MIN_CAT_HEIGHT : MAX_CAT_HEIGHT;
    }
    
    if (config->overlay_height < MIN_OVERLAY_HEIGHT || config->overlay_height > MAX_OVERLAY_HEIGHT) {
        bongocat_log_warning("overlay_height %d out of range [%d-%d], clamping", 
                           config->overlay_height, MIN_OVERLAY_HEIGHT, MAX_OVERLAY_HEIGHT);
        config->overlay_height = config->overlay_height < MIN_OVERLAY_HEIGHT ? MIN_OVERLAY_HEIGHT : MAX_OVERLAY_HEIGHT;
    }
    
    // Validate FPS
    if (config->fps < MIN_FPS || config->fps > MAX_FPS) {
        bongocat_log_warning("fps %d out of range [%d-%d], clamping", 
                           config->fps, MIN_FPS, MAX_FPS);
        config->fps = config->fps < MIN_FPS ? MIN_FPS : MAX_FPS;
    }
    
    // Validate durations
    if (config->keypress_duration < MIN_DURATION || config->keypress_duration > MAX_DURATION) {
        bongocat_log_warning("keypress_duration %d out of range [%d-%d], clamping", 
                           config->keypress_duration, MIN_DURATION, MAX_DURATION);
        config->keypress_duration = config->keypress_duration < MIN_DURATION ? MIN_DURATION : MAX_DURATION;
    }
    
    if (config->test_animation_duration < MIN_DURATION || config->test_animation_duration > MAX_DURATION) {
        bongocat_log_warning("test_animation_duration %d out of range [%d-%d], clamping", 
                           config->test_animation_duration, MIN_DURATION, MAX_DURATION);
        config->test_animation_duration = config->test_animation_duration < MIN_DURATION ? MIN_DURATION : MAX_DURATION;
    }
    
    // Validate interval (0 is allowed to disable)
    if (config->test_animation_interval < 0 || config->test_animation_interval > MAX_INTERVAL) {
        bongocat_log_warning("test_animation_interval %d out of range [0-%d], clamping", 
                           config->test_animation_interval, MAX_INTERVAL);
        config->test_animation_interval = config->test_animation_interval < 0 ? 0 : MAX_INTERVAL;
    }
    
    // Validate opacity
    if (config->overlay_opacity < 0 || config->overlay_opacity > 255) {
        bongocat_log_warning("overlay_opacity %d out of range [0-255], clamping", config->overlay_opacity);
        config->overlay_opacity = config->overlay_opacity < 0 ? 0 : 255;
    }
    
    // Validate idle frame
    if (config->idle_frame < 0 || config->idle_frame >= NUM_FRAMES) {
        bongocat_log_warning("idle_frame %d out of range [0-%d], resetting to 0", 
                           config->idle_frame, NUM_FRAMES - 1);
        config->idle_frame = 0;
    }
    
    // Validate enable_debug
    config->enable_debug = config->enable_debug ? 1 : 0;
    
    // Validate cat positioning doesn't go off-screen
    if (abs(config->cat_x_offset) > config->screen_width) {
        bongocat_log_warning("cat_x_offset %d may position cat off-screen (screen width: %d)", 
                           config->cat_x_offset, config->screen_width);
    }
    
    return BONGOCAT_SUCCESS;
}

static bongocat_error_t parse_config_file(config_t *config, const char *config_file_path) {
    BONGOCAT_CHECK_NULL(config, BONGOCAT_ERROR_INVALID_PARAM);
    
    const char *file_path = config_file_path ? config_file_path : "bongocat.conf";
    
    FILE *file = fopen(file_path, "r");
    if (!file) {
        bongocat_log_info("Config file '%s' not found, using defaults", file_path);
        return BONGOCAT_SUCCESS;
    }
    
    char line[512];
    char key[256], value[256];
    int line_number = 0;
    bongocat_error_t result = BONGOCAT_SUCCESS;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0' || strspn(line, " \t") == strlen(line)) {
            continue;
        }
        
        // Parse key=value pairs
        if (sscanf(line, " %255[^=] = %255s", key, value) == 2) {
            // Trim whitespace from key
            char *key_start = key;
            while (*key_start == ' ' || *key_start == '\t') key_start++;
            char *key_end = key_start + strlen(key_start) - 1;
            while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }
            
            if (strcmp(key_start, "cat_x_offset") == 0) {
                config->cat_x_offset = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "cat_y_offset") == 0) {
                config->cat_y_offset = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "cat_height") == 0) {
                config->cat_height = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "overlay_height") == 0) {
                config->overlay_height = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "idle_frame") == 0) {
                config->idle_frame = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "keypress_duration") == 0) {
                config->keypress_duration = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "test_animation_duration") == 0) {
                config->test_animation_duration = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "test_animation_interval") == 0) {
                config->test_animation_interval = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "fps") == 0) {
                config->fps = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "overlay_opacity") == 0) {
                config->overlay_opacity = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "enable_debug") == 0) {
                config->enable_debug = (int)strtol(value, NULL, 10);
            } else if (strcmp(key_start, "keyboard_device") == 0) {
                BONGOCAT_SAFE_FREE(config_keyboard_device);
                size_t value_len = strlen(value);
                config_keyboard_device = BONGOCAT_MALLOC(value_len + 1);
                if (config_keyboard_device) {
                    strncpy(config_keyboard_device, value, value_len);
                    config_keyboard_device[value_len] = '\0';
                    config->keyboard_device = config_keyboard_device;
                } else {
                    bongocat_log_error("Failed to allocate memory for keyboard_device");
                    result = BONGOCAT_ERROR_MEMORY;
                }
            } else {
                bongocat_log_warning("Unknown configuration key '%s' at line %d", key_start, line_number);
            }
        } else if (strlen(line) > 0) {
            bongocat_log_warning("Invalid configuration line %d: %s", line_number, line);
        }
    }
    
    fclose(file);
    
    if (result == BONGOCAT_SUCCESS) {
        bongocat_log_info("Loaded configuration from bongocat.conf");
    }
    
    return result;
}

bongocat_error_t load_config(config_t *config, const char *config_file_path) {
    BONGOCAT_CHECK_NULL(config, BONGOCAT_ERROR_INVALID_PARAM);
    
    // Initialize with defaults
    *config = (config_t) {
        .screen_width = get_screen_width(),
        .bar_height = DEFAULT_BAR_HEIGHT,
        .asset_paths = {
            "assets/bongo-cat-both-up.png",
            "assets/bongo-cat-left-down.png", 
            "assets/bongo-cat-right-down.png"
        },
        .keyboard_device = "/dev/input/event4",
        .cat_x_offset = 100,  // Default values from current config
        .cat_y_offset = 10,
        .cat_height = 40,
        .overlay_height = 50,
        .idle_frame = 0,
        .keypress_duration = 100,
        .test_animation_duration = 200,
        .test_animation_interval = 3,
        .fps = 60,
        .overlay_opacity = 150,
        .enable_debug = 1
    };
    
    // Parse config file and override defaults
    bongocat_error_t result = parse_config_file(config, config_file_path);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to parse configuration file: %s", bongocat_error_string(result));
        return result;
    }
    
    // Validate and sanitize configuration
    result = validate_config(config);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Configuration validation failed: %s", bongocat_error_string(result));
        return result;
    }
    
    // Update bar_height from config
    config->bar_height = config->overlay_height;
    
    // Initialize error system with debug setting
    bongocat_error_init(config->enable_debug);
    
    bongocat_log_debug("Configuration loaded successfully");
    bongocat_log_debug("  Screen: %dx%d", config->screen_width, config->bar_height);
    bongocat_log_debug("  Cat: %dx%d at offset (%d,%d)", 
                      config->cat_height, (config->cat_height * 954) / 393,
                      config->cat_x_offset, config->cat_y_offset);
    bongocat_log_debug("  FPS: %d, Opacity: %d", config->fps, config->overlay_opacity);
    
    return BONGOCAT_SUCCESS;
}

void config_cleanup(void) {
    BONGOCAT_SAFE_FREE(config_keyboard_device);
}

int get_screen_width(void) {
    FILE *fp = popen("/usr/bin/hyprctl monitors active", "r");
    if (!fp) {
        return DEFAULT_SCREEN_WIDTH;
    }

    char buf[1000];
    if (!fgets(buf, sizeof(buf), fp)) { // skip first line
        pclose(fp);
        return DEFAULT_SCREEN_WIDTH;
    }
    if (fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        strtok(buf, "x");
        return atoi(buf);
    }
    
    pclose(fp);
    return DEFAULT_SCREEN_WIDTH;
}