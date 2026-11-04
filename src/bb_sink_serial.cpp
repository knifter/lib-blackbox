#include "bb_internal.h"

#if BLACKBOX_SINK_SERIAL

#include "bb_port.h"

class bb_sink_serial : public bb_sink
{
    public:
        const char* name() const override { return "serial"; };

        void begin() override
        {
            bb_port_serial_begin();
        };

        void poll() override
        {
#if BLACKBOX_SERIAL_COMMANDS
            char in[32];
            int n;
            while((n = bb_port_serial_read(in, sizeof(in))) > 0)
                for(int i = 0; i < n; i++)
                    if(_reader.feed(in[i]))
                        bb_command_line(this, _reader.line());
#endif
            bb_record_t rec;
            char out[BLACKBOX_LINE_MAX + 96];
            while(next(rec))
            {
                if(!show(rec))
                    continue;
                size_t len = bb_format_text(rec, out, sizeof(out), BLACKBOX_SERIAL_COLORS);
                bb_port_serial_write(out, len);
            };
        };

        void sync() override
        {
            bb_port_serial_sync();
        };

    private:
#if BLACKBOX_SERIAL_COMMANDS
        bb_line_reader _reader;
#endif
};

static bb_sink_serial s_sink;

bb_sink* bb_sink_serial_get()
{
    return &s_sink;
};

#endif // BLACKBOX_SINK_SERIAL
