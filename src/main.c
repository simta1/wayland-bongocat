#define _POSIX_C_SOURCE 200809L
#include "bongocat.h"
#include "wayland.h"
#include "animation.h"
#include "input.h"
#include "config.h"
#include "error.h"
#include "memory.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

static volatile sig_atomic_t running = 1;
static config_t g_config;
static ConfigWatcher g_config_watcher;

static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            bongocat_log_info("Received signal %d, shutting down gracefully", sig);
            running = 0;
            break;
        case SIGCHLD:
            // Handle child process termination
            while (waitpid(-1, NULL, WNOHANG) > 0);
            break;
        default:
            bongocat_log_warning("Received unexpected signal %d", sig);
            break;
    }
}

static void config_reload_callback(const char *config_path) {
    bongocat_log_info("Reloading configuration from: %s", config_path);
    
    // Create a temporary config to test loading
    config_t temp_config;
    bongocat_error_t result = load_config(&temp_config, config_path);
    
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to reload config: %s", bongocat_error_string(result));
        bongocat_log_info("Keeping current configuration");
        return;
    }
    
    // If successful, update the global config
    config_t old_config = g_config;
    g_config = temp_config;
    
    // Update the running systems with new config
    wayland_update_config(&g_config);
    
    // Check if input devices changed and restart monitoring if needed
    bool devices_changed = false;
    if (old_config.num_keyboard_devices != g_config.num_keyboard_devices) {
        devices_changed = true;
    } else {
        // Check if any device paths changed
        for (int i = 0; i < g_config.num_keyboard_devices; i++) {
            bool found = false;
            for (int j = 0; j < old_config.num_keyboard_devices; j++) {
                if (strcmp(g_config.keyboard_devices[i], old_config.keyboard_devices[j]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                devices_changed = true;
                break;
            }
        }
    }
    
    if (devices_changed) {
        bongocat_log_info("Input devices changed, restarting input monitoring");
        bongocat_error_t input_result = input_restart_monitoring(g_config.keyboard_devices, 
                                                                g_config.num_keyboard_devices, 
                                                                g_config.enable_debug);
        if (input_result != BONGOCAT_SUCCESS) {
            bongocat_log_error("Failed to restart input monitoring: %s", bongocat_error_string(input_result));
        } else {
            bongocat_log_info("Input monitoring restarted successfully");
        }
    }
    
    bongocat_log_info("Configuration reloaded successfully!");
    bongocat_log_info("New screen dimensions: %dx%d", g_config.screen_width, g_config.bar_height);
}

static bongocat_error_t setup_signal_handlers(void) {
    struct sigaction sa;
    
    // Setup signal handler
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        bongocat_log_error("Failed to setup SIGINT handler: %s", strerror(errno));
        return BONGOCAT_ERROR_THREAD;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        bongocat_log_error("Failed to setup SIGTERM handler: %s", strerror(errno));
        return BONGOCAT_ERROR_THREAD;
    }
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        bongocat_log_error("Failed to setup SIGCHLD handler: %s", strerror(errno));
        return BONGOCAT_ERROR_THREAD;
    }
    
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    return BONGOCAT_SUCCESS;
}

static void cleanup_and_exit(int exit_code) {
    bongocat_log_info("Performing cleanup...");
    
    // Stop config watcher
    config_watcher_cleanup(&g_config_watcher);
    
    // Stop animation system
    animation_cleanup();
    
    // Cleanup Wayland
    wayland_cleanup();
    
    // Cleanup input system
    input_cleanup();
    
    // Cleanup configuration
    config_cleanup();
    
    // Print memory statistics in debug mode
    if (g_config.enable_debug) {
        memory_print_stats();
    }
    
#ifdef DEBUG
    memory_leak_check();
#endif
    
    bongocat_log_info("Cleanup complete, exiting with code %d", exit_code);
    exit(exit_code);
}

int main(int argc, char *argv[]) {
    bongocat_error_t result;
    
    // Initialize error system early
    bongocat_error_init(1); // Enable debug initially
    
    bongocat_log_info("Starting Bongo Cat Overlay v" BONGOCAT_VERSION);
    
    // Parse command line arguments
    const char *config_file = NULL;
    bool watch_config = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Bongo Cat Wayland Overlay\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -h, --help         Show this help message\n");
            printf("  -v, --version      Show version information\n");
            printf("  -c, --config       Specify config file (default: bongocat.conf)\n");
            printf("  -w, --watch-config Watch config file for changes and reload automatically\n");
            printf("\nConfiguration is loaded from bongocat.conf in the current directory.\n");
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Bongo Cat Overlay v" BONGOCAT_VERSION "\n");
            printf("Built with fast optimizations\n");
            return 0;
        } else if (strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                config_file = argv[i + 1];
                i++; // Skip the next argument since it's the config file path
            } else {
                bongocat_log_error("--config option requires a file path");
                return 1;
            }
        } else if (strcmp(argv[i], "--watch-config") == 0 || strcmp(argv[i], "-w") == 0) {
            watch_config = true;
        } else {
            bongocat_log_warning("Unknown argument: %s", argv[i]);
        }
    }
    
    // Setup signal handlers
    result = setup_signal_handlers();
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to setup signal handlers: %s", bongocat_error_string(result));
        return 1;
    }
    
    // Load configuration
    result = load_config(&g_config, config_file);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to load configuration: %s", bongocat_error_string(result));
        return 1;
    }
    
    bongocat_log_info("Screen dimensions: %dx%d", g_config.screen_width, g_config.bar_height);
    
    // Initialize config watcher if requested
    if (watch_config) {
        const char *watch_path = config_file ? config_file : "bongocat.conf";
        if (config_watcher_init(&g_config_watcher, watch_path, config_reload_callback) == 0) {
            config_watcher_start(&g_config_watcher);
            bongocat_log_info("Config file watching enabled for: %s", watch_path);
        } else {
            bongocat_log_warning("Failed to initialize config watcher, continuing without hot-reload");
        }
    }
    
    // Initialize Wayland
    result = wayland_init(&g_config);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to initialize Wayland: %s", bongocat_error_string(result));
        cleanup_and_exit(1);
    }
    
    // Initialize animation system
    result = animation_init(&g_config);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to initialize animation system: %s", bongocat_error_string(result));
        cleanup_and_exit(1);
    }
    
    // Start input monitoring
    result = input_start_monitoring(g_config.keyboard_devices, g_config.num_keyboard_devices, g_config.enable_debug);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to start input monitoring: %s", bongocat_error_string(result));
        cleanup_and_exit(1);
    }
    
    // Start animation thread
    result = animation_start();
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to start animation thread: %s", bongocat_error_string(result));
        cleanup_and_exit(1);
    }
    
    bongocat_log_info("Bongo Cat Overlay started successfully");
    
    // Main Wayland event loop with graceful shutdown
    result = wayland_run(&running);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Wayland event loop error: %s", bongocat_error_string(result));
        cleanup_and_exit(1);
    }
    
    bongocat_log_info("Main loop exited, shutting down");
    cleanup_and_exit(0);
    
    return 0; // Never reached
}