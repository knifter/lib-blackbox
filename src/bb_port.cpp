#include "bb_internal.h"

#if BLACKBOX_SINK_SERIAL

#include "bb_port.h"

#ifdef ARDUINO

#include <Arduino.h>

// Serial.begin() is left to the application
void bb_port_serial_begin()
{
};

void bb_port_serial_write(const char* buf, size_t len)
{
    Serial.write((const uint8_t*) buf, len);
};

int bb_port_serial_read(char* buf, size_t max)
{
    int n = 0;
    while(n < (int) max && Serial.available() > 0)
        buf[n++] = (char) Serial.read();
    return n;
};

void bb_port_serial_sync()
{
    Serial.flush();
};

#else // ESP-IDF: the console on stdin/stdout

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

void bb_port_serial_begin()
{
    int flags = fcntl(fileno(stdin), F_GETFL, 0);
    fcntl(fileno(stdin), F_SETFL, flags | O_NONBLOCK);
};

void bb_port_serial_write(const char* buf, size_t len)
{
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
};

int bb_port_serial_read(char* buf, size_t max)
{
    int n = read(fileno(stdin), buf, max);
    return (n > 0) ? n : 0;
};

void bb_port_serial_sync()
{
    fflush(stdout);
    fsync(fileno(stdout));
};

#endif // ARDUINO
#endif // BLACKBOX_SINK_SERIAL
