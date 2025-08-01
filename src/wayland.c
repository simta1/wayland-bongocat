#define _POSIX_C_SOURCE 200809L
#include "wayland.h"
#include "animation.h"
#include <poll.h>
#include <sys/time.h>

// Wayland globals
bool configured = false;
bool fullscreen_detected = false;
struct wl_display *display;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct zwlr_layer_shell_v1 *layer_shell;
struct xdg_wm_base *xdg_wm_base;
struct wl_output *output;
struct wl_surface *surface;
struct wl_buffer *buffer;
struct zwlr_layer_surface_v1 *layer_surface;
uint8_t *pixels;

// Screen dimensions from Wayland output
static int wayland_screen_width = 0;
static int wayland_screen_height = 0;
static int wayland_transform = WL_OUTPUT_TRANSFORM_NORMAL;
static int wayland_raw_width = 0;
static int wayland_raw_height = 0;
static bool wayland_mode_received = false;
static bool wayland_geometry_received = false;

static config_t *current_config;

// Function to check if any window is fullscreen (compositor-specific)
static bool check_fullscreen_status(void) {
    // Only check for fullscreen when using OVERLAY layer with fullscreen detection enabled
    if (current_config->layer != LAYER_OVERLAY || !current_config->hide_on_fullscreen) {
        bongocat_log_debug("Fullscreen detection disabled (layer=%s, hide_on_fullscreen=%d)", 
                          current_config->layer == LAYER_OVERLAY ? "overlay" : "top",
                          current_config->hide_on_fullscreen);
        return false;
    }
    
    // Try Hyprland first
    FILE *fp = popen("hyprctl activewindow 2>/dev/null", "r");
    if (fp) {
        char line[512];
        bool is_fullscreen = false;
        
        bongocat_log_debug("Checking Hyprland for fullscreen status");
        while (fgets(line, sizeof(line), fp)) {
            // Remove newline for cleaner logging
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
            }
            
            bongocat_log_debug("Hyprctl line: %s", line);
            // Check for any fullscreen mode (1 = real fullscreen, 2 = fake fullscreen)
            if (strstr(line, "fullscreen: 1") || strstr(line, "fullscreen: 2") || strstr(line, "fullscreen: true")) {
                is_fullscreen = true;
                bongocat_log_debug("Fullscreen detected in Hyprland");
                break;
            }
        }
        pclose(fp);
        bongocat_log_debug("Hyprland fullscreen check result: %s", is_fullscreen ? "true" : "false");
        return is_fullscreen;
    }
    
    // Try sway as fallback
    bongocat_log_debug("Hyprland not available, trying Sway");
    fp = popen("swaymsg -t get_tree 2>/dev/null", "r");
    if (fp) {
        char sway_buffer[4096];
        bool is_fullscreen = false;
        
        while (fgets(sway_buffer, sizeof(sway_buffer), fp)) {
            if (strstr(sway_buffer, "\"fullscreen_mode\":1")) {
                is_fullscreen = true;
                bongocat_log_debug("Fullscreen detected in Sway");
                break;
            }
        }
        pclose(fp);
        bongocat_log_debug("Sway fullscreen check result: %s", is_fullscreen ? "true" : "false");
        return is_fullscreen;
    }
    
    // If no compositor detection works, assume not fullscreen
    bongocat_log_debug("No supported compositor found for fullscreen detection");
    return false;
}

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

    // Determine effective opacity based on fullscreen detection (only for OVERLAY layer)
    int effective_opacity = current_config->overlay_opacity;
    bool should_hide_overlay = (current_config->layer == LAYER_OVERLAY && 
                               current_config->hide_on_fullscreen && 
                               fullscreen_detected);
    
    if (should_hide_overlay) {
        effective_opacity = 0;
        bongocat_log_debug("Fullscreen detected, hiding overlay (opacity=0)");
    }

    // Clear entire buffer with configurable transparency
    for (int i = 0; i < current_config->screen_width * current_config->bar_height * 4; i += 4) {
        pixels[i] = 0;       // B
        pixels[i + 1] = 0;   // G
        pixels[i + 2] = 0;   // R
        pixels[i + 3] = effective_opacity; // A (configurable transparency, 0 when fullscreen)
    }

    // Only draw the cat if not in fullscreen mode (only applies to OVERLAY layer with fullscreen detection enabled)
    bool should_hide_cat = (current_config->layer == LAYER_OVERLAY && 
                           current_config->hide_on_fullscreen && 
                           fullscreen_detected);
    
    if (!should_hide_cat) {
        pthread_mutex_lock(&anim_lock);
        // Use configured cat height
        int cat_height = current_config->cat_height;
        int cat_width = (cat_height * 779) / 320;  // Maintain 954:393 ratio
        int cat_x = (current_config->screen_width - cat_width) / 2 + current_config->cat_x_offset;
        int cat_y = (current_config->bar_height - cat_height) / 2 + current_config->cat_y_offset;

        blit_image_scaled(pixels, current_config->screen_width, current_config->bar_height,
                          anim_imgs[anim_index], anim_width[anim_index], anim_height[anim_index],
                          cat_x, cat_y, cat_width, cat_height);
        pthread_mutex_unlock(&anim_lock);
        bongocat_log_debug("Cat drawn (fullscreen_detected=%s)", fullscreen_detected ? "true" : "false");
    } else {
        bongocat_log_debug("Cat drawing skipped due to fullscreen detection");
    }

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, current_config->screen_width, current_config->bar_height);
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

<<<<<<< HEAD
// XDG shell handlers for fullscreen detection
static void xdg_wm_base_ping(void *data __attribute__((unused)), struct xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}

static struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
=======
static void calculate_effective_screen_dimensions(void) {
    if (!wayland_mode_received || !wayland_geometry_received) {
        return; // Wait for both mode and geometry
    }
    
    // Only calculate and log if we haven't already done so
    if (wayland_screen_width > 0) {
        return; // Already calculated
    }
    
    // Check if the display is rotated (90° or 270°)
    // In these cases, width and height are swapped from the user's perspective
    // WL_OUTPUT_TRANSFORM values:
    // 0 = normal, 1 = 90°, 2 = 180°, 3 = 270°
    // 4 = flipped, 5 = flipped+90°, 6 = flipped+180°, 7 = flipped+270°
    bool is_rotated = (wayland_transform == WL_OUTPUT_TRANSFORM_90 ||
                      wayland_transform == WL_OUTPUT_TRANSFORM_270 ||
                      wayland_transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
                      wayland_transform == WL_OUTPUT_TRANSFORM_FLIPPED_270);
    
    if (is_rotated) {
        // For rotated displays, swap width and height to get the effective screen width
        wayland_screen_width = wayland_raw_height;
        wayland_screen_height = wayland_raw_width;
        bongocat_log_info("Detected rotated screen dimensions: %dx%d (transform: %d)", 
                         wayland_raw_height, wayland_raw_width, wayland_transform);
    } else {
        wayland_screen_width = wayland_raw_width;
        wayland_screen_height = wayland_raw_height;
        bongocat_log_info("Detected screen dimensions: %dx%d (transform: %d)", 
                         wayland_raw_width, wayland_raw_height, wayland_transform);
    }
}

static void output_geometry(void *data __attribute__((unused)),
                           struct wl_output *wl_output __attribute__((unused)),
                           int32_t x __attribute__((unused)),
                           int32_t y __attribute__((unused)),
                           int32_t physical_width __attribute__((unused)),
                           int32_t physical_height __attribute__((unused)),
                           int32_t subpixel __attribute__((unused)),
                           const char *make __attribute__((unused)),
                           const char *model __attribute__((unused)),
                           int32_t transform) {
    wayland_transform = transform;
    wayland_geometry_received = true;
    bongocat_log_debug("Output transform: %d", transform);
    calculate_effective_screen_dimensions();
}

static void output_mode(void *data __attribute__((unused)),
                       struct wl_output *wl_output __attribute__((unused)),
                       uint32_t flags,
                       int32_t width,
                       int32_t height,
                       int32_t refresh __attribute__((unused))) {
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        wayland_raw_width = width;
        wayland_raw_height = height;
        wayland_mode_received = true;
        bongocat_log_debug("Received raw screen mode: %dx%d", width, height);
        calculate_effective_screen_dimensions();
    }
}

static void output_done(void *data __attribute__((unused)),
                       struct wl_output *wl_output __attribute__((unused))) {
    // Output configuration is complete - ensure we have calculated dimensions
    calculate_effective_screen_dimensions();
    bongocat_log_debug("Output configuration complete");
}

static void output_scale(void *data __attribute__((unused)),
                        struct wl_output *wl_output __attribute__((unused)),
                        int32_t factor __attribute__((unused))) {
    // We don't need scale info for screen width detection
}

static struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
>>>>>>> main
};

static void registry_global(void *data __attribute__((unused)), struct wl_registry *reg, uint32_t name,
                           const char *iface, uint32_t ver __attribute__((unused))) {
    if (strcmp(iface, wl_compositor_interface.name) == 0) {
        compositor = (struct wl_compositor *)wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    } else if (strcmp(iface, wl_shm_interface.name) == 0) {
        shm = (struct wl_shm *)wl_registry_bind(reg, name, &wl_shm_interface, 1);
    } else if (strcmp(iface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = (struct zwlr_layer_shell_v1 *)wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 1);
<<<<<<< HEAD
    } else if (strcmp(iface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        if (xdg_wm_base) {
            xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
=======
    } else if (strcmp(iface, wl_output_interface.name) == 0) {
        // Bind to the first output we find
        if (!output) {
            output = (struct wl_output *)wl_registry_bind(reg, name, &wl_output_interface, 2);
            wl_output_add_listener(output, &output_listener, NULL);
>>>>>>> main
        }
    }
}

static void registry_remove(void *data __attribute__((unused)),
                           struct wl_registry *registry __attribute__((unused)),
                           uint32_t name __attribute__((unused))) {}

static struct wl_registry_listener reg_listener = {
    .global = registry_global,
    .global_remove = registry_remove
};

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
        bongocat_log_error("Missing required Wayland protocols (compositor=%p, shm=%p, layer_shell=%p)",
                          (void*)compositor, (void*)shm, (void*)layer_shell);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        display = NULL;
        return BONGOCAT_ERROR_WAYLAND;
    }
    bongocat_log_debug("Got required Wayland protocols");

    // Wait for output information if we have an output
    if (output) {
        wl_display_roundtrip(display);
        if (wayland_screen_width > 0) {
            bongocat_log_info("Detected screen width from Wayland: %d", wayland_screen_width);
            config->screen_width = wayland_screen_width;
        } else {
            bongocat_log_warning("Failed to detect screen width from Wayland, using default: %d", DEFAULT_SCREEN_WIDTH);
            config->screen_width = DEFAULT_SCREEN_WIDTH;
        }
    } else {
        bongocat_log_warning("No Wayland output found, using default screen width: %d", DEFAULT_SCREEN_WIDTH);
        config->screen_width = DEFAULT_SCREEN_WIDTH;
    }

    surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        bongocat_log_error("Failed to create Wayland surface");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        display = NULL;
        return BONGOCAT_ERROR_WAYLAND;
    }

    // Use configurable layer - TOP for broader compatibility, OVERLAY for advanced compositors
    uint32_t layer_type = (config->layer == LAYER_OVERLAY) ? 
                         ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY : 
                         ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    
    layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, NULL,
                                                          layer_type, "bongocat-overlay");
    
    bongocat_log_debug("Using layer: %s", 
                      (config->layer == LAYER_OVERLAY) ? "OVERLAY" : "TOP");
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
    uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
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
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface,
                                                     ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_listener, NULL);

    // Create an empty input region to make the surface click-through
    struct wl_region *input_region = wl_compositor_create_region(compositor);
    if (input_region) {
        // Don't add any rectangles to the region, keeping it empty
        wl_surface_set_input_region(surface, input_region);
        wl_region_destroy(input_region);
        bongocat_log_debug("Set empty input region for click-through functionality");
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

    pixels = (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

    buffer = wl_shm_pool_create_buffer(pool, 0, config->screen_width, config->bar_height,
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
    
    struct timeval last_fullscreen_check = {0};
    const int fullscreen_check_interval_ms = 500; // Check every 500ms

    while (*running && display) {
        // Check for fullscreen status periodically
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - last_fullscreen_check.tv_sec) * 1000 + 
                         (now.tv_usec - last_fullscreen_check.tv_usec) / 1000;
        
        if (elapsed_ms >= fullscreen_check_interval_ms) {
            bongocat_log_debug("Performing fullscreen check (interval: %dms)", fullscreen_check_interval_ms);
            bool new_fullscreen_state = check_fullscreen_status();
            if (new_fullscreen_state != fullscreen_detected) {
                fullscreen_detected = new_fullscreen_state;
                bongocat_log_info("Fullscreen state changed: %s", 
                                 fullscreen_detected ? "detected" : "cleared");
                // Trigger a redraw with the new state
                if (configured) {
                    bongocat_log_debug("Triggering redraw due to fullscreen state change");
                    draw_bar();
                }
            }
            last_fullscreen_check = now;
        }

        // Use non-blocking dispatch with timeout
        struct pollfd pfd = {
            .fd = wl_display_get_fd(display),
            .events = POLLIN,
        };
        
        // Prepare display for reading
        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) == -1) {
                bongocat_log_error("Failed to dispatch pending events");
                return BONGOCAT_ERROR_WAYLAND;
            }
        }
        
        // Poll with timeout to allow periodic fullscreen checks
        int poll_result = poll(&pfd, 1, 100); // 100ms timeout
        
        if (poll_result > 0) {
            if (wl_display_read_events(display) == -1) {
                bongocat_log_error("Failed to read Wayland events");
                return BONGOCAT_ERROR_WAYLAND;
            }
            if (wl_display_dispatch_pending(display) == -1) {
                bongocat_log_error("Failed to dispatch Wayland events");
                return BONGOCAT_ERROR_WAYLAND;
            }
        } else if (poll_result == 0) {
            // Timeout - cancel the read
            wl_display_cancel_read(display);
        } else {
            // Error
            wl_display_cancel_read(display);
            if (errno != EINTR) {
                bongocat_log_error("Poll error: %s", strerror(errno));
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

int wayland_get_screen_width(void) {
    return wayland_screen_width;
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

    if (output) {
        wl_output_destroy(output);
        output = NULL;
    }

    if (layer_shell) {
        zwlr_layer_shell_v1_destroy(layer_shell);
        layer_shell = NULL;
    }

    if (xdg_wm_base) {
        xdg_wm_base_destroy(xdg_wm_base);
        xdg_wm_base = NULL;
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
<<<<<<< HEAD
    fullscreen_detected = false;
=======
    wayland_screen_width = 0;
    wayland_screen_height = 0;
    wayland_transform = WL_OUTPUT_TRANSFORM_NORMAL;
    wayland_raw_width = 0;
    wayland_raw_height = 0;
    wayland_mode_received = false;
    wayland_geometry_received = false;
>>>>>>> main
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
