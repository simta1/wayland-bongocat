#ifndef BONGOCAT_H
#define BONGOCAT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/input.h>

#define _POSIX_C_SOURCE 200809L
#include <wayland-client.h>

#include "../lib/stb_image.h"

// Version
#define BONGOCAT_VERSION "1.2"

// Common constants
#define NUM_FRAMES 3
#define DEFAULT_SCREEN_WIDTH 1920
#define DEFAULT_BAR_HEIGHT 40

#endif // BONGOCAT_H