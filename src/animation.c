#define _POSIX_C_SOURCE 199309L
#define STB_IMAGE_IMPLEMENTATION
#include "animation.h"
#include "wayland.h"
#include "input.h"
#include "memory.h"
#include "embedded_assets.h"
#include <time.h>

// Animation globals
unsigned char *anim_imgs[NUM_FRAMES];
int anim_width[NUM_FRAMES], anim_height[NUM_FRAMES];
int anim_index = 0;
pthread_mutex_t anim_lock = PTHREAD_MUTEX_INITIALIZER;

static config_t *current_config;
static pthread_t anim_thread;

void blit_image_scaled(uint8_t *dest, int dest_w, int dest_h,
                       unsigned char *src, int src_w, int src_h,
                       int offset_x, int offset_y, int target_w, int target_h) {
    for (int y = 0; y < target_h; y++) {
        for (int x = 0; x < target_w; x++) {
            int dx = x + offset_x;
            int dy = y + offset_y;
            if (dx < 0 || dy < 0 || dx >= dest_w || dy >= dest_h)
                continue;

            // Map destination pixel to source pixel
            int sx = (x * src_w) / target_w;
            int sy = (y * src_h) / target_h;

            int di = (dy * dest_w + dx) * 4;
            int si = (sy * src_w + sx) * 4;

            // Only draw non-transparent pixels
            if (src[si + 3] > 128) {
                dest[di + 0] = src[si + 2]; // B
                dest[di + 1] = src[si + 1]; // G
                dest[di + 2] = src[si + 0]; // R
                dest[di + 3] = src[si + 3]; // A
            }
        }
    }
}

void draw_rect(uint8_t *dest, int width, int height, int x, int y, int w, int h, 
               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (i < 0 || j < 0 || i >= width || j >= height)
                continue;
            int idx = (j * width + i) * 4;
            dest[idx + 0] = b;
            dest[idx + 1] = g;
            dest[idx + 2] = r;
            dest[idx + 3] = a;
        }
    }
}

static void *animate(void *arg __attribute__((unused))) {
    // Calculate frame time based on configured FPS
    long frame_time_ns = 1000000000L / current_config->fps;
    struct timespec ts = {0, frame_time_ns};
    
    long hold_until = 0;
    int test_counter = 0;
    int test_interval_frames = current_config->test_animation_interval * current_config->fps;

    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        long now_us = now.tv_sec * 1000000 + now.tv_usec;

        pthread_mutex_lock(&anim_lock);

        // Test animation based on configured interval
        if (current_config->test_animation_interval > 0) {
            test_counter++;
            if (test_counter > test_interval_frames) {
                if (current_config->enable_debug) {
                    printf("Test animation trigger\n");
                }
                anim_index = (rand() % 2) + 1;
                hold_until = now_us + (current_config->test_animation_duration * 1000);
                test_counter = 0;
            }
        }

        if (*any_key_pressed) {
            if (current_config->enable_debug) {
                printf("Key pressed! Switching to frame %d\n", (rand() % 2) + 1);
            }
            anim_index = (rand() % 2) + 1;
            hold_until = now_us + (current_config->keypress_duration * 1000);
            *any_key_pressed = 0;
            test_counter = 0; // Reset test counter
        } else if (now_us > hold_until) {
            if (anim_index != current_config->idle_frame) {
                if (current_config->enable_debug) {
                    printf("Returning to idle frame %d\n", current_config->idle_frame);
                }
                anim_index = current_config->idle_frame;
            }
        }
        
        pthread_mutex_unlock(&anim_lock);

        draw_bar();
        nanosleep(&ts, NULL);
    }
    
    return NULL;
}

bongocat_error_t animation_init(config_t *config) {
    BONGOCAT_CHECK_NULL(config, BONGOCAT_ERROR_INVALID_PARAM);
    
    current_config = config;
    
    bongocat_log_info("Initializing animation system");
    
    // Load embedded animation frames
    const unsigned char *embedded_data[] = {
        bongo_cat_both_up_png,
        bongo_cat_left_down_png,
        bongo_cat_right_down_png
    };
    
    const size_t embedded_sizes[] = {
        bongo_cat_both_up_png_size,
        bongo_cat_left_down_png_size,
        bongo_cat_right_down_png_size
    };
    
    const char *frame_names[] = {
        "embedded bongo-cat-both-up.png",
        "embedded bongo-cat-left-down.png", 
        "embedded bongo-cat-right-down.png"
    };
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        bongocat_log_debug("Loading embedded image: %s", frame_names[i]);
        
        anim_imgs[i] = stbi_load_from_memory(embedded_data[i], embedded_sizes[i], 
                                           &anim_width[i], &anim_height[i], NULL, 4);
        if (!anim_imgs[i]) {
            bongocat_log_error("Failed to load embedded image: %s", frame_names[i]);
            
            // Cleanup already loaded images
            for (int j = 0; j < i; j++) {
                if (anim_imgs[j]) {
                    stbi_image_free(anim_imgs[j]);
                    anim_imgs[j] = NULL;
                }
            }
            return BONGOCAT_ERROR_FILE_IO;
        }
        
        bongocat_log_debug("Loaded %dx%d embedded image", anim_width[i], anim_height[i]);
    }
    
    bongocat_log_info("Animation system initialized successfully with embedded assets");
    return BONGOCAT_SUCCESS;
}

bongocat_error_t animation_start(void) {
    bongocat_log_info("Starting animation thread");
    
    int result = pthread_create(&anim_thread, NULL, animate, NULL);
    if (result != 0) {
        bongocat_log_error("Failed to create animation thread: %s", strerror(result));
        return BONGOCAT_ERROR_THREAD;
    }
    
    bongocat_log_debug("Animation thread started successfully");
    return BONGOCAT_SUCCESS;
}

void animation_cleanup(void) {
    pthread_cancel(anim_thread);
    pthread_join(anim_thread, NULL);
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (anim_imgs[i]) {
            stbi_image_free(anim_imgs[i]);
        }
    }
    
    pthread_mutex_destroy(&anim_lock);
}

void animation_trigger(void) {
    *any_key_pressed = 1;
}