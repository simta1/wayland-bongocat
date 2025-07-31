#define _POSIX_C_SOURCE 200809L
#include "wayland.h"
#include "animation.h"

// Wayland globals
bool configured = false;
struct wl_display *display;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct zwlr_layer_shell_v1 *layer_shell;
struct wl_surface *surface;
struct wl_buffer *buffer;
struct zwlr_layer_surface_v1 *layer_surface;
uint8_t *pixels;

static config_t *current_config;

int create_shm(int size) {
  char name[] = "/bar-shm-XXXXXX";
  int fd;

  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 6; j++) {
      name[9 + j] = 'A' + (rand() % 26);
    }
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
      shm_unlink(name);
      break;
    }
  }

  if (fd < 0 || ftruncate(fd, size) < 0) {
    perror("shm");
    exit(1);
  }

  return fd;
}

void draw_bar(void) {
  if (!configured) {
    bongocat_log_debug("Surface not configured yet, skipping draw");
    return;
  }

  // Clear entire buffer with configurable transparency
  for (int i = 0;
       i < current_config->screen_width * current_config->bar_height * 4;
       i += 4) {
    pixels[i] = 0;     // B
    pixels[i + 1] = 0; // G
    pixels[i + 2] = 0; // R
    pixels[i + 3] =
        current_config->overlay_opacity; // A (configurable transparency)
  }

  pthread_mutex_lock(&anim_lock);
  // Use configured cat height
  int cat_height = current_config->cat_height;
  int cat_width = (cat_height * 779) / 320; // Maintain 954:393 ratio
  int cat_x = (current_config->screen_width - cat_width) / 2 +
              current_config->cat_x_offset;
  int cat_y = (current_config->bar_height - cat_height) / 2 +
              current_config->cat_y_offset;

  blit_image_scaled(pixels, current_config->screen_width,
                    current_config->bar_height, anim_imgs[anim_index],
                    anim_width[anim_index], anim_height[anim_index], cat_x,
                    cat_y, cat_width, cat_height);
  pthread_mutex_unlock(&anim_lock);

  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage_buffer(surface, 0, 0, current_config->screen_width,
                           current_config->bar_height);
  wl_surface_commit(surface);
  wl_display_flush(display);
}

static void layer_surface_configure(void *data __attribute__((unused)),
                                    struct zwlr_layer_surface_v1 *ls,
                                    uint32_t serial, uint32_t w, uint32_t h) {
  bongocat_log_debug("Layer surface configured: %dx%d", w, h);
  zwlr_layer_surface_v1_ack_configure(ls, serial);
  configured = true;
  draw_bar();
}

static struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_surface_configure,
    .closed = NULL,
};

static void registry_global(void *data __attribute__((unused)),
                            struct wl_registry *reg, uint32_t name,
                            const char *iface,
                            uint32_t ver __attribute__((unused))) {
  if (strcmp(iface, wl_compositor_interface.name) == 0) {
    compositor = (struct wl_compositor *)wl_registry_bind(
        reg, name, &wl_compositor_interface, 4);
  } else if (strcmp(iface, wl_shm_interface.name) == 0) {
    shm = (struct wl_shm *)wl_registry_bind(reg, name, &wl_shm_interface, 1);
  } else if (strcmp(iface, zwlr_layer_shell_v1_interface.name) == 0) {
    layer_shell = (struct zwlr_layer_shell_v1 *)wl_registry_bind(
        reg, name, &zwlr_layer_shell_v1_interface, 1);
  }
}

static void registry_remove(void *data __attribute__((unused)),
                            struct wl_registry *registry
                            __attribute__((unused)),
                            uint32_t name __attribute__((unused))) {}

static struct wl_registry_listener reg_listener = {
    .global = registry_global, .global_remove = registry_remove};

bongocat_error_t wayland_init(config_t *config) {
  BONGOCAT_CHECK_NULL(config, BONGOCAT_ERROR_INVALID_PARAM);

  current_config = config;

  bongocat_log_info("Initializing Wayland connection");

  display = wl_display_connect(NULL);
  if (!display) {
    bongocat_log_error("Failed to connect to Wayland display");
    return BONGOCAT_ERROR_WAYLAND;
  }
  bongocat_log_debug("Connected to Wayland display");

  struct wl_registry *registry = wl_display_get_registry(display);
  if (!registry) {
    bongocat_log_error("Failed to get Wayland registry");
    wl_display_disconnect(display);
    display = NULL;
    return BONGOCAT_ERROR_WAYLAND;
  }

  wl_registry_add_listener(registry, &reg_listener, NULL);
  wl_display_roundtrip(display);

  if (!compositor || !shm || !layer_shell) {
    bongocat_log_error("Missing required Wayland protocols (compositor=%p, "
                       "shm=%p, layer_shell=%p)",
                       (void *)compositor, (void *)shm, (void *)layer_shell);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    display = NULL;
    return BONGOCAT_ERROR_WAYLAND;
  }
  bongocat_log_debug("Got required Wayland protocols");

  surface = wl_compositor_create_surface(compositor);
  if (!surface) {
    bongocat_log_error("Failed to create Wayland surface");
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    display = NULL;
    return BONGOCAT_ERROR_WAYLAND;
  }

  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
      "bongocat-overlay");
  if (!layer_surface) {
    bongocat_log_error("Failed to create layer surface");
    wl_surface_destroy(surface);
    surface = NULL;
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    display = NULL;
    return BONGOCAT_ERROR_WAYLAND;
  }

  // Set anchor based on configured position
  uint32_t anchor =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  if (config->overlay_position == POSITION_TOP) {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    bongocat_log_debug("Setting overlay position to top");
  } else {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    bongocat_log_debug("Setting overlay position to bottom");
  }
  zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
  zwlr_layer_surface_v1_set_size(layer_surface, 0, config->bar_height);
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
  zwlr_layer_surface_v1_set_margin(layer_surface, 0, 0, 0, 0);

  // Make the overlay click-through by setting keyboard interactivity to none
  zwlr_layer_surface_v1_set_keyboard_interactivity(
      layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_listener, NULL);

  // Create an empty input region to make the surface click-through
  struct wl_region *input_region = wl_compositor_create_region(compositor);
  if (input_region) {
    // Don't add any rectangles to the region, keeping it empty
    wl_surface_set_input_region(surface, input_region);
    wl_region_destroy(input_region);
    bongocat_log_debug(
        "Set empty input region for click-through functionality");
  } else {
    bongocat_log_warning("Failed to create input region for click-through");
  }

  wl_surface_commit(surface);

  // Create shared memory buffer
  int size = config->screen_width * config->bar_height * 4;
  if (size <= 0) {
    bongocat_log_error("Invalid buffer size: %d", size);
    return BONGOCAT_ERROR_WAYLAND;
  }

  int fd = create_shm(size);
  if (fd < 0) {
    bongocat_log_error("Failed to create shared memory");
    return BONGOCAT_ERROR_WAYLAND;
  }

  pixels =
      (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels == MAP_FAILED) {
    bongocat_log_error("Failed to map shared memory: %s", strerror(errno));
    close(fd);
    return BONGOCAT_ERROR_MEMORY;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
  if (!pool) {
    bongocat_log_error("Failed to create shared memory pool");
    munmap(pixels, size);
    pixels = NULL;
    close(fd);
    return BONGOCAT_ERROR_WAYLAND;
  }

  buffer = wl_shm_pool_create_buffer(
      pool, 0, config->screen_width, config->bar_height,
      config->screen_width * 4, WL_SHM_FORMAT_ARGB8888);
  if (!buffer) {
    bongocat_log_error("Failed to create buffer");
    wl_shm_pool_destroy(pool);
    munmap(pixels, size);
    pixels = NULL;
    close(fd);
    return BONGOCAT_ERROR_WAYLAND;
  }

  wl_shm_pool_destroy(pool);
  close(fd);
  wl_registry_destroy(registry);

  bongocat_log_info("Wayland initialization complete (%dx%d buffer)",
                    config->screen_width, config->bar_height);
  return BONGOCAT_SUCCESS;
}

bongocat_error_t wayland_run(volatile sig_atomic_t *running) {
  BONGOCAT_CHECK_NULL(running, BONGOCAT_ERROR_INVALID_PARAM);

  bongocat_log_info("Starting Wayland event loop");

  while (*running && display) {
    int ret = wl_display_dispatch(display);
    if (ret == -1) {
      int err = wl_display_get_error(display);
      if (err == EPROTO) {
        bongocat_log_error("Wayland protocol error");
        return BONGOCAT_ERROR_WAYLAND;
      } else if (err == EPIPE) {
        bongocat_log_warning("Wayland display connection lost");
        return BONGOCAT_SUCCESS; // Graceful shutdown
      } else {
        bongocat_log_error("Wayland dispatch error: %s", strerror(err));
        return BONGOCAT_ERROR_WAYLAND;
      }
    }

    // Flush any pending requests
    if (wl_display_flush(display) == -1) {
      bongocat_log_warning("Failed to flush Wayland display");
    }
  }

  bongocat_log_info("Wayland event loop exited");
  return BONGOCAT_SUCCESS;
}

void wayland_cleanup(void) {
  bongocat_log_info("Cleaning up Wayland resources");

  if (buffer) {
    wl_buffer_destroy(buffer);
    buffer = NULL;
  }

  if (pixels) {
    // Calculate buffer size for unmapping
    if (current_config) {
      int size = current_config->screen_width * current_config->bar_height * 4;
      munmap(pixels, size);
    }
    pixels = NULL;
  }

  if (layer_surface) {
    zwlr_layer_surface_v1_destroy(layer_surface);
    layer_surface = NULL;
  }

  if (surface) {
    wl_surface_destroy(surface);
    surface = NULL;
  }

  if (layer_shell) {
    zwlr_layer_shell_v1_destroy(layer_shell);
    layer_shell = NULL;
  }

  if (shm) {
    wl_shm_destroy(shm);
    shm = NULL;
  }

  if (compositor) {
    wl_compositor_destroy(compositor);
    compositor = NULL;
  }

  if (display) {
    wl_display_disconnect(display);
    display = NULL;
  }

  configured = false;
  bongocat_log_debug("Wayland cleanup complete");
}

void wayland_update_config(config_t *config) {
  if (!config) {
    bongocat_log_error("Cannot update wayland config: config is NULL");
    return;
  }

  bongocat_log_info("Updating wayland config");
  current_config = config;

  // Trigger a redraw with the new config
  if (configured) {
    draw_bar();
    bongocat_log_info("Wayland config updated and redrawn");
  }
}
