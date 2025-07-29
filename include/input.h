#ifndef INPUT_H
#define INPUT_H

#include "bongocat.h"
#include "error.h"

extern int *any_key_pressed;

bongocat_error_t input_start_monitoring(const char *device_path, int enable_debug);
void input_cleanup(void);

#endif // INPUT_H