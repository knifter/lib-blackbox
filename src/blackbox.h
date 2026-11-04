#ifndef __BLACKBOX_H
#define __BLACKBOX_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "blackbox_config.h"

#ifdef ARDUINO
    #include <Print.h>
#endif

// Same numbering as esp_log_level_t
typedef enum : uint8_t
{
    BB_NONE = 0,
    BB_ERROR,
    BB_WARNING,
    BB_INFO,
    BB_DEBUG,
    BB_VERBOSE,
} bb_level_t;

typedef enum : uint8_t
{
    BB_SRC_APP = 0,         // the logging macros
    BB_SRC_ESPIDF,          // ESP_LOGx
    BB_SRC_PRINT,           // BlackBox.print() and friends
    BB_SRC_SYSTEM,          // BlackBox itself: boot marker, lost records, command replies
} bb_source_t;

typedef void (*bb_command_handler_t)(const char* args);

class BlackBoxClass
#ifdef ARDUINO
    : public Print
#endif
{
    public:
        void begin();                       // starts the dispatcher, the sinks and the ESP-IDF hook

        void log(bb_level_t level, const char* file, const char* func, uint16_t line, const char* fmt, ...)
            __attribute__((format(printf, 6, 7)));
        void vlog(bb_level_t level, const char* file, const char* func, uint16_t line, const char* fmt, va_list args);

        bool flush(uint32_t timeout_ms);    // waits until every sink has sent everything; false on timeout
        void clear();                       // empties the ring

        // Application commands, typed over telnet/serial; queued and run by handleCommands()
        bool addCommand(const char* name, bb_command_handler_t handler, const char* help = nullptr);
        void handleCommands();              // call from loop(): runs queued commands in the caller's task

#ifdef ARDUINO
        size_t write(uint8_t c) override;
        size_t write(const uint8_t* buf, size_t len) override;
        void flush() override { flush(500); };
#else
        void flush() { flush(500); };
#endif
};

extern BlackBoxClass BlackBox;

// Logging macros, compatible with lib-tools' tools-log.h
#define BB_LOG(level, msg, ...)     BlackBox.log(level, __FILE__, __FUNCTION__, __LINE__, msg, ##__VA_ARGS__)

#undef ERROR
#undef WARNING
#undef INFO
#undef DBG
#undef VERBOSE

#if BLACKBOX_LEVEL >= 1
    #define ERROR(msg, ...)         BB_LOG(BB_ERROR, msg, ##__VA_ARGS__)
#else
    #define ERROR(msg, ...)
#endif
#if BLACKBOX_LEVEL >= 2
    #define WARNING(msg, ...)       BB_LOG(BB_WARNING, msg, ##__VA_ARGS__)
#else
    #define WARNING(msg, ...)
#endif
#if BLACKBOX_LEVEL >= 3
    #define INFO(msg, ...)          BB_LOG(BB_INFO, msg, ##__VA_ARGS__)
#else
    #define INFO(msg, ...)
#endif
#if BLACKBOX_LEVEL >= 4
    #define DBG(msg, ...)           BB_LOG(BB_DEBUG, msg, ##__VA_ARGS__)
#else
    #define DBG(msg, ...)
#endif
#if BLACKBOX_LEVEL >= 5
    #define VERBOSE(msg, ...)       BB_LOG(BB_VERBOSE, msg, ##__VA_ARGS__)
#else
    #define VERBOSE(msg, ...)
#endif

#endif // __BLACKBOX_H
