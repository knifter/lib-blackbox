#include "bb_internal.h"

#if BLACKBOX_SINK_SYSLOG

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include "bb_net.h"

// RFC 5424 over UDP, one record per datagram
class bb_sink_syslog : public bb_sink
{
    public:
        const char* name() const override { return "syslog"; };

        void begin() override
        {
            memset(&_dest, 0, sizeof(_dest));
            _dest.sin_family = AF_INET;
            _dest.sin_port = htons(BLACKBOX_SYSLOG_PORT);
            _valid = inet_aton(BLACKBOX_SYSLOG_SERVER, &_dest.sin_addr) != 0;
        };

        void poll() override
        {
            if(!_valid)
            {
                set_cursor(bb_head());  // no server configured
                return;
            };

            uint32_t now = bb_now_ms();
            if(_fd < 0)
            {
                if(!bb_net_ready() || (_tried && now - _last_try < 1000))
                    return;
                _tried = true;
                _last_try = now;
                _fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if(_fd < 0)
                    return;
            };

            bb_record_t rec;
            char msg[BLACKBOX_LINE_MAX + 128];
            for(;;)
            {
                uint32_t before = cursor();
                if(!next(rec))
                    break;
                if(!show(rec))
                    continue;
                int len = format(rec, msg, sizeof(msg));
                if(len <= 0)
                    continue;
                if(sendto(_fd, msg, len, 0, (struct sockaddr*) &_dest, sizeof(_dest)) < 0)
                {
                    // Network not up (yet): keep this record and retry in a second
                    set_cursor(before);
                    close(_fd);
                    _fd = -1;
                    _last_try = now;
                    break;
                };
            };
        };

        bool idle() override
        {
            return !_valid || _fd < 0 || at_head();
        };

    private:
        int format(const bb_record_t& rec, char* out, size_t size)
        {
            uint8_t severity;
            switch(rec.level)
            {
                case BB_ERROR:      severity = 3; break;
                case BB_WARNING:    severity = 4; break;
                case BB_INFO:       severity = 6; break;
                default:            severity = 7; break;
            };
            const uint8_t facility = 1;     // user-level messages

            const char* app;
            switch(rec.source)
            {
                case BB_SRC_APP:    app = "app"; break;
                case BB_SRC_ESPIDF: app = "esp-idf"; break;
                case BB_SRC_PRINT:  app = "print"; break;
                default:            app = "blackbox"; break;
            };

            // No timestamp ("-"): the server stamps the time of arrival
            int n;
            if(rec.source == BB_SRC_APP)
                n = snprintf(out, size, "<%u>1 - %s %s - - - %" PRIu32 ".%03" PRIu32 " %s:%s(%u) %.*s",
                    facility * 8 + severity, BLACKBOX_HOSTNAME, app, rec.time_ms / 1000, rec.time_ms % 1000,
                    bb_basename(rec.file), rec.func ? rec.func : "", rec.line, (int) rec.text_len, rec.text);
            else
                n = snprintf(out, size, "<%u>1 - %s %s - - - %" PRIu32 ".%03" PRIu32 " %.*s",
                    facility * 8 + severity, BLACKBOX_HOSTNAME, app, rec.time_ms / 1000, rec.time_ms % 1000,
                    (int) rec.text_len, rec.text);
            if(n < 0)
                return n;
            return BB_MIN((size_t) n, size - 1);
        };

        struct sockaddr_in _dest;
        bool _valid = false;
        int _fd = -1;
        uint32_t _last_try = 0;
        bool _tried = false;
};

static bb_sink_syslog s_sink;

bb_sink* bb_sink_syslog_get()
{
    return &s_sink;
};

#endif // BLACKBOX_SINK_SYSLOG
