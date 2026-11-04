// BlackBox basic example: logging to the serial port only (the default, no network code).
//
// Open the serial monitor at 115200 baud. Every line shows the time since boot, the level,
// file, function and line, and the message. Press the reset button: the log of the previous
// run is kept in RTC memory, followed by a "boot, reset reason" marker.

#include <Arduino.h>
#include "blackbox.h"

void setup()
{
    Serial.begin(115200);
    BlackBox.begin();                   // as early as possible

    INFO("Basic example started");
    WARNING("this is a warning");
    ERROR("this is an error, value %d", 42);
};

void loop()
{
    static uint32_t count = 0;
    DBG("loop %lu, free heap %lu", (unsigned long) count++, (unsigned long) ESP.getFreeHeap());
    VERBOSE("verbose messages show up too, unless filtered");

    // Type "level i" in the serial monitor to hide the debug and verbose lines again
    delay(1000);
};
