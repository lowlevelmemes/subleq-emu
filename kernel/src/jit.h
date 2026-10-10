#ifndef __JIT_H__
#define __JIT_H__

#include <stdint.h>
#include <stddef.h>

/*
 * SUBLEQ to x86-64 translator (jit.c)
 */

/* Shared with jit_enter and jit_exit in jit.asm, which depend on the layout */
typedef struct {
    uint64_t eip;
    uint64_t budget;
    uint64_t exit;
    uint8_t *bitmap;
} jit_state_t;

typedef struct {
    uint64_t executed;
    uint64_t translations;
    uint64_t translated_insns;
    uint64_t code_bytes;
    uint64_t entries;
    uint64_t links;
    uint64_t smc_exits;
    uint64_t hoist_exits;
    uint64_t smc_words;
    uint64_t invalidations;
    uint64_t revalidations;
    uint64_t interp_steps;
    uint64_t flushes;
} jit_stats_t;

/* Set up translation for Dawn memory of mem_size bytes and ncpus CPUs, with
 * at most max_bytes of memory; returns how much it took, or 0 on failure */
size_t jit_init(uint64_t mem_size, int ncpus, size_t max_bytes, int movbe);

/* Shrink the Dawn memory size given to jit_init() to the final one */
void jit_set_mem_size(uint64_t mem_size);

/* Copy with copy in the word copy loop, rather than memmove; the copies
 * only go down over their source, if at all */
void jit_set_copy(void (*copy)(void *, const void *, size_t));

/* Make CPU cpu leave its code, at the next block, once the big-endian word at
 * addr no longer holds value */
void jit_set_watch(int cpu, uint64_t addr, uint64_t value);

/* Run CPU cpu from eip for about budget instructions, returning the next eip */
uint64_t jit_run(int cpu, uint64_t eip, uint64_t budget);

/* Whether jit_init succeeded */
extern int jit_enabled;

jit_stats_t *jit_get_stats(int cpu);

/* Count rip, interrupted on CPU cpu, against the block it is in, and list the
 * blocks most counted since the last call in starts and counts, by count */
void jit_sample(int cpu, uint64_t rip);
int jit_top_samples(int cpu, int max, uint64_t *starts, uint32_t *counts, uint32_t *total);

#ifdef JIT_HOSTED
/* Provided by the host: zeroed, page aligned, and executable memory */
void *jit_os_alloc(size_t bytes);
/* Called with each block translated */
void jit_os_translated(uint64_t start, uint32_t n, const uint8_t *code, size_t bytes);
#endif

#endif
