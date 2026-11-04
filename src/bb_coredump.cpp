#include "bb_internal.h"
#include "blackbox.h"

#if BLACKBOX_COREDUMP_REPORT && !(defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF))
    #warning "BLACKBOX_COREDUMP_REPORT is set, but this core has no ELF core dumps to flash: the report does nothing"
#endif

#if BLACKBOX_COREDUMP_REPORT && defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <esp_core_dump.h>
#include <esp_system.h>

#if CONFIG_IDF_TARGET_ARCH_XTENSA
// Xtensa exception causes, as named by the ESP-IDF panic handler
static const char* exc_cause_name(uint32_t cause)
{
    static const char* const names[] = {
        "IllegalInstruction", "Syscall", "InstructionFetchError", "LoadStoreError",
        "Level1Interrupt", "Alloca", "IntegerDivideByZero", "PCValue",
        "Privileged", "LoadStoreAlignment", "res", "res",
        "InstrPDAddrError", "LoadStorePIFDataError", "InstrPIFAddrError", "LoadStorePIFAddrError",
        "InstTLBMiss", "InstTLBMultiHit", "InstrFetchPrivilege", "res",
        "InstrFetchProhibited", "res", "res", "res",
        "LoadStoreTLBMiss", "LoadStoreTLBMultihit", "LoadStorePrivilege", "res",
        "LoadProhibited", "StoreProhibited", "res", "res",
        "Cp0Dis", "Cp1Dis", "Cp2Dis", "Cp3Dis",
        "Cp4Dis", "Cp5Dis", "Cp6Dis", "Cp7Dis",
    };
    if(cause >= sizeof(names) / sizeof(names[0]))
        return "unknown";
    return names[cause];
};
#endif

// Logs the summary of the core dump in flash (task, cause, backtrace). It is the last crash that
// was stored, not necessarily one from the previous boot.
void bb_coredump_report()
{
    if(esp_core_dump_image_check() != ESP_OK)
        return;     // no valid dump in flash

    // Too big for a comfortable stack allocation
    esp_core_dump_summary_t* sum = (esp_core_dump_summary_t*) malloc(sizeof(esp_core_dump_summary_t));
    if(!sum)
    {
        WARNING("No Mem for coredump summary!");
        return;
    };

    if(esp_core_dump_get_summary(sum) != ESP_OK)
    {
        ERROR("Core dump present, but reading its summary failed");
        free(sum);
        esp_core_dump_image_erase();
        return;
    };
    if(strncmp((const char*) sum->app_elf_sha256, bb_elf_sha(), 8) != 0)
    {
        // Its addresses only decode with the ELF of the firmware that crashed
        WARNING("Core dump from another firmware (ELF sha256 %.8s) found, erased", (const char*) sum->app_elf_sha256);
        free(sum);
        esp_core_dump_image_erase();
        return;
    };

    // A crash reset this boot means the dump is (almost certainly) from the previous boot
    esp_reset_reason_t reason = esp_reset_reason();
    bool previous = (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT);

    ERROR("Last core dump (%s, this firmware, ELF sha256 %s): task '%s'",
        previous ? "from the previous boot" : "from an earlier boot", bb_elf_sha(), sum->exc_task);

    char line[16 + 11 * 16];

#if CONFIG_IDF_TARGET_ARCH_XTENSA
    ERROR("Exception: %s (cause %" PRIu32 "), PC 0x%08" PRIx32 ", address 0x%08" PRIx32,
        exc_cause_name(sum->ex_info.exc_cause), sum->ex_info.exc_cause, sum->exc_pc, sum->ex_info.exc_vaddr);

    // A0-A15 at the moment of the crash, four per line
    for(int row = 0; row < 4; row++)
    {
        size_t n = 0;
        for(int col = 0; col < 4; col++)
        {
            int r = row * 4 + col;
            n += snprintf(line + n, sizeof(line) - n, "A%-2d 0x%08" PRIx32 "  ", r, sum->ex_info.exc_a[r]);
        };
        ERROR("%s", line);
    };

    // EPC1..EPCn of nested exceptions, only the valid ones
    size_t n = 0;
    for(size_t i = 0; i < sizeof(sum->ex_info.epcx) / sizeof(sum->ex_info.epcx[0]) && n < sizeof(line); i++)
        if(sum->ex_info.epcx_reg_bits & (1 << i))
            n += snprintf(line + n, sizeof(line) - n, "EPC%u 0x%08" PRIx32 "  ", (unsigned)(i + 1), sum->ex_info.epcx[i]);
    if(n)
        ERROR("%s", line);
#else
    ERROR("PC 0x%08" PRIx32, sum->exc_pc);
#endif

    // Same layout as the panic output, ready for addr2line
    size_t pos = snprintf(line, sizeof(line), "Backtrace:");
    for(uint32_t i = 0; i < sum->exc_bt_info.depth && pos < sizeof(line); i++)
        pos += snprintf(line + pos, sizeof(line) - pos, " 0x%08" PRIx32, sum->exc_bt_info.bt[i]);
    ERROR("%s%s", line, sum->exc_bt_info.corrupted ? " (corrupted)" : "");

    free(sum);

#if (BLACKBOX_COREDUMP_ERASE == 1)
    INFO("Core dump erased.");
    esp_core_dump_image_erase();
#endif
};

#else

void bb_coredump_report()
{
};

#endif
