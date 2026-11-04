#include "bb_internal.h"

#if BLACKBOX_COREDUMP_REPORT && !(defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF))
    #warning "BLACKBOX_COREDUMP_REPORT is set, but this core has no ELF core dumps to flash: the report does nothing"
#endif

#if BLACKBOX_COREDUMP_REPORT && defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF)

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_core_dump.h>

// Logs the summary of a core dump in flash (task, cause, backtrace), then erases it so it is
// reported only once
void bb_coredump_report()
{
    if(esp_core_dump_image_check() != ESP_OK)
        return;     // no valid dump in flash

    // Too big for a comfortable stack allocation
    esp_core_dump_summary_t* sum = (esp_core_dump_summary_t*) malloc(sizeof(esp_core_dump_summary_t));
    if(!sum)
        return;

    if(esp_core_dump_get_summary(sum) == ESP_OK)
    {
        ERROR("Core dump: task '%s', PC 0x%08" PRIx32 ", exception cause %" PRIu32 ", address 0x%08" PRIx32,
            sum->exc_task, sum->exc_pc, sum->ex_info.exc_cause, sum->ex_info.exc_vaddr);

        // Same layout as the panic output, ready for addr2line
        char line[16 + 11 * 16];
        size_t pos = snprintf(line, sizeof(line), "Backtrace:");
        for(uint32_t i = 0; i < sum->exc_bt_info.depth && pos < sizeof(line); i++)
            pos += snprintf(line + pos, sizeof(line) - pos, " 0x%08" PRIx32, sum->exc_bt_info.bt[i]);
        ERROR("%s%s", line, sum->exc_bt_info.corrupted ? " (corrupted)" : "");

        // To check the dump belongs to the firmware you decode it with
        ERROR("Core dump ELF sha256: %.8s, running firmware: %s", (const char*) sum->app_elf_sha256, bb_elf_sha());
    }
    else
    {
        ERROR("Core dump present, but reading its summary failed");
    };

    free(sum);
    esp_core_dump_image_erase();
};

#else

void bb_coredump_report()
{
};

#endif
