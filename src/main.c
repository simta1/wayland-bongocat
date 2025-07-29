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

static volatile sig_atomic_t running = 1;
static config_t g_config;

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
    
    bongocat_log_info("Starting Bongo Cat Overlay v1.0");
    
    // Parse command line arguments (basic implementation)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Bongo Cat Wayland Overlay\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -h, --help     Show this help message\n");
            printf("  -v, --version  Show version information\n");
            printf("  -c, --config   Specify config file (default: bongocat.conf)\n");
            printf("\nConfiguration is loaded from bongocat.conf in the current directory.\n");
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Bongo Cat Overlay v1.0\n");
            printf("Built with fast optimizations\n");
            return 0;
        }
    }
    
    // Setup signal handlers
    result = setup_signal_handlers();
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to setup signal handlers: %s", bongocat_error_string(result));
        return 1;
    }
    
    // Load configuration
    result = load_config(&g_config);
    if (result != BONGOCAT_SUCCESS) {
        bongocat_log_error("Failed to load configuration: %s", bongocat_error_string(result));
        return 1;
    }
    
    bongocat_log_info("Screen dimensions: %dx%d", g_config.screen_width, g_config.bar_height);
    
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
    result = input_start_monitoring(g_config.keyboard_device, g_config.enable_debug);
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