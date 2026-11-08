#ifndef __BLACKBOX_H
#define __BLACKBOX_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "blackbox_config.h"

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
    BB_SRC_PRINT,           // BlackBoxPrint (bb_print.h, Arduino only)
    BB_SRC_SYSTEM,          // BlackBox itself: boot marker, lost records, command replies
    BB_SRC_ISR,             // ISR_xxx macros
} bb_source_t;

typedef void (*bb_command_handler_t)(const char* args);

// File and function are stored as pointers and read later by the dispatcher, even after a reset, so they must
// be static strings (__FILE__, __FUNCTION__, literals) or nullptr. With C++20 consteval, anything else
// (a stack or heap buffer) is a compile error; older standards accept it unchecked. No runtime cost either way
struct bb_static_str
{
#if __cpp_consteval
    consteval bb_static_str(const char* s) : str(s) {};
#else
    constexpr bb_static_str(const char* s) : str(s) {};
#endif
    const char* str;
};

class BlackBoxClass
{
    public:
        void begin();                       // starts the dispatcher, the sinks and the ESP-IDF hook

        void log(bb_level_t level, bb_static_str file, bb_static_str func, uint16_t line, const char* fmt, ...)
            __attribute__((format(printf, 6, 7)));
        void vlog(bb_level_t level, bb_static_str file, bb_static_str func, uint16_t line, const char* fmt, va_list args);
#if BLACKBOX_ISR
        void logISR(bb_level_t level, bb_static_str file, bb_static_str func, uint16_t line, const char* text, size_t len);
#endif

        bool flush(uint32_t timeout_ms);    // waits until every sink has sent everything; false on timeout
        void clear();                       // empties the ring

        // Application commands, typed over telnet/serial; queued and run by handleCommands()
        bool addCommand(const char* name, bb_command_handler_t handler, const char* help = nullptr);
        void handleCommands();              // call from loop(): runs queued commands in the caller's task

        void flush() { flush(500); };
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

#if BLACKBOX_ISR

#undef ISR_ERROR
#undef ISR_WARNING
#undef ISR_INFO
#undef ISR_DBG
#undef ISR_VERBOSE

// From an interrupt handler: a string literal only ("" msg won't compile otherwise), no arguments, no formatting.
// Not from IRAM ISRs (ESP_INTR_FLAG_IRAM): this code and the strings are in flash
// Without BLACKBOX_ISR these macros don't exist, so using them is a compile error
#define BB_LOG_ISR(level, msg)      BlackBox.logISR(level, __FILE__, __FUNCTION__, __LINE__, "" msg, sizeof("" msg) - 1)

#if BLACKBOX_LEVEL >= 1
    #define ISR_ERROR(msg)          BB_LOG_ISR(BB_ERROR, msg)
#else
    #define ISR_ERROR(msg)
#endif
#if BLACKBOX_LEVEL >= 2
    #define ISR_WARNING(msg)        BB_LOG_ISR(BB_WARNING, msg)
#else
    #define ISR_WARNING(msg)
#endif
#if BLACKBOX_LEVEL >= 3
    #define ISR_INFO(msg)           BB_LOG_ISR(BB_INFO, msg)
#else
    #define ISR_INFO(msg)
#endif
#if BLACKBOX_LEVEL >= 4
    #define ISR_DBG(msg)            BB_LOG_ISR(BB_DEBUG, msg)
#else
    #define ISR_DBG(msg)
#endif
#if BLACKBOX_LEVEL >= 5
    #define ISR_VERBOSE(msg)        BB_LOG_ISR(BB_VERBOSE, msg)
#else
    #define ISR_VERBOSE(msg)
#endif
#endif // BLACKBOX_ISR

#endif // __BLACKBOX_H
