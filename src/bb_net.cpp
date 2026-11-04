#include "bb_internal.h"

#if BLACKBOX_SINK_TELNET || BLACKBOX_SINK_BINARY

#include "bb_net.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <lwip/sockets.h>

// Retried every second until the network is up and the port is open
void bb_tcp_server::open_listener()
{
    uint32_t now = bb_now_ms();
    if(_listener >= 0 || !bb_net_ready() || (_tried && now - _last_try < 1000))
        return;
    _tried = true;
    _last_try = now;

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0)
        return;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0 || listen(fd, 1) < 0)
    {
        close(fd);
        return;
    };
    fcntl(fd, F_SETFL, O_NONBLOCK);
    _listener = fd;
};

bool bb_tcp_server::poll_accept()
{
    open_listener();
    if(_listener < 0)
        return false;

    int fd = accept(_listener, nullptr, nullptr);
    if(fd < 0)
        return false;

    drop();
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct timeval tv = { 1, 0 };       // a stalled client can block the dispatcher for at most this long
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    _client = fd;
    return true;
};

int bb_tcp_server::receive(char* buf, size_t max)
{
    if(_client < 0)
        return -1;
    int n = recv(_client, buf, max, MSG_DONTWAIT);
    if(n > 0)
        return n;
    if(n == 0 || (errno != EWOULDBLOCK && errno != EAGAIN))
    {
        drop();
        return -1;
    };
    return 0;
};

bool bb_tcp_server::transmit(const void* buf, size_t len)
{
    const uint8_t* p = (const uint8_t*) buf;
    while(len)
    {
        if(_client < 0)
            return false;
        int n = send(_client, p, len, 0);
        if(n <= 0)
        {
            drop();
            return false;
        };
        p += n;
        len -= n;
    };
    return true;
};

void bb_tcp_server::drop()
{
    if(_client >= 0)
    {
        close(_client);
        _client = -1;
    };
};

#endif // BLACKBOX_SINK_TELNET || BLACKBOX_SINK_BINARY
