// BlackBox telnet example: log over WiFi, and read the log of a crash after the reboot.
//
// The telnet output is enabled in this example's platformio.ini. Fill in your WiFi below,
// then connect with "telnet <ip of the board>" or PuTTY. You first get everything still in the
// ring, then the live log. Type "crash" to crash the board on purpose: after the reboot,
// connect again and you'll see the lines leading up to the crash, followed by
// "--- boot, reset reason: panic ---".

#include <Arduino.h>
#include <WiFi.h>
#include "blackbox.h"

#define WIFI_SSID   "your-ssid"
#define WIFI_PASS   "your-password"

void cmd_crash(const char* args)
{
    ERROR("crashing on purpose");
    // No BlackBox.flush() here: the point is that this line survives in the ring anyway
    *(volatile int*) 0 = 0;
};

void setup()
{
    Serial.begin(115200);
    BlackBox.begin();                   // before WiFi: the telnet port opens once the network is up

    INFO("Telnet example started, connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    BlackBox.addCommand("crash", cmd_crash, "crash the board on purpose");
};

void loop()
{
    BlackBox.handleCommands();

    static bool connected = false;
    if(WiFi.isConnected() != connected)
    {
        connected = WiFi.isConnected();
        if(connected)
            INFO("WiFi connected, telnet to %s", WiFi.localIP().toString().c_str());
        else
            WARNING("WiFi disconnected");
    };

    static uint32_t last = 0;
    if(millis() - last >= 2000)
    {
        last = millis();
        DBG("still alive, uptime %lu s", millis() / 1000);
    };
    delay(10);
};
