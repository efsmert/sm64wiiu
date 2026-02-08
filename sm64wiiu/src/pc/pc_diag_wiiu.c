#include "pc_diag.h"

#ifdef TARGET_WII_U

#include <stdbool.h>

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>

static OSThread sDiagWatchdogThread;
static uint8_t sDiagWatchdogStack[0x4000] __attribute__((aligned(16)));
static volatile uint32_t sDiagProgressCounter = 0;
static volatile OSTime sDiagLastProgressTicks = 0;
static volatile uint32_t sDiagLastFrame = 0;
static volatile const char *sDiagStage = "boot";
static bool sDiagWatchdogStarted = false;

static int pc_diag_watchdog_thread_main(int argc, const char **argv) {
    uint32_t last_counter = sDiagProgressCounter;
    uint32_t last_logged_second = 0;

    (void)argc;
    (void)argv;

    while (1) {
        OSSleepTicks(OSMillisecondsToTicks(250));

        uint32_t current_counter = sDiagProgressCounter;
        if (current_counter != last_counter) {
            last_counter = current_counter;
            last_logged_second = 0;
            continue;
        }

        OSTime now = OSGetTime();
        OSTime last_progress = sDiagLastProgressTicks;
        uint64_t stalled_ms = 0;
        if (last_progress > 0 && now > last_progress) {
            stalled_ms = (uint64_t)OSTicksToMilliseconds((uint64_t)(now - last_progress));
        }

        if (stalled_ms < 1000) {
            continue;
        }

        uint32_t stalled_second = (uint32_t)(stalled_ms / 1000);
        if (stalled_second == last_logged_second) {
            continue;
        }
        last_logged_second = stalled_second;

        WHBLogPrintf("diag: stall %llums stage='%s' frame=%u progress=%u",
                     (unsigned long long)stalled_ms,
                     sDiagStage != NULL ? sDiagStage : "(null)",
                     (unsigned)sDiagLastFrame,
                     (unsigned)current_counter);
    }

    return 0;
}

void pc_diag_watchdog_init(void) {
    if (sDiagWatchdogStarted) {
        return;
    }

    sDiagLastProgressTicks = OSGetTime();
    sDiagProgressCounter = 1;
    sDiagLastFrame = 0;
    sDiagStage = "startup";

    if (OSCreateThread(&sDiagWatchdogThread,
                       pc_diag_watchdog_thread_main,
                       0,
                       NULL,
                       sDiagWatchdogStack + sizeof(sDiagWatchdogStack),
                       sizeof(sDiagWatchdogStack),
                       1,
                       OS_THREAD_ATTRIB_AFFINITY_ANY)) {
        OSSetThreadName(&sDiagWatchdogThread, "SM64DiagWatchdog");
        OSResumeThread(&sDiagWatchdogThread);
        sDiagWatchdogStarted = true;
        WHBLogPrint("diag: watchdog started");
    } else {
        WHBLogPrint("diag: watchdog failed to start");
    }
}

void pc_diag_mark_stage(const char *stage) {
    sDiagStage = (stage != NULL) ? stage : "(null)";
    sDiagProgressCounter++;
    sDiagLastProgressTicks = OSGetTime();
}

void pc_diag_mark_frame(uint32_t frame_index) {
    sDiagLastFrame = frame_index;
    sDiagProgressCounter++;
    sDiagLastProgressTicks = OSGetTime();
}

#else

void pc_diag_watchdog_init(void) {
}

void pc_diag_mark_stage(const char *stage) {
    (void)stage;
}

void pc_diag_mark_frame(uint32_t frame_index) {
    (void)frame_index;
}

#endif
