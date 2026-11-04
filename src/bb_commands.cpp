#include "bb_internal.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_system.h>

typedef struct
{
    const char* name;
    bb_command_handler_t handler;
    const char* help;
} bb_command_t;

static bb_command_t s_commands[BLACKBOX_MAX_COMMANDS];
static int s_ncommands = 0;
static QueueHandle_t s_queue = nullptr;

// Splits "name args" in place: returns the args (possibly empty)
static char* split(char* line)
{
    char* p = line;
    while(*p && *p != ' ')
        p++;
    if(!*p)
        return p;
    *p++ = 0;
    while(*p == ' ')
        p++;
    return p;
};

static const bb_command_t* find(const char* name)
{
    for(int i = 0; i < s_ncommands; i++)
        if(strcmp(s_commands[i].name, name) == 0)
            return &s_commands[i];
    return nullptr;
};

void bb_commands_begin()
{
    if(!s_queue)
        s_queue = xQueueCreate(BLACKBOX_CMD_QUEUE, BLACKBOX_CMD_LEN);
};

// Called by the sinks, in the dispatcher task: built-ins run here, application commands are queued
void bb_command_line(bb_sink* from, const char* line)
{
    while(*line == ' ')
        line++;
    if(!*line)
        return;

    char buf[BLACKBOX_CMD_LEN];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* args = split(buf);

    if(strcmp(buf, "help") == 0)
    {
        bb_say("commands: help, level <v|d|i|w|e>, mem, clear, replay, reboot");
        for(int i = 0; i < s_ncommands; i++)
            bb_say("  %s%s%s", s_commands[i].name, s_commands[i].help ? " - " : "", s_commands[i].help ? s_commands[i].help : "");
    }
    else if(strcmp(buf, "level") == 0)
    {
        bb_level_t level;
        switch(args[0])
        {
            case 'v': level = BB_VERBOSE; break;
            case 'd': level = BB_DEBUG; break;
            case 'i': level = BB_INFO; break;
            case 'w': level = BB_WARNING; break;
            case 'e': level = BB_ERROR; break;
            default:
                bb_say("%s level is %c, use: level <v|d|i|w|e>", from->name(), bb_level_char(from->level));
                return;
        };
        from->level = level;
        bb_say("%s level set to %c", from->name(), bb_level_char(level));
    }
    else if(strcmp(buf, "mem") == 0)
    {
        bb_say("free heap %u bytes, lowest %u bytes", (unsigned) esp_get_free_heap_size(), (unsigned) esp_get_minimum_free_heap_size());
    }
    else if(strcmp(buf, "clear") == 0)
    {
        BlackBox.clear();
    }
    else if(strcmp(buf, "replay") == 0)
    {
        from->replay();
    }
    else if(strcmp(buf, "reboot") == 0)
    {
        bb_say("rebooting");
        bb_poll_all();                  // this is the dispatcher: send it out ourselves
        esp_restart();
    }
    else if(find(buf))
    {
        // The queue copies a full BLACKBOX_CMD_LEN item
        char item[BLACKBOX_CMD_LEN];
        strncpy(item, line, sizeof(item) - 1);
        item[sizeof(item) - 1] = 0;
        if(!s_queue || xQueueSend(s_queue, item, 0) != pdTRUE)
            bb_say("command queue full, is handleCommands() being called?");
    }
    else
    {
        bb_say("unknown command '%s', try help", buf);
    };
};

bool BlackBoxClass::addCommand(const char* name, bb_command_handler_t handler, const char* help)
{
    if(s_ncommands >= BLACKBOX_MAX_COMMANDS || !name || !handler)
        return false;
    s_commands[s_ncommands].name = name;
    s_commands[s_ncommands].handler = handler;
    s_commands[s_ncommands].help = help;
    s_ncommands++;
    return true;
};

void BlackBoxClass::handleCommands()
{
    if(!s_queue)
        return;

    char line[BLACKBOX_CMD_LEN];
    while(xQueueReceive(s_queue, line, 0) == pdTRUE)
    {
        line[sizeof(line) - 1] = 0;
        char* args = split(line);
        const bb_command_t* cmd = find(line);
        if(cmd)
            cmd->handler(args);
    };
};
