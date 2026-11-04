// BlackBox commands example: commands of your own, typed in the serial monitor.
//
// Type "help" to list the commands, "interval 500" to change how often the heartbeat is logged,
// "status" for the uptime. Make sure the serial monitor sends a line ending (CR, LF or both).

#include <Arduino.h>
#include "blackbox.h"

uint32_t g_interval = 2000;             // ms between heartbeat messages

// "interval" shows the interval, "interval 500" sets it
void cmd_interval(const char* args)
{
    if(*args)
        g_interval = strtoul(args, nullptr, 10);
    INFO("heartbeat interval %lu ms", (unsigned long) g_interval);
};

// "status": no arguments needed
void cmd_status(const char* args)
{
    INFO("uptime %lu s, heartbeat interval %lu ms", millis() / 1000, (unsigned long) g_interval);
};

void setup()
{
    Serial.begin(115200);
    BlackBox.begin();

    BlackBox.addCommand("interval", cmd_interval, "[ms]: show or set the heartbeat interval");
    BlackBox.addCommand("status", cmd_status, "uptime and heartbeat interval");

    INFO("Commands example started, type help");
};

void loop()
{
    // Runs the commands typed since the last call, in this task
    BlackBox.handleCommands();

    static uint32_t last = 0;
    if(g_interval && millis() - last >= g_interval)
    {
        last = millis();
        DBG("heartbeat");
    };
    delay(10);
};
