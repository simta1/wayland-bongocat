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

static void capture_input(const char *device_path, int enable_debug) {
    bongocat_log_debug("Starting input capture on: %s", device_path);
    
    // Validate device path exists and is readable
    struct stat st;
    if (stat(device_path, &st) != 0) {
        bongocat_log_error("Input device does not exist: %s", device_path);
        exit(1);
    }
    
    if (!S_ISCHR(st.st_mode)) {
        bongocat_log_error("Input device is not a character device: %s", device_path);
        exit(1);
    }
    
    int fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        bongocat_log_error("Failed to open %s: %s", device_path, strerror(errno));
        exit(1);
    }
    
    bongocat_log_info("Input monitoring started on %s", device_path);

    struct input_event ev[64];
    int rd;
    fd_set readfds;
    struct timeval timeout;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int select_result = select(fd + 1, &readfds, NULL, NULL, &timeout);
        if (select_result < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            bongocat_log_error("Select error on %s: %s", device_path, strerror(errno));
            break;
        }
        
        if (select_result == 0) continue; // Timeout
        
        rd = read(fd, ev, sizeof(ev));
        if (rd < 0) {
            if (errno == EAGAIN) continue;
            bongocat_log_error("Read error on %s: %s", device_path, strerror(errno));
            break;
        }
        
        if (rd == 0) {
            bongocat_log_warning("EOF on input device %s", device_path);
            break;
        }

        for (int i = 0; i < (int)(rd / sizeof(struct input_event)); i++) {
            if (ev[i].type == EV_KEY && ev[i].value == 1) {
                if (enable_debug) {
                    bongocat_log_debug("Key event: device=%s, code=%d, time=%ld.%06ld", 
                                     device_path, ev[i].code, ev[i].time.tv_sec, ev[i].time.tv_usec);
                }
                animation_trigger();
            }
        }
    }
    
    close(fd);
    bongocat_log_info("Input monitoring stopped");
}

bongocat_error_t input_start_monitoring(const char *device_path, int enable_debug) {
    BONGOCAT_CHECK_NULL(device_path, BONGOCAT_ERROR_INVALID_PARAM);
    
    bongocat_log_info("Initializing input monitoring system");
    
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
        // Child process - handle keyboard input
        bongocat_log_debug("Input monitoring child process started (PID: %d)", getpid());
        capture_input(device_path, enable_debug);
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