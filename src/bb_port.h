#ifndef __BB_PORT_H
#define __BB_PORT_H

// The only Arduino / ESP-IDF difference: the serial console

#include <stddef.h>

void bb_port_serial_begin();
void bb_port_serial_write(const char* buf, size_t len);
int  bb_port_serial_read(char* buf, size_t max);       // non-blocking, returns the number of bytes read
void bb_port_serial_sync();                             // waits until everything has left the UART

#endif // __BB_PORT_H
