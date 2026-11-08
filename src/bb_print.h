#ifndef __BB_PRINT_H
#define __BB_PRINT_H

#ifdef ARDUINO

#include <Print.h>
#include "blackbox.h"

// Arduino Print that logs one record per line, at a fixed level.
// An instance belongs to one task: it keeps its own line buffer and takes no lock. A task may have several.
class BlackBoxPrint : public Print
{
    public:
        BlackBoxPrint(bb_level_t level = BB_INFO) : _level(level) {};

        using Print::write;
        size_t write(uint8_t c) override;
        size_t write(const uint8_t* buf, size_t len) override;
        void flush() override;              // stores a partial line; BlackBox.flush() waits for the outputs

    private:
        void commit();

        const bb_level_t _level;
        char _line[BLACKBOX_LINE_MAX];
        size_t _len = 0;
};

#endif // ARDUINO
#endif // __BB_PRINT_H
