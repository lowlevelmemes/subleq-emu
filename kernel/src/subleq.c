#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <graphics.h>
#include <klib.h>
#include <system.h>

extern uint64_t _readram(uint64_t);
extern void _writeram(uint64_t, uint64_t);

void _strcpyram(uint64_t dest, const char *mem) {
    for (size_t i = 0; mem[i]; i++)
        _writeram(dest++, (uint64_t)mem[i] << 56);
    _writeram(dest, 0);
}

void init_subleq(void) {

    /* CPU id */
    _strcpyram(335413288, "subleq-emu x86");

    /* */
    _writeram(334364664, (uint64_t)1);

    /* display */
    _writeram(335540096, (uint64_t)edid_width);
    _writeram(335540096 + 8, (uint64_t)edid_height);
    _writeram(335540096 + 16, (uint64_t)32);
    _writeram(335540096 + 24, (uint64_t)2);

}

extern void *initramfs_addr;
extern uint64_t subleq_cycle(uint64_t);

void subleq(void) {
    uint64_t eip = 0;

    int is_halted = 1;
    uint64_t cpu_bank = 334364672 + get_cpu_number() * 16;

    _writeram(cpu_bank + 0, 4);        // status
    _writeram(cpu_bank + 8, 0);        // EIP

    /* if this is CPU0, enable it */
    if (cpu_bank == 334364672)
        _writeram(cpu_bank + 0, 1);        // status

    for (;;) {
        if (!get_cpu_number())
            goto dont_check_status;
        switch (_readram(cpu_bank + 0)) {
            case 0:
                continue;
            case 1:
                /* active */
                if (is_halted) {
                    is_halted = 0;
                    eip = _readram(cpu_bank + 8);
                }
                break;
            case 2:
                /* stop requested */
                _writeram(cpu_bank + 0, 4);
            case 4:
                /* halted */
                is_halted = 1;
                continue;
            default:
                /* unrecognised state */
                kprint(KPRN_ERR, "CPU %U: Unrecognised state: %X", get_cpu_number(), _readram(cpu_bank + 0));
                for (;;)
                    asm volatile ("cli; hlt;");
        }
dont_check_status:
        eip = subleq_cycle(eip);
        _writeram(cpu_bank + 8, eip);
    }
}
