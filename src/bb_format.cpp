#include "bb_internal.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

char bb_level_char(bb_level_t level)
{
    switch(level)
    {
        case BB_ERROR:      return 'E';
        case BB_WARNING:    return 'W';
        case BB_INFO:       return 'I';
        case BB_DEBUG:      return 'D';
        case BB_VERBOSE:    return 'V';
        default:            return '-';
    };
};

const char* bb_basename(const char* path)
{
    if(!path)
        return "";
    const char* p = path;
    for(const char* c = path; *c; c++)
        if(*c == '/' || *c == '\\')
            p = c + 1;
    return p;
};

static const char* level_color(const bb_record_t& rec)
{
    if(rec.source == BB_SRC_SYSTEM)
        return "\033[1;35m";            // bold magenta
    switch(rec.level)
    {
        case BB_ERROR:      return "\033[31m";
        case BB_WARNING:    return "\033[33m";
        case BB_DEBUG:      return "\033[90m";
        case BB_VERBOSE:    return "\033[2m";
        default:            return "";
    };
};

// "  12.345 E main.cpp:handle_ir(301) message\r\n"
size_t bb_format_text(const bb_record_t& rec, char* out, size_t size, bool colors)
{
    const char* col = colors ? level_color(rec) : "";
    const char* rst = (*col) ? "\033[0m" : "";
    uint32_t sec = rec.time_ms / 1000;
    uint32_t ms = rec.time_ms % 1000;
    int n;

    switch(rec.source)
    {
        case BB_SRC_APP:
        case BB_SRC_ISR:
            n = snprintf(out, size, "%s%6" PRIu32 ".%03" PRIu32 " %c %s:%s(%u) %.*s%s\r\n",
                col, sec, ms, bb_level_char(rec.level), bb_basename(rec.file), rec.func ? rec.func : "", rec.line,
                (int) rec.text_len, rec.text, rst);
            break;
        case BB_SRC_SYSTEM:
            n = snprintf(out, size, "%s%6" PRIu32 ".%03" PRIu32 " --- %.*s ---%s\r\n",
                col, sec, ms, (int) rec.text_len, rec.text, rst);
            break;
        default:        // ESP-IDF lines carry their own level letter and tag
            n = snprintf(out, size, "%s%6" PRIu32 ".%03" PRIu32 " %.*s%s\r\n",
                col, sec, ms, (int) rec.text_len, rec.text, rst);
            break;
    };

    if(n < 0)
        return 0;
    if((size_t) n >= size)
    {
        // Truncated: keep the line ending
        n = size - 1;
        if(size >= 3)
        {
            out[n - 2] = '\r';
            out[n - 1] = '\n';
        };
    };
    return n;
};
