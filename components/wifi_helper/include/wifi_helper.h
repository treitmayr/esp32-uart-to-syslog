#pragma once

#include <stdbool.h>
#include <stdint.h>

bool wifi_start(const char* hostname, const uint32_t conn_timeout_ms);
void wifi_stop(void);
// TODO: Support forcing to override credentials already stored in NVS