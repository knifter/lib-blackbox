#ifndef __BB_INTERNAL_H
#define __BB_INTERNAL_H

// Shared between the BlackBox source files; not for applications

#include "blackbox.h"

#define BB_MIN(a, b)    ((a) < (b) ? (a) : (b))

typedef struct
{
    bb_level_t level;
    bb_source_t source;
    uint32_t time_ms;
    const char* file;
    const char* func;
    uint16_t line;
    const char* text;
    uint16_t text_len;
} bb_record_t;

// An output. Positions are monotonic byte counters into the ring, compared with wrap-around
class bb_sink
{
    public:
        virtual ~bb_sink() {};
        virtual const char* name() const = 0;
        virtual void begin() {};
        virtual void poll() = 0;                    // dispatcher: send new records, read input
        virtual bool idle() { return at_head(); };  // flush(): nothing left to send
        virtual void sync() {};                     // flush(): wait until sent data has really left

        void replay();                              // start again at the oldest record in the ring
        void set_cursor(uint32_t pos) { *_cursor = pos; };
        uint32_t cursor() const { return *_cursor; };
        void attach_cursor(uint32_t* storage) { _cursor = storage; };   // bb_core: keeps it in the RTC buffer

        bb_level_t level = BB_VERBOSE;              // runtime filter, set with the "level" command

    protected:
        bool next(bb_record_t& rec);                // false: nothing new
        bool at_head();
        bool show(const bb_record_t& rec) const { return rec.source == BB_SRC_SYSTEM || rec.level <= level; };

        uint32_t _cursor_local = 0;
        uint32_t* _cursor = &_cursor_local;         // read position; points into the RTC buffer once added
        char _text[BLACKBOX_LINE_MAX];
};

// Assembles command lines from single characters
class bb_line_reader
{
    public:
        bool feed(char c)                           // true: a complete line is in line()
        {
            if(c == '\r' || c == '\n')
            {
                if(_n == 0)
                    return false;
                _buf[_n] = 0;
                _n = 0;
                return true;
            };
            if(c == 8 || c == 127)
            {
                if(_n)
                    _n--;
                return false;
            };
            if((uint8_t)c < 32)
                return false;
            if(_n < sizeof(_buf) - 1)
                _buf[_n++] = c;
            return false;
        };
        const char* line() const { return _buf; };

    private:
        char _buf[BLACKBOX_CMD_LEN];
        size_t _n = 0;
};

// bb_core.cpp
void bb_store(bb_level_t level, bb_source_t source, const char* file, const char* func, uint16_t line, const char* text, size_t len);
void bb_say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   // a BB_SRC_SYSTEM record
bool bb_read(uint32_t& cursor, bb_record_t& rec, char* text, size_t text_size, uint32_t& lost);
uint32_t bb_head();
uint32_t bb_tail();
uint32_t bb_now_ms();
const char* bb_elf_sha();                           // first 8 hex characters of the firmware ELF hash
void bb_poll_all();

// bb_format.cpp
char bb_level_char(bb_level_t level);
const char* bb_basename(const char* path);
size_t bb_format_text(const bb_record_t& rec, char* out, size_t size, bool colors);

// bb_commands.cpp
void bb_commands_begin();
void bb_command_line(bb_sink* from, const char* line);

// bb_coredump.cpp
void bb_coredump_report();

// Sink instances, each in its own file
#if BLACKBOX_SINK_SERIAL
bb_sink* bb_sink_serial_get();
#endif
#if BLACKBOX_SINK_TELNET
bb_sink* bb_sink_telnet_get();
#endif
#if BLACKBOX_SINK_SYSLOG
bb_sink* bb_sink_syslog_get();
#endif
#if BLACKBOX_SINK_BINARY
bb_sink* bb_sink_binary_get();
#endif

#endif // __BB_INTERNAL_H
