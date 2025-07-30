#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "input.h"
#include "animation.h"
#include "memory.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

int *any_key_pressed;
static pid_t input_child_pid = -1;

static void capture_input_multiple(char **device_paths, int num_devices, int enable_debug) {
    bongocat_log_debug("Starting input capture on %d devices", num_devices);
    
    int *fds = BONGOCAT_MALLOC(num_devices * sizeof(int));
    if (!fds) {
        bongocat_log_error("Failed to allocate memory for file descriptors");
        exit(1);
    }
    
    int max_fd = -1;
    int valid_devices = 0;
    
    // Open all devices
    for (int i = 0; i < num_devices; i++) {
        fds[i] = -1;
        
        // Validate device path exists and is readable
        struct stat st;
        if (stat(device_paths[i], &st) != 0) {
            bongocat_log_warning("Input device does not exist: %s", device_paths[i]);
            continue;
        }
        
        if (!S_ISCHR(st.st_mode)) {
            bongocat_log_warning("Input device is not a character device: %s", device_paths[i]);
            continue;
        }
        
        fds[i] = open(device_paths[i], O_RDONLY | O_NONBLOCK);
        if (fds[i] < 0) {
            bongocat_log_warning("Failed to open %s: %s", device_paths[i], strerror(errno));
            continue;
        }
        
        bongocat_log_info("Input monitoring started on %s (fd=%d)", device_paths[i], fds[i]);
        if (fds[i] > max_fd) {
            max_fd = fds[i];
        }
        valid_devices++;
    }
    
    if (valid_devices == 0) {
        bongocat_log_error("No valid input devices found");
        BONGOCAT_SAFE_FREE(fds);
        exit(1);
    }
    
    bongocat_log_info("Successfully opened %d/%d input devices", valid_devices, num_devices);

    struct input_event ev[64];
    int rd;
    fd_set readfds;
    struct timeval timeout;
    
    while (1) {
        FD_ZERO(&readfds);
        
        // Add all valid file descriptors to the set
        for (int i = 0; i < num_devices; i++) {
            if (fds[i] >= 0) {
                FD_SET(fds[i], &readfds);
            }
        }
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int select_result = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (select_result < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            bongocat_log_error("Select error: %s", strerror(errno));
            break;
        }
        
        if (select_result == 0) continue; // Timeout
        
        // Check which devices have data
        for (int i = 0; i < num_devices; i++) {
            if (fds[i] >= 0 && FD_ISSET(fds[i], &readfds)) {
                rd = read(fds[i], ev, sizeof(ev));
                if (rd < 0) {
                    if (errno == EAGAIN) continue;
                    bongocat_log_warning("Read error on %s: %s", device_paths[i], strerror(errno));
                    close(fds[i]);
                    fds[i] = -1;
                    valid_devices--;
                    continue;
                }
                
                if (rd == 0) {
                    bongocat_log_warning("EOF on input device %s", device_paths[i]);
                    close(fds[i]);
                    fds[i] = -1;
                    valid_devices--;
                    continue;
                }

                for (int j = 0; j < (int)(rd / sizeof(struct input_event)); j++) {
                    if (ev[j].type == EV_KEY && ev[j].value == 1) {
                        if (enable_debug) {
                            bongocat_log_debug("Key event: device=%s, code=%d, time=%ld.%06ld", 
                                             device_paths[i], ev[j].code, ev[j].time.tv_sec, ev[j].time.tv_usec);
                        }
                        animation_trigger();
                    }
                }
            }
        }
        
        // Exit if no valid devices remain
        if (valid_devices == 0) {
            bongocat_log_error("All input devices became unavailable");
            break;
        }
    }
    
    // Close all file descriptors
    for (int i = 0; i < num_devices; i++) {
        if (fds[i] >= 0) {
            close(fds[i]);
        }
    }
    
    BONGOCAT_SAFE_FREE(fds);
    bongocat_log_info("Input monitoring stopped");
}

bongocat_error_t input_start_monitoring(char **device_paths, int num_devices, int enable_debug) {
    BONGOCAT_CHECK_NULL(device_paths, BONGOCAT_ERROR_INVALID_PARAM);
    
    if (num_devices <= 0) {
        bongocat_log_error("No input devices specified");
        return BONGOCAT_ERROR_INVALID_PARAM;
    }
    
    bongocat_log_info("Initializing input monitoring system for %d devices", num_devices);
    
    // Initialize shared memory for key press flag
    any_key_pressed = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (any_key_pressed == MAP_FAILED) {
        bongocat_log_error("Failed to create shared memory for input monitoring: %s", strerror(errno));
        return BONGOCAT_ERROR_MEMORY;
    }
    *any_key_pressed = 0;

    // Fork process for input monitoring
    input_child_pid = fork();
    if (input_child_pid < 0) {
        bongocat_log_error("Failed to fork input monitoring process: %s", strerror(errno));
        munmap(any_key_pressed, sizeof(int));
        any_key_pressed = NULL;
        return BONGOCAT_ERROR_THREAD;
    }
    
    if (input_child_pid == 0) {
        // Child process - handle keyboard input from multiple devices
        bongocat_log_debug("Input monitoring child process started (PID: %d)", getpid());
        capture_input_multiple(device_paths, num_devices, enable_debug);
        exit(0);
    }
    
    bongocat_log_info("Input monitoring started (child PID: %d)", input_child_pid);
    return BONGOCAT_SUCCESS;
}

void input_cleanup(void) {
    bongocat_log_info("Cleaning up input monitoring system");
    
    // Terminate child process if it exists
    if (input_child_pid > 0) {
        bongocat_log_debug("Terminating input monitoring child process (PID: %d)", input_child_pid);
        kill(input_child_pid, SIGTERM);
        
        // Wait for child to terminate with timeout
        int status;
        int wait_attempts = 0;
        while (wait_attempts < 10) {
            pid_t result = waitpid(input_child_pid, &status, WNOHANG);
            if (result == input_child_pid) {
                bongocat_log_debug("Input monitoring child process terminated gracefully");
                break;
            } else if (result == -1) {
                bongocat_log_warning("Error waiting for input child process: %s", strerror(errno));
                break;
            }
            
            usleep(100000); // Wait 100ms
            wait_attempts++;
        }
        
        // Force kill if still running
        if (wait_attempts >= 10) {
            bongocat_log_warning("Force killing input monitoring child process");
            kill(input_child_pid, SIGKILL);
            waitpid(input_child_pid, &status, 0);
        }
        
        input_child_pid = -1;
    }
    
    // Cleanup shared memory
    if (any_key_pressed && any_key_pressed != MAP_FAILED) {
        munmap(any_key_pressed, sizeof(int));
        any_key_pressed = NULL;
    }
    
    bongocat_log_debug("Input monitoring cleanup complete");
}