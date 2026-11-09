#ifndef SNX_INDEX_CONVERT_H
#define SNX_INDEX_CONVERT_H
#include <stdint.h>
static inline uint16_t vk_index_widen_u8(uint8_t value, int restart, uint32_t restart_index) {
    return restart && value == (restart_index & UINT8_MAX) ? UINT16_MAX : value;
}
#endif
