#include "bb_internal.h"

#if BLACKBOX_SINK_BINARY

#include <string.h>
#include "bb_net.h"

// Wire format, little endian:
//   on connect:  "BBX" version(u8)=1  elf_sha(8 chars)  hostname_len(u8) hostname
//   per record:  total_len(u16)  level(u8)  source(u8)  time_ms(u32)  line(u16)
//                file_len(u8) file  func_len(u8) func  text_len(u16) text
// Lines of text sent by the client are handled as commands.
class bb_sink_binary : public bb_sink
{
    public:
        const char* name() const override { return "binary"; };

        void poll() override
        {
            // While nobody is connected the position stays put: a new client gets what it hasn't seen yet
            if(_server.poll_accept())
                send_header();

            char in[32];
            int n;
            while((n = _server.receive(in, sizeof(in))) > 0)
                for(int i = 0; i < n; i++)
                    if(_reader.feed(in[i]))
                        bb_command_line(this, _reader.line());

            if(!_server.connected())
                return;

            bb_record_t rec;
            uint8_t out[BLACKBOX_LINE_MAX + 2 * 255 + 32];
            while(next(rec))
            {
                if(!show(rec))
                    continue;
                size_t len = encode(rec, out);
                if(!_server.transmit(out, len))
                    break;
            };
        };

        bool idle() override
        {
            return !_server.connected() || at_head();
        };

    private:
        static uint8_t* put8(uint8_t* p, uint8_t v)
        {
            *p++ = v;
            return p;
        };

        static uint8_t* put16(uint8_t* p, uint16_t v)
        {
            *p++ = v & 0xFF;
            *p++ = v >> 8;
            return p;
        };

        static uint8_t* put32(uint8_t* p, uint32_t v)
        {
            p = put16(p, v & 0xFFFF);
            return put16(p, v >> 16);
        };

        static uint8_t* put_str8(uint8_t* p, const char* s)
        {
            size_t len = s ? BB_MIN(strlen(s), (size_t) 255) : 0;
            p = put8(p, (uint8_t) len);
            memcpy(p, s, len);
            return p + len;
        };

        size_t encode(const bb_record_t& rec, uint8_t* out)
        {
            uint8_t* p = out + 2;
            p = put8(p, rec.level);
            p = put8(p, rec.source);
            p = put32(p, rec.time_ms);
            p = put16(p, rec.line);
            p = put_str8(p, rec.file);
            p = put_str8(p, rec.func);
            p = put16(p, rec.text_len);
            memcpy(p, rec.text, rec.text_len);
            p += rec.text_len;
            put16(out, (uint16_t)(p - out));
            return p - out;
        };

        void send_header()
        {
            uint8_t hdr[4 + 8 + 1 + 255];
            uint8_t* p = hdr;
            memcpy(p, "BBX", 3);
            p += 3;
            p = put8(p, 1);
            memcpy(p, bb_elf_sha(), 8);
            p += 8;
            p = put_str8(p, BLACKBOX_HOSTNAME);
            _server.transmit(hdr, p - hdr);
        };

        bb_tcp_server _server{BLACKBOX_BINARY_PORT};
        bb_line_reader _reader;
};

static bb_sink_binary s_sink;

bb_sink* bb_sink_binary_get()
{
    return &s_sink;
};

#endif // BLACKBOX_SINK_BINARY
