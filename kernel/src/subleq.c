#include <stdint.h>
#include <subleq.h>
#include <dawn.h>
#include <smp.h>
#include <pm.h>

/* Instructions run between looks at the CPU's bank */
#define INTERP_SLICE 4096

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
            eip = subleq_interp(eip, INTERP_SLICE);
        }
    }

    for (;;) {
        switch (dawn_readram(bank)) {
            case 1:
                /* run, from the eip in the bank if just started */
                if (halted) {
                    halted = 0;
                    eip = dawn_readram(bank + 8);
                }
                eip = subleq_interp(eip, INTERP_SLICE);
                dawn_writeram(bank + 8, eip);
                break;
            case 2:
                /* stop requested */
                dawn_writeram(bank, 4);
                /* fall through */
            case 4:
                /* stopped */
                halted = 1;
                asm volatile ("hlt");
                break;
            default:
                asm volatile ("pause");
                break;
        }
    }
}
