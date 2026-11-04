#ifndef __BB_NET_H
#define __BB_NET_H

// Single-client TCP server on plain lwIP sockets (works the same in Arduino and ESP-IDF)

#include <stdint.h>
#include <stddef.h>
#include <esp_netif.h>

// lwIP socket calls assert (instead of failing) before esp_netif_init() has started the TCP/IP
// stack, which happens when WiFi or Ethernet is started. Network interfaces only exist after that.
static inline bool bb_net_ready()
{
    return esp_netif_get_nr_of_ifs() > 0;
}

class bb_tcp_server
{
    public:
        bb_tcp_server(uint16_t port) : _port(port) {};

        bool poll_accept();                         // true when a new client connected (replaces the old one)
        bool connected() const { return _client >= 0; };
        int  receive(char* buf, size_t max);        // non-blocking: bytes read, 0 for none, -1 when the client is gone
        bool transmit(const void* buf, size_t len); // false: the client was dropped
        void drop();

    private:
        void open_listener();

        uint16_t _port;
        int _listener = -1;
        int _client = -1;
        uint32_t _last_try = 0;
        bool _tried = false;
};

#endif // __BB_NET_H
