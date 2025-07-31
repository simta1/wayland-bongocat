#ifndef ANIMATION_H
#define ANIMATION_H

#include "bongocat.h"
#include "config.h"
#include "error.h"

extern unsigned char *anim_imgs[NUM_FRAMES];
extern int anim_width[NUM_FRAMES], anim_height[NUM_FRAMES];
extern int anim_index;
extern pthread_mutex_t anim_lock;

bongocat_error_t animation_init(config_t *config);
bongocat_error_t animation_start(void);
void animation_cleanup(void);
void animation_trigger(void);

void blit_image_scaled(uint8_t *dest, int dest_w, int dest_h,
                       unsigned char *src, int src_w, int src_h, int offset_x,
                       int offset_y, int target_w, int target_h);

void draw_rect(uint8_t *dest, int width, int height, int x, int y, int w, int h,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif // ANIMATION_H