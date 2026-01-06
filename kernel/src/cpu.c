#include <stdint.h>
#include <cpu.h>

/* Enable SSE and AVX - called on each CPU */
void enable_simd(void) {
    uint64_t cr0, cr4;
    uint32_t ecx, ebx;

    /* Enable SSE */
    asm volatile ("movq %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);   /* Clear CR0.EM */
    cr0 |= (1ULL << 1);    /* Set CR0.MP */
    asm volatile ("movq %0, %%cr0" :: "r"(cr0));

    asm volatile ("movq %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10);  /* Set CR4.OSFXSR and CR4.OSXMMEXCPT */
    asm volatile ("movq %0, %%cr4" :: "r"(cr4));

    /* Check for XSAVE and AVX support */
    asm volatile (
        "movl $1, %%eax;"
        "xorl %%ecx, %%ecx;"
        "cpuid;"
        : "=c" (ecx)
        :
        : "eax", "ebx", "edx"
    );

    if ((ecx & ((1 << 26) | (1 << 28))) != ((1 << 26) | (1 << 28))) {
        return;  /* No XSAVE or AVX */
    }

    /* Enable OSXSAVE */
    asm volatile ("movq %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 18);
    asm volatile ("movq %0, %%cr4" :: "r"(cr4));

    /* Check for AVX-512 support */
    asm volatile (
        "movl $7, %%eax;"
        "xorl %%ecx, %%ecx;"
        "cpuid;"
        : "=b" (ebx)
        :
        : "eax", "ecx", "edx"
    );

    if (ebx & (1 << 16)) {
        /* Enable AVX-512 in XCR0 */
        asm volatile (
            "xorl %%ecx, %%ecx;"
            "xgetbv;"
            "orl $0xE7, %%eax;"
            "xsetbv;"
            ::: "eax", "ecx", "edx"
        );
    } else {
        /* Enable AVX in XCR0 */
        asm volatile (
            "xorl %%ecx, %%ecx;"
            "xgetbv;"
            "orl $0x7, %%eax;"
            "xsetbv;"
            ::: "eax", "ecx", "edx"
        );
    }
}
