# BlackBox

A logger for the ESP32 that behaves like a flight recorder. Log calls only store a record in a ring buffer; a background task sends the records to one or more outputs (serial, telnet, syslog, a binary TCP stream). The ring lives in RTC memory, so the last few kilobytes of log survive a crash, a watchdog reset or deep sleep, and are there to read after the reboot.

Written for Arduino-ESP32 (core 3.x); prepared for plain ESP-IDF.

## Features

- **Cheap log calls.** A log call formats its message and copies it into the ring. It never waits for a UART or the network, and is safe from any task.
- **Survives resets.** The ring is in `RTC_NOINIT` memory and survives panics, watchdog resets, software restarts and deep sleep (not power loss). Each boot adds a marker with the reset reason: `--- boot, reset reason: panic ---`.
- **Records, not just text.** Every message is stored with its level, source, timestamp, file, function and line. Each output formats them its own way.
- **Several outputs at once.** Serial, telnet (with colours), syslog (RFC 5424 over UDP), and a binary TCP stream for a dedicated client. Each output reads at its own pace.
- **Captures ESP-IDF messages** (`ESP_LOGx`) as records too.
- **Commands** over telnet and serial: built-in ones (`help`, `level`, `mem`, …) plus your own.
- **Core dump report:** logs the task, exception cause and backtrace of the last crash at boot, ready to decode with `addr2line` (optional).
- **Lean when you want it.** With only the serial output enabled, no network code is compiled in.

## Quick start (Arduino)

```cpp
#include <Arduino.h>
#include "blackbox.h"

void setup()
{
    Serial.begin(115200);
    BlackBox.begin();               // as early as possible; network outputs connect by themselves later

    INFO("Booting, version %s", "1.0");
};

void loop()
{
    BlackBox.handleCommands();      // runs your own commands, if you registered any
    DBG("tick %lu", millis());
    delay(1000);
};
```

Messages logged before `begin()` are kept and sent once `begin()` has run.

More in [`examples/`](examples). Each example is a small PlatformIO project (`platformio.ini` plus `src/main.cpp`) that uses the library from this folder; open the example folder in PlatformIO and build it.

| Example | Shows |
|---|---|
| [Basic](examples/Basic/src/main.cpp) | serial only, all levels, the `level` command |
| [Commands](examples/Commands/src/main.cpp) | commands of your own over serial |
| [Telnet](examples/Telnet/src/main.cpp) | logging over WiFi, settings as build flags in [`platformio.ini`](examples/Telnet/platformio.ini), and reading the log of a deliberate crash after the reboot |

## Logging macros

| Macro |
|---|
| `ERROR(fmt, ...)` |
| `WARNING(fmt, ...)` |
| `INFO(fmt, ...)` |
| `DBG(fmt, ...)` |
| `VERBOSE(fmt, ...)` |

They take a `printf` format and arguments, and add file, function and line automatically. The compiler checks the format against the arguments.

`BLACKBOX_LEVEL` removes the macros that are more verbose than that level at compile time: with `BLACKBOX_LEVEL 3`, `ERROR`, `WARNING` and `INFO` stay, and `DBG` and `VERBOSE` compile to nothing. The `level` command filters the same way per output at runtime.

On Arduino, `BlackBox` is also a `Print`: `BlackBox.print()`, `println()` and `printf()` work and are logged at info level, one record per line. Use them from one task at a time.

## API

```cpp
BlackBox.begin();                                   // starts the dispatcher task, the outputs and the ESP-IDF hook
BlackBox.log(level, file, func, line, fmt, ...);    // what the macros call
BlackBox.flush(timeout_ms);                         // waits until every output has sent everything; false on timeout
BlackBox.flush();                                   // the same, with a 500 ms timeout
BlackBox.clear();                                   // empties the ring
BlackBox.addCommand(name, handler, help);           // registers a command of your own
BlackBox.handleCommands();                          // call from loop(): runs queued commands
```

Call `BlackBox.flush()` before doing something that might crash, to get everything out live. Whatever doesn't make it out is still in the ring after the reset.

## Configuration

Set these in your project's `config.h` (included automatically if it exists) or as `-D` build flags.

| Define | Default | Meaning |
|---|---|---|
| `BLACKBOX_BUFSIZE` | `4096` | ring size in bytes |
| `BLACKBOX_RTC` | `1` | keep the ring in RTC memory |
| `BLACKBOX_LINE_MAX` | `256` | longest message text |
| `BLACKBOX_LEVEL` | `5` | compile-time level; macros more verbose than this are removed |
| `BLACKBOX_ESPLOG` | `1` | capture ESP-IDF `ESP_LOGx` output |
| `BLACKBOX_SINK_SERIAL` | `1` | serial output |
| `BLACKBOX_SERIAL_COLORS` | `0` | ANSI colours on serial |
| `BLACKBOX_SERIAL_COMMANDS` | `1` | read commands from serial |
| `BLACKBOX_SINK_TELNET` | `0` | telnet output |
| `BLACKBOX_TELNET_PORT` | `23` | |
| `BLACKBOX_TELNET_COLORS` | `1` | ANSI colours on telnet |
| `BLACKBOX_SINK_SYSLOG` | `0` | syslog output |
| `BLACKBOX_SYSLOG_SERVER` | `""` | syslog server IPv4 address as a string, e.g. `"192.168.1.10"` |
| `BLACKBOX_SYSLOG_PORT` | `514` | |
| `BLACKBOX_SINK_BINARY` | `0` | binary TCP output |
| `BLACKBOX_BINARY_PORT` | `2323` | |
| `BLACKBOX_HOSTNAME` | `"esp32"` | name used in syslog and the binary header |
| `BLACKBOX_COREDUMP_REPORT` | `0` | log the last crash's core dump at boot |
| `BLACKBOX_COREDUMP_ERASE` | `0` | `0`: keep the dump and report it at every boot; `1`: erase it after reporting (reported once) |
| `BLACKBOX_MAX_COMMANDS` | `16` | number of commands of your own |
| `BLACKBOX_CMD_QUEUE` | `4` | queued commands waiting for `handleCommands()` |
| `BLACKBOX_CMD_LEN` | `64` | longest command line |
| `BLACKBOX_TASK_CORE` | `0` | core of the dispatcher task |
| `BLACKBOX_TASK_PRIO` | `1` | priority of the dispatcher task |
| `BLACKBOX_TASK_STACK` | `4096` | stack size of the dispatcher task |

Example `config.h` for serial plus telnet:

```cpp
#define BLACKBOX_SINK_TELNET    1
#define BLACKBOX_HOSTNAME       "mydevice"
```

## Outputs

### Serial
Writes formatted lines to the serial port, and reads command lines from it. Messages are sent from the dispatcher task, not from the logging task. If the board crashes, the last messages may not have reached the serial port yet, but they are still in the ring.

Colours need an ANSI-capable terminal. PlatformIO's serial monitor strips control codes with its default filter; `monitor_filters = direct` keeps them.

### Telnet
A telnet server for one client at a time; a new connection replaces the old one. On connect, the client first gets everything still in the ring (including previous boots), then everything live. Connect with `telnet <ip>` or PuTTY (Telnet or Raw).

The server opens its port by itself once the network is up, so `begin()` can run before WiFi is connected.

### Syslog
Sends every record as one UDP datagram in RFC 5424 format to `BLACKBOX_SYSLOG_SERVER`, e.g. a Synology (Log Center), rsyslog or Graylog. There is no timestamp in the message; the server records the time of arrival. While the network isn't up yet, records wait in the ring and are sent once it is.

### Binary
A TCP server (one client) for a dedicated client program that does its own colouring and filtering. Little endian:

```
on connect:   "BBX"  version(u8) = 1  elf_sha(8 chars)  hostname_len(u8)  hostname
per record:   total_len(u16)  level(u8)  source(u8)  time_ms(u32)  line(u16)
              file_len(u8) file  func_len(u8) func  text_len(u16) text
```

`source` is 0 = application macros, 1 = ESP-IDF, 2 = `print()`, 3 = BlackBox itself. Text lines sent by the client are handled as commands.

## Text format

```
  12.345 E main.cpp:handle_ir(301) Crash test: writing to address 0
  12.401 W (12401) wifi: something from ESP-IDF
  13.002 --- boot, reset reason: panic ---
```

Seconds.milliseconds since boot, a level letter, file without path, function(line), and the message. ESP-IDF lines keep their own prefix; BlackBox's own messages are shown between `---`.

Telnet colours: errors red, warnings yellow, debug grey, verbose dim, BlackBox messages magenta.

## Commands

Typed over telnet, serial or the binary connection, one per line.

| Command | Effect |
|---|---|
| `help` | lists all commands |
| `level <v\|d\|i\|w\|e>` | sets the level for the output you type it in |
| `mem` | free heap and lowest free heap |
| `clear` | empties the ring |
| `replay` | sends the whole ring again (on this output) |
| `reboot` | restarts the ESP |

Replies are logged as BlackBox messages, so they appear on all outputs.

### Commands of your own

A command is a name plus a handler function that gets everything typed after the name as one string (`args`, empty if nothing was typed). Register commands in `setup()`, and call `BlackBox.handleCommands()` from `loop()`:

```cpp
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
};

void loop()
{
    BlackBox.handleCommands();      // runs the commands typed since the last call
    // ... the rest of your loop
};
```

A telnet session then looks like this:

```
help
  10.120 --- commands: help, level <v|d|i|w|e>, mem, clear, replay, reboot ---
  10.120 ---   interval - [ms]: show or set the heartbeat interval ---
  10.120 ---   status - uptime and heartbeat interval ---
interval 500
  12.503 I main.cpp:cmd_interval(10) heartbeat interval 500 ms
status
  15.871 I main.cpp:cmd_status(16) uptime 15 s, heartbeat interval 500 ms
```

How it works:

- Typed commands are queued and run by `BlackBox.handleCommands()`, so your handlers run in your own task (normally `loop()`), not in a different task from the rest of your code. You don't need extra locking for anything `loop()` already uses.
- If `handleCommands()` isn't called, typed commands wait in the queue (`BLACKBOX_CMD_QUEUE`, default 4). When it's full, you get "command queue full".
- A handler replies by logging. The output appears on every output, with the handler's file, function and line.
- `name` and `help` must stay valid for the program's lifetime; string literals are fine. Up to `BLACKBOX_MAX_COMMANDS` (16) commands, command lines up to `BLACKBOX_CMD_LEN` (64) characters.
- The built-in commands (`help`, `level`, …) run directly in the dispatcher task and don't need `handleCommands()`.

## Crashes and the core dump

After a crash, connect and you'll see the log leading up to the crash, followed by the boot marker with the reset reason.

The backtrace itself is printed by the panic handler straight to the UART and is not in the ring. If your core writes core dumps to flash (Arduino-ESP32 3.x does by default, with a `coredump` partition in the partition table), set `BLACKBOX_COREDUMP_REPORT 1` to log the crash at the next boot:

```
Last core dump (from the previous boot, this firmware, ELF sha256 1a2b3c4d): task 'loopTask'
Exception: StoreProhibited (cause 29), PC 0x400d2f1a, address 0x00000000
A0  0x800d3b44  A1  0x3ffb2270  A2  0x00000000  A3  0x00000001
A4  0x3ffc1a2c  A5  0x00000000  A6  0x00000000  A7  0x00000000
A8  0x800d2f18  A9  0x3ffb2250  A10 0x00000000  A11 0x3ffc1b10
A12 0x00000001  A13 0x00000000  A14 0x00000000  A15 0x00000000
EPC1 0x400d2f1a
Backtrace: 0x400d2f1a 0x400d3b44 0x400e1c20 ...
```

### Reading the report

- **task:** the FreeRTOS task that crashed, e.g. `loopTask` for Arduino's `loop()`.
- **PC:** the instruction that crashed.
- **A0–A15:** the CPU registers at the moment of the crash. A0 usually holds the return address (with the top bits set: `0x800d3b44` means `0x400d3b44`), A1 is the stack pointer; the others depend on the code. Registers holding `0x0` next to a cause 28/29 often point to the NULL pointer.
- **EPC1…:** the address of the instruction where each (nested) exception happened; EPC1 is normally the same as PC.
- **exception cause:** the Xtensa exception code, with the name ESP-IDF's panic handler uses. The common ones:

  | Cause | Name | Usually means |
  |---|---|---|
  | 0 | IllegalInstruction | jumped to garbage, e.g. a corrupted function pointer |
  | 3 | LoadStoreError | 8- or 16-bit access to memory that only allows 32-bit access |
  | 6 | IntegerDivideByZero | division by zero |
  | 9 | LoadStoreAlignment | unaligned access |
  | 28 | LoadProhibited | reading through a bad pointer (often NULL) |
  | 29 | StoreProhibited | writing through a bad pointer (often NULL) |

- **address:** the memory address that was accessed, for causes 3, 9, 28 and 29. `0x00000000` or a small value means a NULL pointer, possibly plus a struct offset.
- **Backtrace:** the call chain of the crashed task, newest call first, up to 16 levels. `(corrupted)` means the stack was damaged, so the list is incomplete.

For a failed `assert()`, `abort()` or a watchdog, the first entries are in the panic and abort code itself; your own code is further down the list.

### Decoding the backtrace

The backtrace holds addresses. `addr2line` turns them into function names and source lines, using the ELF of the exact firmware that crashed:

```
xtensa-esp32-elf-addr2line -pfiaC -e firmware.elf 0x400d2f1a 0x400d3b44 0x400e1c20
```

```
0x400d2f1a: cmd_crash(char const*) at src/main.cpp:20
0x400d3b44: BlackBoxClass::handleCommands() at lib/lib-blackbox/src/bb_commands.cpp:162
0x400e1c20: loop() at src/main.cpp:37
```

In a PlatformIO project:

- **`addr2line`** comes with the toolchain: `~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-addr2line` (on Windows `%USERPROFILE%\.platformio\packages\…\xtensa-esp32-elf-addr2line.exe`).
- **`firmware.elf`** is in the build folder: `.pio/build/<env>/firmware.elf`, or under `build_dir` if you set one.
- **Keep the ELF of every firmware you ship or flash.** The report's ELF hash tells you which one you need; with a different ELF the addresses point to the wrong functions. Rebuilding the same source usually gives the same ELF, but not guaranteed.

On a serial cable you don't need any of this: PlatformIO's monitor with `monitor_filters = esp32_exception_decoder` decodes the panic output live.

The report is about the last crash that was stored, not necessarily one from the previous boot. It says "from the previous boot" when this boot's reset reason is a panic or watchdog, and "from an earlier boot" otherwise.

What happens to the dump after the report:

- `BLACKBOX_COREDUMP_ERASE 0` (default): it is kept and reported at every boot, until the next crash replaces it. No flash writes.
- `BLACKBOX_COREDUMP_ERASE 1`: it is erased, so each crash is reported once, followed by "Core dump erased.". Erasing is a flash write.
- A dump from another firmware (different ELF hash) is not reported: its addresses only decode with that firmware's ELF. You get one warning, `Core dump from another firmware (ELF sha256 …) found, erased`, and it is erased. An unreadable dump is always erased too.

While the report is off, nothing reads or erases the dump: the last crash stays in flash until you turn the report on.

The ESP-IDF panic handler writes the dump on every crash, regardless of these settings.

## How it works

```
any task ─► DBG()/INFO()/…  ─► record ─► ring buffer ─► dispatcher task ─┬─► serial
ESP_LOGx ─► vprintf hook ──────┘          (RTC memory)    (core 0, low prio) ├─► telnet
BlackBox.print() ──────────────┘                                             ├─► syslog
                                                                             └─► binary
```

- A record is an 18-byte header (length, level, source, time, file and function pointers, line) plus the formatted text.
- File and function are stored as pointers to the string literals in flash. Those addresses change with every new firmware, so the ring header stores the firmware's ELF hash and the ring is cleared when it changes.
- When the ring is full, the oldest records are dropped. Each output keeps its own read position; an output that falls too far behind gets a "bytes of log lost" message instead of garbled records.
- The dispatcher task wakes on every new record, and every 50 ms to handle input and new connections. It runs on core 0 by default, so a busy hang in Arduino's `loop()` (core 1) doesn't stop the log.

## ESP-IDF

The core, the commands, the core dump report and all network outputs use only FreeRTOS, ESP-IDF and lwIP. Arduino is only used for the serial port (`bb_port.cpp`) and the `Print` base class. Both have an ESP-IDF branch: serial uses the console on `stdin`/`stdout`. `CMakeLists.txt` registers the library as an ESP-IDF component.

The ESP-IDF build has not been tested yet.

## Limitations

- Messages longer than `BLACKBOX_LINE_MAX` are cut off.
- The ring holds roughly 50 to 100 messages with the default 4 KB, depending on their length.
- After a brownout, the RTC memory may be partly corrupted; the checks catch a fully random buffer, not necessarily a few flipped bytes.
- One client at a time on telnet and on binary.
- The panic backtrace is only available through the core dump report or a serial cable.
