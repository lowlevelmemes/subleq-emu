#include <stdint.h>
#include <subleq.h>
#include <dawn.h>
#include <jit.h>
#include <smp.h>
#include <pm.h>

#ifdef JIT_DEBUG
#include <klib.h>
#include <time.h>

/* Log each CPU's speed and translator counters every second */
static void report(void) {
    static uint64_t next = 1000, last_t, last[128];
    if (uptime_raw < next || !jit_enabled)
        return;
    uint64_t dt = uptime_raw - last_t;
    last_t = uptime_raw;
    next = uptime_raw + 1000;
    for (int c = 0; c < cpu_count && c < 128; c++) {
        jit_stats_t *s = jit_get_stats(c);
        kprint(KPRN_DBG, "cpu %u: %U MIPS, %U blocks, %U KiB code, %U smc words, %U inval, "
               "%U reval, %U interp, %U flushes, %U links, %U entries",
               (uint32_t)c, (s->executed - last[c]) / (dt * 1000), s->translations,
               s->code_bytes >> 10, s->smc_words, s->invalidations, s->revalidations,
               s->interp_steps, s->flushes, s->links, s->entries);
        last[c] = s->executed;
    }
    uint64_t starts[12];
    uint32_t counts[12], total;
    int n = jit_top_samples(0, 12, starts, counts, &total);
    for (int k = 0; k < n; k++)
        kprint(KPRN_DBG, "cpu 0 hot: %X %U percent", starts[k], (uint64_t)counts[k] * 100 / (total ? total : 1));
}
#endif

/* Instructions run between looks at the CPU's bank; translated code also
 * leaves as soon as an AP's status changes */
#define JIT_SLICE 65536
#define INTERP_SLICE 4096

static uint64_t run(int cpu, uint64_t eip) {
    if (jit_enabled)
        return jit_run(cpu, eip, JIT_SLICE);
    return subleq_interp(eip, INTERP_SLICE);
}

/* Wait while the bank says the CPU is stopped, which can only change when
 * CPU 0 writes it: Dawn starts and stops CPUs at a fast pace and gives up on
 * those that do not answer soon */
static void wait_stopped(uint64_t bank) {
    while (dawn_readram(bank) == 4) {
        if (has_monitor) {
            asm volatile ("monitor" :: "a" (bank), "c" (0), "d" (0) : "memory");
            if (dawn_readram(bank) == 4)
                asm volatile ("mwait" :: "a" (0), "c" (0) : "memory");
        } else {
            asm volatile ("pause" ::: "memory");
        }
    }
}

void subleq(void) {
    int cpu = get_cpu_number();
    uint64_t bank = DAWN_CPU_BANK_BASE + 16 * (uint64_t)cpu;
    uint64_t eip = 0;
    int halted = 1;

    /* status stopped, eip 0 */
    dawn_writeram(bank, 4);
    dawn_writeram(bank + 8, 0);

    if (cpu == 0) {
        for (;;) {
            switch (dawn_readram(bank)) {
                case 8:
                    shutdown();
                    break;
                case 16:
                    reboot();
                    break;
                case 32:
                    pm_sleep();
                    break;
            }
#ifdef JIT_DEBUG
            report();
#endif
            eip = run(0, eip);
        }
    }

    /* translated code checks the status on every block, to stop at once */
    if (jit_enabled)
        jit_set_watch(cpu, bank, 1);

    for (;;) {
        switch (dawn_readram(bank)) {
            case 1:
                /* run, from the eip in the bank if just started */
                if (halted) {
                    halted = 0;
                    eip = dawn_readram(bank + 8);
                }
                eip = run(cpu, eip);
                dawn_writeram(bank + 8, eip);
                break;
            case 2:
                /* stop requested */
                dawn_writeram(bank, 4);
                /* fall through */
            case 4:
                /* stopped */
                halted = 1;
                wait_stopped(bank);
                break;
            default:
                asm volatile ("pause");
                break;
        }
    }
}
