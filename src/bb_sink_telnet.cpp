#include "bb_internal.h"

#if BLACKBOX_SINK_TELNET

#include "bb_net.h"

#define TELNET_IAC  255
#define TELNET_SB   250
#define TELNET_SE   240
#define TELNET_WILL 251
#define TELNET_DONT 254

class bb_sink_telnet : public bb_sink
{
    public:
        const char* name() const override { return "telnet"; };

        void poll() override
        {
            // While nobody is connected the position stays put: a new client gets what it hasn't seen yet
            if(_server.poll_accept())
                _iac = 0;

            char in[32];
            int n;
            while((n = _server.receive(in, sizeof(in))) > 0)
                for(int i = 0; i < n; i++)
                    input((uint8_t) in[i]);

            if(!_server.connected())
                return;

            bb_record_t rec;
            char out[BLACKBOX_LINE_MAX + 96];
            while(next(rec))
            {
                if(!show(rec))
                    continue;
                size_t len = bb_format_text(rec, out, sizeof(out), BLACKBOX_TELNET_COLORS);
                if(!_server.transmit(out, len))
                    break;
            };
        };

        bool idle() override
        {
            return !_server.connected() || at_head();
        };

    private:
        // Strips telnet negotiation (IAC sequences) from the input
        void input(uint8_t c)
        {
            switch(_iac)
            {
                case 0:
                    if(c == TELNET_IAC)
                        _iac = 1;
                    else if(_reader.feed((char) c))
                        bb_command_line(this, _reader.line());
                    break;
                case 1:     // after IAC: WILL/WONT/DO/DONT take one option byte, SB starts a subnegotiation
                    if(c >= TELNET_WILL && c <= TELNET_DONT)
                        _iac = 2;
                    else if(c == TELNET_SB)
                        _iac = 3;
                    else
                        _iac = 0;
                    break;
                case 2:
                    _iac = 0;
                    break;
                case 3:     // inside SB, until IAC SE
                    if(c == TELNET_IAC)
                        _iac = 4;
                    break;
                case 4:
                    _iac = (c == TELNET_SE) ? 0 : 3;
                    break;
            };
        };

        bb_tcp_server _server{BLACKBOX_TELNET_PORT};
        bb_line_reader _reader;
        uint8_t _iac = 0;
};

static bb_sink_telnet s_sink;

bb_sink* bb_sink_telnet_get()
{
    return &s_sink;
};

#endif // BLACKBOX_SINK_TELNET
