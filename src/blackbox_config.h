#ifndef __BLACKBOX_CONFIG_H
#define __BLACKBOX_CONFIG_H

// Settings can be overridden in the project's config.h or with -D build flags
#if __has_include("config.h")
    #include "config.h"
#else
    #warning "config.h not detected. Blackbox logging is using defaults. Make sure to include that path to config.h to your build."
#endif

#include <sdkconfig.h>

// Ring buffer
#ifndef BLACKBOX_BUFSIZE
    #define BLACKBOX_BUFSIZE            4096    // bytes; records of header + text
#endif
#ifndef BLACKBOX_RTC
    #define BLACKBOX_RTC                1       // 1: ring in RTC memory, survives deep sleep, panic and watchdog resets
#endif
#ifndef BLACKBOX_LINE_MAX
    #define BLACKBOX_LINE_MAX           256     // longest message text
#endif

// Compile time level: macros more verbose than this are removed (1=error .. 5=verbose)
#ifndef BLACKBOX_LEVEL
    #define BLACKBOX_LEVEL              5
#endif

// 1: the ISR_xxx macros, for logging from interrupt handlers (see the README for the conditions)
#ifndef BLACKBOX_ISR
    #define BLACKBOX_ISR                0
#endif

// Capture ESP-IDF ESP_LOGx output
#ifndef BLACKBOX_ESPLOG
    #define BLACKBOX_ESPLOG             1
#endif

// Sinks
#ifndef BLACKBOX_SINK_SERIAL
    #define BLACKBOX_SINK_SERIAL        1
#endif
#ifndef BLACKBOX_SERIAL_COLORS
    #define BLACKBOX_SERIAL_COLORS      0
#endif
#ifndef BLACKBOX_SERIAL_COMMANDS
    #define BLACKBOX_SERIAL_COMMANDS    1
#endif

#ifndef BLACKBOX_SINK_TELNET
    #define BLACKBOX_SINK_TELNET        0
#endif
#ifndef BLACKBOX_TELNET_PORT
    #define BLACKBOX_TELNET_PORT        23
#endif
#ifndef BLACKBOX_TELNET_COLORS
    #define BLACKBOX_TELNET_COLORS      1
#endif

#ifndef BLACKBOX_SINK_SYSLOG
    #define BLACKBOX_SINK_SYSLOG        0
#endif
#ifndef BLACKBOX_SYSLOG_SERVER
    #define BLACKBOX_SYSLOG_SERVER      ""      // IPv4 address as a string, e.g. "192.168.1.10"
#endif
#ifndef BLACKBOX_SYSLOG_PORT
    #define BLACKBOX_SYSLOG_PORT        514
#endif

#ifndef BLACKBOX_SINK_BINARY
    #define BLACKBOX_SINK_BINARY        0
#endif
#ifndef BLACKBOX_BINARY_PORT
    #define BLACKBOX_BINARY_PORT        2323
#endif

#ifndef BLACKBOX_HOSTNAME
    #define BLACKBOX_HOSTNAME           "esp32"
#endif

// Log the last crash's core dump at boot
#ifndef BLACKBOX_COREDUMP_REPORT
    #define BLACKBOX_COREDUMP_REPORT    0
#endif
// 1: also log registers A0-A15 of the crash (Xtensa); rarely needed, the backtrace usually says enough
#ifndef BLACKBOX_COREDUMP_REGISTERS
    #define BLACKBOX_COREDUMP_REGISTERS 0
#endif
// 1: erase the dump after reporting it (a flash write), so it is reported once
// 0: keep it and report it at every boot. A dump from another firmware or an unreadable one is
//    always erased, with only a warning
#ifndef BLACKBOX_COREDUMP_ERASE
    #define BLACKBOX_COREDUMP_ERASE     0
#endif

// Commands
#ifndef BLACKBOX_MAX_COMMANDS
    #define BLACKBOX_MAX_COMMANDS       16
#endif
#ifndef BLACKBOX_CMD_QUEUE
    #define BLACKBOX_CMD_QUEUE          4       // queued application commands, run by handleCommands()
#endif
#ifndef BLACKBOX_CMD_LEN
    #define BLACKBOX_CMD_LEN            64
#endif

// Dispatcher task
#ifndef BLACKBOX_TASK_CORE
    #define BLACKBOX_TASK_CORE          0       // not the Arduino loop() core, so a hanging loop() can't starve it
#endif
#ifndef BLACKBOX_TASK_PRIO
    #define BLACKBOX_TASK_PRIO          1
#endif
#ifndef BLACKBOX_TASK_STACK
    #define BLACKBOX_TASK_STACK         4096
#endif

#endif // __BLACKBOX_CONFIG_H
