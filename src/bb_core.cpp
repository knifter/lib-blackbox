#include "bb_internal.h"

#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_app_desc.h>

#define BB_MAGIC        0x58424242      // "BBBX"
#define BB_MAX_SINKS    4

// Stored byte by byte in the ring, so it may straddle the wrap point
typedef struct __attribute__((packed))
{
    uint16_t len;                       // header + text
    uint8_t level;
    uint8_t source;
    uint32_t time_ms;
    const char* file;
    const char* func;
    uint16_t line;
} bb_hdr_t;

typedef struct
{
    uint32_t magic;
    uint32_t size;
    char elf[8];                        // file/func pointers are only valid for the firmware that wrote them
    uint32_t head;                      // total bytes ever written
    uint32_t tail;                      // start of the oldest complete record
    uint8_t ring[BLACKBOX_BUFSIZE];
} bb_buf_t;

#if BLACKBOX_RTC
RTC_NOINIT_ATTR static bb_buf_t s_buf;
#else
static bb_buf_t s_buf;
#endif

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;
static uint32_t s_boot_start = 0;
static char s_elf[9];
static TaskHandle_t s_task = nullptr;
static bb_sink* s_sinks[BB_MAX_SINKS];
static int s_nsinks = 0;
#if BLACKBOX_ESPLOG && !BLACKBOX_SINK_SERIAL
static vprintf_like_t s_prev_vprintf = nullptr;
#endif

BlackBoxClass BlackBox;

static void ring_in(uint32_t pos, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*) data;
    while(len)
    {
        size_t at = pos % BLACKBOX_BUFSIZE;
        size_t n = BB_MIN(len, BLACKBOX_BUFSIZE - at);
        memcpy(s_buf.ring + at, p, n);
        pos += n;
        p += n;
        len -= n;
    };
};

static void ring_out(uint32_t pos, void* data, size_t len)
{
    uint8_t* p = (uint8_t*) data;
    while(len)
    {
        size_t at = pos % BLACKBOX_BUFSIZE;
        size_t n = BB_MIN(len, BLACKBOX_BUFSIZE - at);
        memcpy(p, s_buf.ring + at, n);
        pos += n;
        p += n;
        len -= n;
    };
};

static const char* reset_reason_str(esp_reset_reason_t r)
{
    switch(r)
    {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external pin";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog";
        case ESP_RST_WDT:       return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep sleep wake";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "unknown";
    };
};

// Once per boot, at the first log call (in setup/app_main, before other tasks log)
static void bb_init()
{
    s_initialized = true;
    esp_app_get_elf_sha256(s_elf, sizeof(s_elf));

    if(s_buf.magic != BB_MAGIC || s_buf.size != BLACKBOX_BUFSIZE || memcmp(s_buf.elf, s_elf, 8) != 0
        || s_buf.head - s_buf.tail > BLACKBOX_BUFSIZE)
    {
        s_buf.magic = BB_MAGIC;
        s_buf.size = BLACKBOX_BUFSIZE;
        memcpy(s_buf.elf, s_elf, 8);
        s_buf.head = 0;
        s_buf.tail = 0;
    };
    s_boot_start = s_buf.head;

    bb_say("boot, reset reason: %s", reset_reason_str(esp_reset_reason()));
};

uint32_t bb_now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
};

const char* bb_elf_sha()
{
    return s_elf;
};

uint32_t bb_head()
{
    return s_buf.head;
};

uint32_t bb_tail()
{
    return s_buf.tail;
};

void bb_store(bb_level_t level, bb_source_t source, const char* file, const char* func, uint16_t line, const char* text, size_t len)
{
    if(!s_initialized)
        bb_init();

    // Strip the line endings the old macros and ESP-IDF add; sinks add their own
    while(len && (text[len - 1] == '\n' || text[len - 1] == '\r'))
        len--;
    if(len > BLACKBOX_LINE_MAX)
        len = BLACKBOX_LINE_MAX;

    bb_hdr_t hdr;
    hdr.len = (uint16_t)(sizeof(bb_hdr_t) + len);
    hdr.level = level;
    hdr.source = source;
    hdr.time_ms = bb_now_ms();
    hdr.file = file;
    hdr.func = func;
    hdr.line = line;

    portENTER_CRITICAL(&s_mux);
    // Drop the oldest records until the new one fits
    while(s_buf.head + hdr.len - s_buf.tail > BLACKBOX_BUFSIZE)
    {
        uint16_t old;
        ring_out(s_buf.tail, &old, sizeof(old));
        if(old < sizeof(bb_hdr_t) || old > BLACKBOX_BUFSIZE)
        {
            s_buf.tail = s_buf.head;    // corrupted, start over
            break;
        };
        s_buf.tail += old;
    };
    ring_in(s_buf.head, &hdr, sizeof(hdr));
    ring_in(s_buf.head + sizeof(hdr), text, len);
    s_buf.head += hdr.len;
    portEXIT_CRITICAL(&s_mux);

    if(s_task)
        xTaskNotifyGive(s_task);
};

void bb_say(const char* fmt, ...)
{
    char text[BLACKBOX_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    if(len > 0)
        bb_store(BB_INFO, BB_SRC_SYSTEM, nullptr, nullptr, 0, text, BB_MIN((size_t)len, sizeof(text) - 1));
};

bool bb_read(uint32_t& cursor, bb_record_t& rec, char* text, size_t text_size, uint32_t& lost)
{
    bb_hdr_t hdr;
    size_t n = 0;
    bool ok = false;
    lost = 0;

    portENTER_CRITICAL(&s_mux);
    if((int32_t)(cursor - s_buf.tail) < 0)
    {
        lost = s_buf.tail - cursor;
        cursor = s_buf.tail;
    }
    else if(cursor != s_buf.head)
    {
        ring_out(cursor, &hdr, sizeof(hdr));
        if(hdr.len < sizeof(bb_hdr_t) || hdr.len > BLACKBOX_BUFSIZE)
        {
            cursor = s_buf.head;        // corrupted, skip to the newest data
        }
        else
        {
            n = BB_MIN((size_t)(hdr.len - sizeof(bb_hdr_t)), text_size - 1);
            ring_out(cursor + sizeof(hdr), text, n);
            cursor += hdr.len;
            ok = true;
        };
    };
    portEXIT_CRITICAL(&s_mux);

    if(!ok)
        return false;
    text[n] = 0;
    rec.level = (bb_level_t) hdr.level;
    rec.source = (bb_source_t) hdr.source;
    rec.time_ms = hdr.time_ms;
    rec.file = hdr.file;
    rec.func = hdr.func;
    rec.line = hdr.line;
    rec.text = text;
    rec.text_len = n;
    return true;
};

bool bb_sink::next(bb_record_t& rec)
{
    uint32_t lost;
    if(bb_read(_cursor, rec, _text, sizeof(_text), lost))
        return true;
    if(!lost)
        return false;

    // This sink fell behind: tell it instead of silently skipping
    int n = snprintf(_text, sizeof(_text), "%u bytes of log lost (output too slow)", (unsigned) lost);
    rec.level = BB_WARNING;
    rec.source = BB_SRC_SYSTEM;
    rec.time_ms = bb_now_ms();
    rec.file = nullptr;
    rec.func = nullptr;
    rec.line = 0;
    rec.text = _text;
    rec.text_len = (n > 0) ? BB_MIN((size_t)n, sizeof(_text) - 1) : 0;
    return true;
};

bool bb_sink::at_head()
{
    return _cursor == s_buf.head;
};

void bb_sink::replay()
{
    portENTER_CRITICAL(&s_mux);
    _cursor = s_buf.tail;
    portEXIT_CRITICAL(&s_mux);
};

void bb_poll_all()
{
    for(int i = 0; i < s_nsinks; i++)
        s_sinks[i]->poll();
};

static void bb_task(void* arg)
{
    for(;;)
    {
        bb_poll_all();
        // Woken by new records, or every 50ms for input and new clients
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
    };
};

static void bb_add_sink(bb_sink* sink)
{
    if(s_nsinks >= BB_MAX_SINKS)
        return;
    sink->set_cursor(s_boot_start);     // everything logged since this boot, including before begin()
    s_sinks[s_nsinks++] = sink;
};

#if BLACKBOX_ESPLOG
// Installed with esp_log_set_vprintf(): receives every ESP_LOGx message
static int bb_vprintf(const char* fmt, va_list args)
{
#if !BLACKBOX_SINK_SERIAL
    // No serial sink: keep the normal UART output as well
    va_list copy;
    va_copy(copy, args);
    if(s_prev_vprintf)
        s_prev_vprintf(fmt, copy);
    va_end(copy);
#endif

    char line[BLACKBOX_LINE_MAX];
    int len = vsnprintf(line, sizeof(line), fmt, args);
    if(len <= 0)
        return len;
    size_t n = BB_MIN((size_t)len, sizeof(line) - 1);

    // "E (1234) tag: message", possibly behind a color code
    const char* p = line;
    if(*p == '\033')
    {
        while(*p && *p != 'm')
            p++;
        if(*p)
            p++;
    };
    bb_level_t level;
    switch(*p)
    {
        case 'E': level = BB_ERROR; break;
        case 'W': level = BB_WARNING; break;
        case 'D': level = BB_DEBUG; break;
        case 'V': level = BB_VERBOSE; break;
        default:  level = BB_INFO; break;
    };

    bb_store(level, BB_SRC_ESPIDF, nullptr, nullptr, 0, line, n);
    return len;
};
#endif

void BlackBoxClass::begin()
{
    if(s_task)
        return;
    if(!s_initialized)
        bb_init();

#if BLACKBOX_SINK_SERIAL
    bb_add_sink(bb_sink_serial_get());
#endif
#if BLACKBOX_SINK_TELNET
    bb_add_sink(bb_sink_telnet_get());
#endif
#if BLACKBOX_SINK_SYSLOG
    bb_add_sink(bb_sink_syslog_get());
#endif
#if BLACKBOX_SINK_BINARY
    bb_add_sink(bb_sink_binary_get());
#endif
    for(int i = 0; i < s_nsinks; i++)
        s_sinks[i]->begin();

    bb_commands_begin();

#if BLACKBOX_ESPLOG
  #if BLACKBOX_SINK_SERIAL
    esp_log_set_vprintf(&bb_vprintf);
  #else
    s_prev_vprintf = esp_log_set_vprintf(&bb_vprintf);
  #endif
#endif

    xTaskCreatePinnedToCore(&bb_task, "blackbox", BLACKBOX_TASK_STACK, nullptr, BLACKBOX_TASK_PRIO, &s_task, BLACKBOX_TASK_CORE);

#if BLACKBOX_COREDUMP_REPORT
    bb_coredump_report();
#endif
};

void BlackBoxClass::vlog(bb_level_t level, const char* file, const char* func, uint16_t line, const char* fmt, va_list args)
{
    char text[BLACKBOX_LINE_MAX];
    int len = vsnprintf(text, sizeof(text), fmt, args);
    if(len < 0)
        return;
    bb_store(level, BB_SRC_APP, file, func, line, text, BB_MIN((size_t)len, sizeof(text) - 1));
};

void BlackBoxClass::log(bb_level_t level, const char* file, const char* func, uint16_t line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(level, file, func, line, fmt, args);
    va_end(args);
};

bool BlackBoxClass::flush(uint32_t timeout_ms)
{
    // The dispatcher is the one sending: it can't wait for itself
    if(!s_task || xTaskGetCurrentTaskHandle() == s_task)
        return false;

    xTaskNotifyGive(s_task);
    TickType_t start = xTaskGetTickCount();
    bool done;
    for(;;)
    {
        done = true;
        for(int i = 0; i < s_nsinks; i++)
            if(!s_sinks[i]->idle())
                done = false;
        if(done || xTaskGetTickCount() - start >= pdMS_TO_TICKS(timeout_ms))
            break;
        vTaskDelay(1);
    };

    for(int i = 0; i < s_nsinks; i++)
        s_sinks[i]->sync();
    return done;
};

void BlackBoxClass::clear()
{
    portENTER_CRITICAL(&s_mux);
    s_buf.tail = s_buf.head;
    for(int i = 0; i < s_nsinks; i++)
        s_sinks[i]->set_cursor(s_buf.head);
    portEXIT_CRITICAL(&s_mux);
    bb_say("log cleared");
};

#ifdef ARDUINO
// Print interface: collects characters into lines; use from one task at a time
static char s_print_line[BLACKBOX_LINE_MAX];
static size_t s_print_len = 0;

size_t BlackBoxClass::write(uint8_t c)
{
    return write(&c, 1);
};

size_t BlackBoxClass::write(const uint8_t* buf, size_t len)
{
    for(size_t i = 0; i < len; i++)
    {
        char c = (char) buf[i];
        if(c == '\r')
            continue;
        bool commit = (c == '\n');
        if(!commit)
        {
            s_print_line[s_print_len++] = c;
            commit = (s_print_len >= sizeof(s_print_line));
        };
        if(commit && s_print_len)
        {
            bb_store(BB_INFO, BB_SRC_PRINT, nullptr, nullptr, 0, s_print_line, s_print_len);
            s_print_len = 0;
        };
    };
    return len;
};
#endif
