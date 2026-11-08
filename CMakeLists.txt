# ESP-IDF component definition (PlatformIO/Arduino builds use library.json)
idf_component_register(
    SRCS
        "src/bb_core.cpp"
        "src/bb_format.cpp"
        "src/bb_commands.cpp"
        "src/bb_port.cpp"
        "src/bb_net.cpp"
        "src/bb_sink_serial.cpp"
        "src/bb_sink_telnet.cpp"
        "src/bb_sink_syslog.cpp"
        "src/bb_sink_binary.cpp"
        "src/bb_coredump.cpp"
        "src/bb_print.cpp"
    INCLUDE_DIRS "src"
    REQUIRES esp_timer esp_system log lwip esp_netif espcoredump esp_app_format vfs
)
