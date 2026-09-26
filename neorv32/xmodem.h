#ifndef XMODEM_H_
#define XMODEM_H_

#include <stdint.h>

int xmodem_receive(uint32_t *base_addr);
int xmodem_get_block();
uint8_t xmodem_get_payload(uint32_t *buf);

#endif // include guard
