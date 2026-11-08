#ifdef ARDUINO

#include "bb_print.h"
#include "bb_internal.h"

void BlackBoxPrint::commit()
{
    if(!_len)
        return;
    bb_store(_level, BB_SRC_PRINT, nullptr, nullptr, 0, _line, _len);
    _len = 0;
};

size_t BlackBoxPrint::write(uint8_t c)
{
    return write(&c, 1);
};

size_t BlackBoxPrint::write(const uint8_t* buf, size_t len)
{
    for(size_t i = 0; i < len; i++)
    {
        char c = (char) buf[i];
        if(c == '\r')
            continue;
        if(c == '\n')
        {
            commit();
            continue;
        };
        _line[_len++] = c;
        if(_len >= sizeof(_line))
            commit();
    };
    return len;
};

void BlackBoxPrint::flush()
{
    commit();
};

#endif // ARDUINO
