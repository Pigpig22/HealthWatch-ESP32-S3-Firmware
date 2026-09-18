#ifndef __BLE_H__
#define __BLE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_init(void);
bool ble_send_data(const uint8_t *data, uint16_t len);
bool ble_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif