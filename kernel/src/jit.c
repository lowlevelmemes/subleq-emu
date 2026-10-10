/*
 * SUBLEQ to x86-64 translator
 *
 * Translates runs of SUBLEQ instructions into blocks of x86 code, each
 * instruction keeping its own loads, stores, and branch, so that values flow
 * through registers within a block and every SUBLEQ branch is predicted on
 * its own. A block ends at an unconditional jump, after JIT_MAX_BLOCK
 * instructions, or where another block starts; conditional branches leave it
 * through side exits, which get linked straight to the target block.
 *
 * Each operand field (A, B, C) of an instruction is static, baked into the
 * code, or dynamic, read from memory at run time. SUBLEQ code dereferences
 * pointers by patching the operands of the instruction about to run, so a
 * field the block itself writes is dynamic, and so is one caught being
 * written before (dyn_map). The words of static fields are marked in
 * static_map, and every store checks it first: those to static addresses
 * all at once when the block is entered, those to computed ones each time,
 * against a byte per page first. A hit drops the code translated from the
 * word, which turns dynamic.
 *
 * Within a block, values are cached in registers in native byte order. Every
 * store still goes to memory, so memory is current at the block boundaries
 * and for the accesses to computed addresses. Device registers and CPU banks
 * are never cached, so that they are read and written as the interpreter
 * would.
 *
 * Each CPU translates into its own code cache, so none ever runs code that
 * another CPU patches. They share the two maps, and a CPU that drops code
 * for a word queues the word for the others. As a store can pass the check
 * just before another CPU marks its word, each CPU checks its new blocks
 * against memory again once all the CPUs have since come back to their
 * dispatchers, which bounds how long a block translated from a word that was
 * changing can survive.
 *
 * The compiler's word copy loop is recognised and run by jit_copy() for as
 * many iterations as the budget allows, when nothing that would change what
 * the loop does is in the way.
 *
 * Branches are taken when B <= A as signed numbers, as with "sub; jg".
 */

#include <stdint.h>
#include <stddef.h>
#include <jit.h>

#ifndef JIT_HOSTED
#include <klib.h>
#include <pmm.h>
#endif

#define BIAS 0x80000000ULL

/* Device registers and CPU banks, never cached in registers */
#define HW_LO 0x13e00000ULL
#define HW_HI 0x14000000ULL

#define JIT_MAX_BLOCK 64
#define JIT_MAX_CPUS 128
#define HASH_BITS 16
#define QUEUE_LEN 1024
#define PENDING_LEN 4096

/* Kept free at the end of the code cache, for the largest possible block */
#define CODE_SLACK (256 * 1024)

#define NREGS 11
#define NCONST 16

int jit_enabled = 0;

typedef struct jit_link {
    uint8_t *site;                  /* rel32 of a jump to the block, which went
                                     * to site + 4 before it was linked */
    struct jit_link *next;
} jit_link_t;

typedef struct jit_block {
    uint64_t start;
    uint32_t n;
    uint32_t nhoist;
    uint8_t *entry;
    uint8_t *inv_stub;
    struct jit_block *hnext;
    jit_link_t *incoming;
    uint64_t *hoist;                /* words stored to, checked on entry */
    uint64_t smask[3];              /* which fields are static */
    uint64_t shash;                 /* and a hash of their values */
    int valid;
    uint32_t samples;
} jit_block_t;

typedef struct {
    int used;                       /* caches the value at addr */
    uint64_t addr;
    int pinned;                     /* in use by the instruction translated */
    uint32_t lru;
} creg_t;

typedef struct {
    int kind;                       /* 0 none, 1 register, 2 constant */
    int reg;
    uint64_t val;
} cval_t;

typedef struct {
    int used;
    uint64_t addr;
    uint64_t val;
} cconst_t;

/* a store held back, as it stood where a stub leaves */
typedef struct {
    int kind;                       /* 0 none, 1 register, 2 constant */
    int reg;
    uint64_t val;
    uint64_t addr;
} pending_t;

typedef struct {
    int dyn;                        /* exit to C in register creg */
    uint8_t *site;                  /* rel32 jumping to the stub */
    uint64_t target;
    uint32_t idx;                   /* instruction the exit leaves from */
    int creg;
    pending_t pend;
} stub_t;

typedef struct {
    uint8_t *site;
    uint32_t idx;                   /* instruction left before it ran */
    pending_t pend;
    /* a store to a computed address in a page with translated code, whose
     * words are checked out of line, going back if they are clear */
    int precise;
    int rb, t1, t2;
    uint8_t *back;
} smc_t;

typedef struct {
    jit_state_t st;
    int cpu;
    int exact;                      /* run budgets out exactly, interpreting */

    uint8_t *code_base, *code_ptr, *code_end;
    jit_block_t *blocks;
    uint32_t nblocks, max_blocks;
    jit_link_t *links, *free_links;
    uint32_t nlinks, max_links;
    uint64_t *meta;
    uint32_t nmeta, max_meta;
    jit_block_t **htab;

    /* blocks leave when the word at watch_addr is not watch_value */
    uint64_t watch_addr;
    uint64_t watch_value;

    /* where execution resumes: blocks only start at branch targets and where
     * other blocks end, and after resuming elsewhere it is interpreted up to
     * one of those, so that blocks do not get split at arbitrary points */
    uint64_t last_eip;
    int strong;
    uint32_t weak_steps;

    /* words other CPUs dropped code for, which they add under qlock */
    volatile int qlock;
    uint32_t qhead, qtail;
    volatile int qoverflow;
    uint64_t queue[QUEUE_LEN];

    /* new blocks, to check against memory again */
    uint32_t phead, ptail;
    struct {
        jit_block_t *b;
        uint64_t stamp;
    } pending[PENDING_LEN];

    /* translator */
    uint8_t *cp;
    /* the last store to a static word, held back in case the next store goes
     * to the same word and makes it dead: stores still reach memory in
     * program order, minus those overwritten before any other */
    int pend;
    uint64_t pend_addr;
    creg_t regs[NREGS];
    uint32_t lru_clock;
    cconst_t consts[NCONST];
    uint32_t const_next;
    stub_t stubs[JIT_MAX_BLOCK + 2];
    smc_t smcs[JIT_MAX_BLOCK * 4];
    uint64_t hoist[JIT_MAX_BLOCK * 2];
    uint8_t hoist_done[JIT_MAX_BLOCK * 2];
    uint8_t *hoist_sites[JIT_MAX_BLOCK * 2];
    uint8_t written[JIT_MAX_BLOCK * 3];

    jit_stats_t stats;
    uint32_t samples, samples_out;
    /* samples outside translated code, by 64-byte line */
    struct {
        uint64_t line;
        uint32_t count;
    } out[64];
} jit_cpu_t;

static uint64_t mem_size;
static uint8_t *static_map, *dyn_map;

/* After static_map: a byte per page, set when the page, or the start of the
 * next one, holds a static field, so that a store's first byte tells */
static uint8_t *page_map;
static uint64_t page_off;
static int ncpus;
static int use_movbe;
static jit_cpu_t *cpus[JIT_MAX_CPUS];

/* Grace periods: each CPU publishes the epoch it last saw in its dispatcher,
 * or ~0 while outside of jit_run */
static volatile uint64_t epoch = 1;
static volatile uint64_t quiet[JIT_MAX_CPUS];

void jit_enter(uint8_t *code, jit_state_t *st);

#ifndef JIT_HOSTED
static void *jit_os_alloc(size_t bytes) {
    void *p = kmalloc((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    return p ? (void *)((uintptr_t)p + PHYS_MEM_OFFSET) : NULL;
}
#endif

static inline uint64_t rd(uint64_t a) { return __builtin_bswap64(*(volatile uint64_t *)a); }
static inline void wr(uint64_t a, uint64_t v) { *(volatile uint64_t *)a = __builtin_bswap64(v); }

static inline int bit_get(const uint8_t *m, uint64_t w) {
    return (__atomic_load_n(&m[w >> 3], __ATOMIC_RELAXED) >> (w & 7)) & 1;
}
static inline void bit_set(uint8_t *m, uint64_t w) { __atomic_fetch_or(&m[w >> 3], 1 << (w & 7), __ATOMIC_SEQ_CST); }
static inline void bit_clr(uint8_t *m, uint64_t w) { __atomic_fetch_and(&m[w >> 3], ~(1 << (w & 7)), __ATOMIC_SEQ_CST); }

static inline void mark_static(uint64_t w) {
    bit_set(static_map, w);
    uint64_t pg = (w << 3) >> 12;
    page_map[pg] = 1;
    if (pg)
        page_map[pg - 1] = 1;
}

static inline void lock(volatile int *l) {
    while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE))
        while (*l)
            asm volatile ("pause");
}
static inline void unlock(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }

/* ------------------------------------------------------------------------ */
/* blocks */

static inline uint64_t hash(uint64_t a) {
    return ((a >> 3) * 0x9e3779b97f4a7c15ULL) >> (64 - HASH_BITS);
}

static jit_block_t *lookup(jit_cpu_t *J, uint64_t a) {
    for (jit_block_t *b = J->htab[hash(a)]; b; b = b->hnext)
        if (b->start == a)
            return b;
    return NULL;
}

static void patch_rel32(uint8_t *site, uint8_t *target) {
    int32_t rel = (int32_t)(target - (site + 4));
    __builtin_memcpy(site, &rel, 4);
}

static void invalidate(jit_cpu_t *J, jit_block_t *b) {
    if (!b->valid)
        return;
    b->valid = 0;
    jit_block_t **pp = &J->htab[hash(b->start)];
    while (*pp && *pp != b)
        pp = &(*pp)->hnext;
    if (*pp)
        *pp = b->hnext;
    /* send the jumps linked to the block back to their slow paths, and what
     * still enters it to the dispatcher */
    for (jit_link_t *l = b->incoming; l; ) {
        jit_link_t *next = l->next;
        patch_rel32(l->site, l->site + 4);
        l->next = J->free_links;
        J->free_links = l;
        l = next;
    }
    b->incoming = NULL;
    b->entry[0] = 0xe9;
    patch_rel32(b->entry + 1, b->inv_stub);
    J->stats.invalidations++;
}

/* drop this CPU's blocks with an instruction covering word w */
static void invalidate_word(jit_cpu_t *J, uint64_t w) {
    uint64_t a = w << 3;
    uint64_t lo = a >= 24 * JIT_MAX_BLOCK ? a - 24 * JIT_MAX_BLOCK + 8 : 0;
    for (uint64_t s = lo; s <= a; s += 8) {
        jit_block_t *b = lookup(J, s);
        if (b && a < s + 24ULL * b->n)
            invalidate(J, b);
    }
}

static void flush(jit_cpu_t *J) {
    __builtin_memset(J->htab, 0, sizeof(jit_block_t *) << HASH_BITS);
    J->code_ptr = J->code_base;
    J->nblocks = 0;
    J->nlinks = 0;
    J->free_links = NULL;
    J->nmeta = 0;
    J->phead = J->ptail = 0;
    J->stats.flushes++;
}

/* a store to word w, which some translation took as static */
static void smc_hit(jit_cpu_t *J, uint64_t w) {
    J->stats.smc_words++;
    bit_set(dyn_map, w);
    bit_clr(static_map, w);
    invalidate_word(J, w);
    for (int c = 0; c < ncpus; c++) {
        jit_cpu_t *o = cpus[c];
        if (o == J)
            continue;
        lock(&o->qlock);
        uint32_t next = (o->qtail + 1) % QUEUE_LEN;
        if (next == __atomic_load_n(&o->qhead, __ATOMIC_ACQUIRE)) {
            o->qoverflow = 1;
        } else {
            o->queue[o->qtail] = w;
            __atomic_store_n(&o->qtail, next, __ATOMIC_RELEASE);
        }
        unlock(&o->qlock);
    }
}

static void drain_queue(jit_cpu_t *J) {
    if (J->qoverflow) {
        lock(&J->qlock);
        J->qoverflow = 0;
        J->qhead = J->qtail;
        unlock(&J->qlock);
        flush(J);
        return;
    }
    while (J->qhead != __atomic_load_n(&J->qtail, __ATOMIC_ACQUIRE)) {
        invalidate_word(J, J->queue[J->qhead]);
        __atomic_store_n(&J->qhead, (J->qhead + 1) % QUEUE_LEN, __ATOMIC_RELEASE);
    }
}

static inline void store_check(jit_cpu_t *J, uint64_t b) {
    uint64_t w0 = b >> 3, w1 = (b + 7) >> 3;
    if (w0 < (mem_size >> 3) && bit_get(static_map, w0))
        smc_hit(J, w0);
    if (w1 != w0 && w1 < (mem_size >> 3) && bit_get(static_map, w1))
        smc_hit(J, w1);
}

/* one instruction, with its store checked against translated code */
static uint64_t interp_step(jit_cpu_t *J, uint64_t eip) {
    uint64_t a = rd(eip), b = rd(eip + 8), c = rd(eip + 16);
    int64_t va = (int64_t)rd(a), vb = (int64_t)rd(b);
    store_check(J, b);
    wr(b, (uint64_t)vb - (uint64_t)va);
    J->stats.interp_steps++;
    return vb <= va ? c : eip + 24;
}

/* the static fields of a block, as they are in memory now */
static uint64_t static_hash(const jit_block_t *b) {
    uint64_t h = 0x9e3779b97f4a7c15ULL;
    for (uint32_t f = 0; f < b->n * 3; f++)
        if ((b->smask[f >> 6] >> (f & 63)) & 1)
            h = (h ^ *(volatile uint64_t *)(b->start + 8ULL * f)) * 0x100000001b3ULL;
    return h;
}

/* check new blocks against memory once every CPU has been through its
 * dispatcher since they were made, and every store that could have missed
 * their marks is visible */
static void revalidate(jit_cpu_t *J) {
    if (J->phead == J->ptail)
        return;
    uint64_t min = ~0ULL;
    for (int c = 0; c < ncpus; c++) {
        uint64_t q = __atomic_load_n(&quiet[c], __ATOMIC_ACQUIRE);
        if (q < min)
            min = q;
    }
    while (J->phead != J->ptail && J->pending[J->phead].stamp <= min) {
        jit_block_t *b = J->pending[J->phead].b;
        J->phead = (J->phead + 1) % PENDING_LEN;
        if (!b->valid || static_hash(b) == b->shash)
            continue;
        /* a static field changed: drop it everywhere */
        J->stats.revalidations++;
        for (uint32_t f = 0; f < b->n * 3; f++)
            if ((b->smask[f >> 6] >> (f & 63)) & 1)
                smc_hit(J, (b->start >> 3) + f);
    }
}

/* ------------------------------------------------------------------------ */
/* x86-64 emitter */

enum { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };

#define CC_C  0x2
#define CC_NZ 0x5
#define CC_L  0xc
#define CC_LE 0xe

static inline void e8(jit_cpu_t *J, uint8_t v) { *J->cp++ = v; }
static inline void e32(jit_cpu_t *J, uint32_t v) { __builtin_memcpy(J->cp, &v, 4); J->cp += 4; }
static inline void e64(jit_cpu_t *J, uint64_t v) { __builtin_memcpy(J->cp, &v, 8); J->cp += 8; }

/* REX with W, and the high bits of reg, index, and base/rm */
static void rex(jit_cpu_t *J, int w, int r, int x, int b) {
    uint8_t v = 0x40 | (w << 3) | ((r >> 3) << 2) | ((x >> 3) << 1) | (b >> 3);
    if (v != 0x40)
        e8(J, v);
}

/* modrm, and SIB if needed, for [base + disp] */
static void mem_operand(jit_cpu_t *J, int reg, int base, int32_t disp) {
    int mod;
    if (disp == 0 && (base & 7) != 5)
        mod = 0;
    else if (disp >= -128 && disp <= 127)
        mod = 1;
    else
        mod = 2;
    e8(J, (mod << 6) | ((reg & 7) << 3) | (base & 7));
    if ((base & 7) == 4)
        e8(J, 0x24);
    if (mod == 1)
        e8(J, (uint8_t)disp);
    else if (mod == 2)
        e32(J, (uint32_t)disp);
}

static void modrm_reg(jit_cpu_t *J, int reg, int rm) { e8(J, 0xc0 | ((reg & 7) << 3) | (rm & 7)); }

/* 64-bit op with a [base + disp] operand: 8b load, 89 store, 8d lea */
static void op_mem(jit_cpu_t *J, uint8_t op, int r, int base, int32_t disp) {
    rex(J, 1, r, 0, base);
    e8(J, op);
    mem_operand(J, r, base, disp);
}

static void movbe_mem(jit_cpu_t *J, int store, int r, int base, int32_t disp) {
    rex(J, 1, r, 0, base);
    e8(J, 0x0f); e8(J, 0x38); e8(J, store ? 0xf1 : 0xf0);
    mem_operand(J, r, base, disp);
}

/* mov r, [base + index * 8] */
static void load_sib8(jit_cpu_t *J, int r, int base, int index) {
    rex(J, 1, r, index, base);
    e8(J, 0x8b);
    e8(J, 0x04 | ((r & 7) << 3));
    e8(J, 0xc0 | ((index & 7) << 3) | (base & 7));
}

static void bswap(jit_cpu_t *J, int r) { rex(J, 1, 0, 0, r); e8(J, 0x0f); e8(J, 0xc8 + (r & 7)); }
static void mov_rr(jit_cpu_t *J, int dst, int src) { rex(J, 1, src, 0, dst); e8(J, 0x89); modrm_reg(J, src, dst); }
static void sub_rr(jit_cpu_t *J, int dst, int src) { rex(J, 1, src, 0, dst); e8(J, 0x29); modrm_reg(J, src, dst); }
static void neg_r(jit_cpu_t *J, int r) { rex(J, 1, 0, 0, r); e8(J, 0xf7); modrm_reg(J, 3, r); }
static void test_rr(jit_cpu_t *J, int a, int b) { rex(J, 1, b, 0, a); e8(J, 0x85); modrm_reg(J, b, a); }
static void shr_ri(jit_cpu_t *J, int r, uint8_t n) { rex(J, 1, 0, 0, r); e8(J, 0xc1); modrm_reg(J, 5, r); e8(J, n); }
static void shl_ri(jit_cpu_t *J, int r, uint8_t n) { rex(J, 1, 0, 0, r); e8(J, 0xc1); modrm_reg(J, 4, r); e8(J, n); }
static void or_rr(jit_cpu_t *J, int dst, int src) { rex(J, 1, src, 0, dst); e8(J, 0x09); modrm_reg(J, src, dst); }
static void and_rr(jit_cpu_t *J, int dst, int src) { rex(J, 1, src, 0, dst); e8(J, 0x21); modrm_reg(J, src, dst); }
static void bt_rr(jit_cpu_t *J, int base, int bit) { rex(J, 1, bit, 0, base); e8(J, 0x0f); e8(J, 0xa3); modrm_reg(J, bit, base); }

/* leaves the flags alone */
static void mov_imm(jit_cpu_t *J, int r, uint64_t v) {
    if (v <= 0xffffffffULL) {
        rex(J, 0, 0, 0, r); e8(J, 0xb8 + (r & 7)); e32(J, (uint32_t)v);
    } else if ((int64_t)v == (int32_t)v) {
        rex(J, 1, 0, 0, r); e8(J, 0xc7); modrm_reg(J, 0, r); e32(J, (uint32_t)v);
    } else {
        rex(J, 1, 0, 0, r); e8(J, 0xb8 + (r & 7)); e64(J, v);
    }
}

/* add (ext 0) or sub (ext 5) r, imm32 */
static void alu_imm(jit_cpu_t *J, int ext, int r, int32_t v) {
    rex(J, 1, 0, 0, r); e8(J, 0x81); modrm_reg(J, ext, r); e32(J, (uint32_t)v);
}

static uint8_t *jcc32(jit_cpu_t *J, uint8_t cc) { e8(J, 0x0f); e8(J, 0x80 + cc); uint8_t *s = J->cp; e32(J, 0); return s; }
static uint8_t *jmp32(jit_cpu_t *J) { e8(J, 0xe9); uint8_t *s = J->cp; e32(J, 0); return s; }
static void jmp_r(jit_cpu_t *J, int r) { rex(J, 0, 0, 0, r); e8(J, 0xff); modrm_reg(J, 4, r); }

/* cmp byte [base + index + disp], 0 */
static void cmp_byte_zero(jit_cpu_t *J, int base, int index, int32_t disp) {
    rex(J, 0, 0, index, base);
    e8(J, 0x80);
    e8(J, 0xbc);                                    /* mod 10, /7, SIB */
    e8(J, ((index & 7) << 3) | (base & 7));
    e32(J, (uint32_t)disp);
    e8(J, 0);
}

/* test byte [r14 + w / 8], 1 << (w % 8) */
static void test_bitmap(jit_cpu_t *J, uint64_t w) {
    rex(J, 0, 0, 0, R14); e8(J, 0xf6); mem_operand(J, 0, R14, (int32_t)(w >> 3)); e8(J, 1 << (w & 7));
}

/* leave with eip in r13 and the exit info in rbx */
static void exit_to(jit_cpu_t *J, uint64_t eip, uint32_t info) {
    mov_imm(J, R13, eip);
    mov_imm(J, RBX, info);
    jmp_r(J, R12);
}

/* ------------------------------------------------------------------------ */
/* register cache */

static const int alloc_regs[NREGS] = { RAX, RCX, RDX, RBX, RSI, RDI, R8, R9, R10, R11, R13 };

static inline int cacheable(uint64_t a) { return !(a & 7) && (a < HW_LO || a >= HW_HI); }

static int reg_index(int r) {
    for (int i = 0; i < NREGS; i++)
        if (alloc_regs[i] == r)
            return i;
    return -1;
}

static void cache_drop(jit_cpu_t *J, uint64_t a) {
    for (int i = 0; i < NREGS; i++)
        if (J->regs[i].used && J->regs[i].addr == a)
            J->regs[i].used = 0;
    for (int i = 0; i < NCONST; i++)
        if (J->consts[i].used && J->consts[i].addr == a)
            J->consts[i].used = 0;
}

static void cache_drop_all(jit_cpu_t *J) {
    for (int i = 0; i < NREGS; i++)
        J->regs[i].used = 0;
    for (int i = 0; i < NCONST; i++)
        J->consts[i].used = 0;
}

static cval_t cache_find(jit_cpu_t *J, uint64_t a) {
    cval_t v = { 0, -1, 0 };
    if (!cacheable(a))
        return v;
    for (int i = 0; i < NCONST; i++)
        if (J->consts[i].used && J->consts[i].addr == a) {
            v.kind = 2;
            v.val = J->consts[i].val;
            return v;
        }
    for (int i = 0; i < NREGS; i++)
        if (J->regs[i].used && J->regs[i].addr == a) {
            J->regs[i].lru = ++J->lru_clock;
            v.kind = 1;
            v.reg = alloc_regs[i];
            return v;
        }
    return v;
}

static void set_const(jit_cpu_t *J, uint64_t a, uint64_t val) {
    cache_drop(J, a);
    if (!cacheable(a))
        return;
    for (int i = 0; i < NCONST; i++)
        if (!J->consts[i].used) {
            J->consts[i] = (cconst_t){ 1, a, val };
            return;
        }
    J->consts[J->const_next++ % NCONST] = (cconst_t){ 1, a, val };
}

static void pin(jit_cpu_t *J, int r) {
    int i = reg_index(r);
    if (i >= 0)
        J->regs[i].pinned = 1;
}

static void unpin(jit_cpu_t *J, int r) {
    int i = reg_index(r);
    if (i >= 0)
        J->regs[i].pinned = 0;
}

static void flush_pending(jit_cpu_t *J);

/* a free register, pinned, taking the least recently used unpinned one; an
 * instruction pins at most 8 of the 11 */
static int scratch(jit_cpu_t *J) {
    int best = -1;
    for (int i = 0; i < NREGS; i++) {
        if (J->regs[i].pinned)
            continue;
        if (!J->regs[i].used) {
            best = i;
            break;
        }
        if (best < 0 || J->regs[i].lru < J->regs[best].lru)
            best = i;
    }
    if (J->regs[best].used && J->pend && J->regs[best].addr == J->pend_addr)
        flush_pending(J);
    J->regs[best].used = 0;
    J->regs[best].pinned = 1;
    return alloc_regs[best];
}

/* make register r cache the value at a */
static void bind(jit_cpu_t *J, int r, uint64_t a) {
    if (!cacheable(a))
        return;
    cache_drop(J, a);
    int i = reg_index(r);
    J->regs[i].used = 1;
    J->regs[i].addr = a;
    J->regs[i].lru = ++J->lru_clock;
}

static inline int fits_disp(uint64_t a) { return a < (BIAS << 1); }

/* r = native value at [base + disp] */
static void load_mem(jit_cpu_t *J, int r, int base, int32_t disp) {
    if (use_movbe) {
        movbe_mem(J, 0, r, base, disp);
    } else {
        op_mem(J, 0x8b, r, base, disp);
        bswap(J, r);
    }
}

/* r = native value at a static address */
static void load_static(jit_cpu_t *J, int r, uint64_t a) {
    if (fits_disp(a)) {
        load_mem(J, r, R15, (int32_t)(a - BIAS));
    } else {
        mov_imm(J, r, a);
        load_mem(J, r, r, 0);
    }
}

static cval_t shifted(jit_cpu_t *J, cval_t v, int left, int n);
static void or_val(jit_cpu_t *J, int r, cval_t v);

/* the value at a static address, from the cache or loaded and cached; an
 * unaligned one is put together from the two words it spans when either is
 * cached, as its load could not be forwarded from their stores */
static cval_t get_static(jit_cpu_t *J, uint64_t a) {
    if ((a & 7) && cacheable(a & ~7ULL) && cacheable((a & ~7ULL) + 8)) {
        int k = (a & 7) * 8;
        cval_t lo = cache_find(J, a & ~7ULL), hi = cache_find(J, (a & ~7ULL) + 8);
        if (lo.kind || hi.kind) {
            if (lo.kind == 1)
                pin(J, lo.reg);
            if (hi.kind == 1)
                pin(J, hi.reg);
            if (!lo.kind)
                lo = get_static(J, a & ~7ULL);
            if (!hi.kind)
                hi = get_static(J, (a & ~7ULL) + 8);
            /* (lo << k) | (hi >> (64 - k)) */
            cval_t v = shifted(J, lo, 1, k), h = shifted(J, hi, 0, 64 - k);
            if (v.kind == 2 && h.kind == 2)
                return (cval_t){ 2, -1, v.val | h.val };
            if (v.kind == 2) {
                cval_t t = v;
                v = h;
                h = t;
            }
            or_val(J, v.reg, h);
            return v;
        }
    }
    cval_t v = cache_find(J, a);
    if (v.kind == 1)
        pin(J, v.reg);
    if (v.kind)
        return v;
    /* memory has to have a held back store the load covers */
    if (J->pend && (a & 7) && (J->pend_addr == (a & ~7ULL) || J->pend_addr == (a & ~7ULL) + 8))
        flush_pending(J);
    int r = scratch(J);
    load_static(J, r, a);
    bind(J, r, a);
    v.kind = 1;
    v.reg = r;
    return v;
}

static int to_reg(jit_cpu_t *J, cval_t v) {
    if (v.kind == 1)
        return v.reg;
    int r = scratch(J);
    mov_imm(J, r, v.val);
    return r;
}

/* store the native value in r to a static address, or [base] if dynamic */
static void store_reg(jit_cpu_t *J, int r, int dyn, uint64_t a, int base) {
    int32_t disp = 0;
    if (!dyn) {
        if (fits_disp(a)) {
            base = R15;
            disp = (int32_t)(a - BIAS);
        } else {
            base = scratch(J);
            mov_imm(J, base, a);
        }
    }
    if (use_movbe) {
        movbe_mem(J, 1, r, base, disp);
    } else {
        int t = scratch(J);
        mov_rr(J, t, r);
        bswap(J, t);
        op_mem(J, 0x89, t, base, disp);
    }
}

static void store_const(jit_cpu_t *J, uint64_t v, int dyn, uint64_t a, int base) {
    uint64_t be = __builtin_bswap64(v);
    if (!dyn && fits_disp(a) && (int64_t)be == (int32_t)be) {
        rex(J, 1, 0, 0, R15); e8(J, 0xc7); mem_operand(J, 0, R15, (int32_t)(a - BIAS)); e32(J, (uint32_t)be);
        return;
    }
    int t = scratch(J);
    mov_imm(J, t, be);
    if (!dyn) {
        if (fits_disp(a)) {
            op_mem(J, 0x89, t, R15, (int32_t)(a - BIAS));
            return;
        }
        base = scratch(J);
        mov_imm(J, base, a);
    }
    op_mem(J, 0x89, t, base, 0);
}

/* store the held back value, and stop holding it */
static void flush_pending(jit_cpu_t *J) {
    if (!J->pend)
        return;
    J->pend = 0;
    cval_t v = cache_find(J, J->pend_addr);
    if (v.kind == 2) {
        store_const(J, v.val, 0, J->pend_addr, -1);
    } else {
        int i = reg_index(v.reg), was = J->regs[i].pinned;
        J->regs[i].pinned = 1;
        store_reg(J, v.reg, 0, J->pend_addr, -1);
        J->regs[i].pinned = was;
    }
}

static pending_t snap_pending(jit_cpu_t *J) {
    pending_t p = { 0, -1, 0, 0 };
    if (J->pend) {
        cval_t v = cache_find(J, J->pend_addr);
        p.kind = v.kind;
        p.reg = v.reg;
        p.val = v.val;
        p.addr = J->pend_addr;
    }
    return p;
}

/* store a held back value from a stub, where any register but the value's and
 * keep can be clobbered */
static void stub_store(jit_cpu_t *J, const pending_t *p, int keep) {
    if (!p->kind)
        return;
    int t = RAX, base = R15;
    while (t == p->reg || t == keep)
        t++;
    int32_t disp = (int32_t)(p->addr - BIAS);
    if (!fits_disp(p->addr)) {
        base = t + 1;
        while (base == p->reg || base == keep)
            base++;
        mov_imm(J, base, p->addr);
        disp = 0;
    }
    if (p->kind == 2) {
        mov_imm(J, t, __builtin_bswap64(p->val));
        op_mem(J, 0x89, t, base, disp);
    } else if (use_movbe) {
        movbe_mem(J, 1, p->reg, base, disp);
    } else {
        mov_rr(J, t, p->reg);
        bswap(J, t);
        op_mem(J, 0x89, t, base, disp);
    }
}

/* r &= mask, through a scratch register when the mask is no sign-extended
 * imm32 */
static void and_imm64(jit_cpu_t *J, int r, uint64_t mask) {
    if ((int64_t)mask == (int32_t)mask) {
        rex(J, 1, 0, 0, r); e8(J, 0x81); modrm_reg(J, 4, r); e32(J, (uint32_t)mask);
    } else if (mask == 0xffffffffULL) {
        rex(J, 0, r, 0, r); e8(J, 0x89); modrm_reg(J, r, r);      /* mov r32, r32 */
    } else {
        int t = scratch(J);
        mov_imm(J, t, mask);
        and_rr(J, r, t);
        unpin(J, t);
    }
}

/* r |= v */
static void or_val(jit_cpu_t *J, int r, cval_t v) {
    if (v.kind == 2 && !v.val)
        return;
    int t = to_reg(J, v);
    or_rr(J, r, t);
    if (v.kind == 2)
        unpin(J, t);
}

/* v shifted left (left = 1) or right by n bits, as a new value */
static cval_t shifted(jit_cpu_t *J, cval_t v, int left, int n) {
    if (v.kind == 2)
        return (cval_t){ 2, -1, left ? v.val << n : v.val >> n };
    int r = scratch(J);
    mov_rr(J, r, v.reg);
    if (left)
        shl_ri(J, r, n);
    else
        shr_ri(J, r, n);
    return (cval_t){ 1, r, 0 };
}

/* ------------------------------------------------------------------------ */
/* translation */

/* The word copy loop the compiler emits: with the A field of instruction 5
 * (F1), the B field of 15 (F2), and the A and B fields of 14 (F3, F4)
 * patched with the source and destination pointers, it copies a word from
 * where S points to where D points, then adds 8 to S, D, and the counter C,
 * the last time when C was over -8 before:
 *
 *    0: T1 T1     1: T2 T2     2: F1 F1     3: S T1      4: T1 F1
 *    5: [F1] T2   6: T1 T1     7: F2 F2     8: F3 F3     9: F4 F4
 *   10: D T1     11: T1 F2    12: T1 F3    13: T1 F4    14: [F3] [F4]
 *   15: T2 [F2]  16: M S/D    17: M D/S    18: M C, back to 0
 *
 * with M holding -8, and every C but 18's the next instruction. */
enum { X_ANY, X_T1, X_T2, X_S, X_D, X_C, X_M, X_F1, X_F2, X_F3, X_F4, X_SD, X_N };

static const uint8_t copy_tmpl[19][2] = {
    { X_T1, X_T1 }, { X_T2, X_T2 }, { X_F1, X_F1 }, { X_S, X_T1 }, { X_T1, X_F1 },
    { X_ANY, X_T2 }, { X_T1, X_T1 }, { X_F2, X_F2 }, { X_F3, X_F3 }, { X_F4, X_F4 },
    { X_D, X_T1 }, { X_T1, X_F2 }, { X_T1, X_F3 }, { X_T1, X_F4 }, { X_ANY, X_ANY },
    { X_T2, X_ANY }, { X_M, X_SD }, { X_M, X_SD }, { X_M, X_C },
};

/* whether the loop starts at start, filling desc with start, T1, T2, S, D,
 * C, and M */
static int match_copy(uint64_t start, uint64_t *desc) {
    if (start + 19 * 24 > mem_size)
        return 0;
    uint64_t sym[X_N] = { 0 };
    int bound[X_N] = { 0 };
    sym[X_F1] = start + 5 * 24;
    sym[X_F2] = start + 15 * 24 + 8;
    sym[X_F3] = start + 14 * 24;
    sym[X_F4] = start + 14 * 24 + 8;
    bound[X_F1] = bound[X_F2] = bound[X_F3] = bound[X_F4] = 1;
    uint64_t sd = 0;
    for (int k = 0; k < 19; k++) {
        uint64_t p = start + 24ULL * k;
        if (rd(p + 16) != (k == 18 ? start : p + 24) || bit_get(dyn_map, (p + 16) >> 3))
            return 0;
        for (int f = 0; f < 2; f++) {
            int x = copy_tmpl[k][f];
            uint64_t v = rd(p + 8 * f);
            if (x == X_ANY)
                continue;
            if (bit_get(dyn_map, (p + 8 * f) >> 3))
                return 0;
            if (x == X_SD) {
                /* S and D in either order */
                if (k == 16) {
                    if (v != sym[X_S] && v != sym[X_D])
                        return 0;
                    sd = v;
                } else if (v == sd || (v != sym[X_S] && v != sym[X_D])) {
                    return 0;
                }
                continue;
            }
            if (!bound[x]) {
                sym[x] = v;
                bound[x] = 1;
            } else if (sym[x] != v) {
                return 0;
            }
        }
    }
    /* the variables are distinct aligned words outside the loop */
    static const int vars[] = { X_T1, X_T2, X_S, X_D, X_C, X_M };
    for (int k = 0; k < 6; k++) {
        uint64_t v = sym[vars[k]];
        if ((v & 7) || v + 8 > mem_size || (v + 8 > start && v < start + 19 * 24))
            return 0;
        for (int j = 0; j < k; j++)
            if (sym[vars[j]] == v)
                return 0;
        desc[k + 1] = v;
    }
    desc[0] = start;
    return 1;
}

typedef struct {
    uint64_t n;                     /* iterations run, 0 to run the loop */
    uint64_t done;                  /* whether that ended the loop */
} copy_result_t;

/* below this many iterations, the call costs more than it saves */
#define COPY_MIN 4

/* the copy jit_copy() does, which only ever goes down over its source */
static void move(void *d, const void *s, size_t n) {
    __builtin_memmove(d, s, n);
}

static void (*copy_fn)(void *, const void *, size_t) = move;

static inline int overlaps(uint64_t a, uint64_t alen, uint64_t b, uint64_t blen) {
    return a < b + blen && b < a + alen;
}

/* Run the loop desc describes for as many iterations as fit in budget plus
 * one, from its translated code, when no word it reads or writes is one it
 * changes otherwise, its code, a device, or translated code, and the copy
 * does not run forward over itself, where the loop repeats words */
copy_result_t jit_copy(const uint64_t *desc, uint64_t budget);
copy_result_t jit_copy(const uint64_t *desc, uint64_t budget) {
    copy_result_t r = { 0, 0 };
    uint64_t start = desc[0], t1 = desc[1], t2 = desc[2], sv = desc[3], dv = desc[4], cv = desc[5], mv = desc[6];
    if ((int64_t)rd(mv) != -8)
        return r;
    uint64_t s = rd(sv), d = rd(dv);
    int64_t c = (int64_t)rd(cv);
    uint64_t total = c <= -8 ? (uint64_t)(-8 - c) / 8 + 2 : 1;
    uint64_t n = budget / 19 + 1;
    if (n > total)
        n = total;
    if (n < COPY_MIN)
        return r;
    uint64_t bytes = 8 * n;
    if (s > mem_size || bytes > mem_size - s || d > mem_size || bytes > mem_size - d)
        return r;
    if (d > s && d < s + bytes)
        return r;
    const uint64_t vars[] = { t1, t2, sv, dv, cv };
    for (int k = 0; k < 5; k++)
        if (overlaps(s, bytes, vars[k], 8) || overlaps(d, bytes, vars[k], 8))
            return r;
    if (overlaps(d, bytes, mv, 8) || overlaps(s, bytes, start, 19 * 24) ||
        overlaps(d, bytes, start, 19 * 24) || overlaps(s, bytes, HW_LO, HW_HI - HW_LO) ||
        overlaps(d, bytes, HW_LO, HW_HI - HW_LO))
        return r;
    for (uint64_t pg = d >> 12; pg <= (d + bytes - 1) >> 12; pg++)
        if (page_map[pg])
            return r;

    uint64_t last = rd(s + bytes - 8);
    copy_fn((void *)d, (void *)s, bytes);
    /* the variables and fields as the last iteration leaves them */
    uint64_t dl = d + bytes - 8;
    wr(sv, s + bytes);
    wr(dv, d + bytes);
    wr(cv, (uint64_t)c + bytes);
    wr(t1, -dl);
    wr(t2, -last);
    wr(start + 5 * 24, s + bytes - 8);
    wr(start + 14 * 24, dl);
    wr(start + 14 * 24 + 8, dl);
    wr(start + 15 * 24 + 8, dl);
    r.n = n;
    r.done = n == total;
    return r;
}

static int is_dynamic(jit_cpu_t *J, uint64_t a, uint64_t start) {
    if (bit_get(dyn_map, a >> 3))
        return 1;
    if (a >= start && a < start + 24ULL * JIT_MAX_BLOCK)
        return J->written[(a - start) >> 3];
    return 0;
}

static void *meta_alloc(jit_cpu_t *J, uint32_t words) {
    if (J->nmeta + words > J->max_meta)
        return NULL;
    void *p = &J->meta[J->nmeta];
    J->nmeta += words;
    return p;
}

static int hoisted(const jit_block_t *blk, uint64_t w) {
    for (uint32_t k = 0; k < blk->nhoist; k++)
        if (blk->hoist[k] == w)
            return 1;
    return 0;
}

/* (w & keep) | part, as a new value */
static cval_t masked_or(jit_cpu_t *J, cval_t w, uint64_t keep, cval_t part) {
    if (w.kind == 2 && part.kind == 2)
        return (cval_t){ 2, -1, (w.val & keep) | part.val };
    int r = scratch(J);
    if (w.kind == 2) {
        mov_imm(J, r, w.val & keep);
    } else {
        mov_rr(J, r, w.reg);
        and_imm64(J, r, keep);
    }
    or_val(J, r, part);
    return (cval_t){ 1, r, 0 };
}

/* after v was stored to unaligned a, the cached words it spans, put together
 * as the store left them; clobbers the flags */
static void update_spanned(jit_cpu_t *J, uint64_t a, cval_t v) {
    int k = (a & 7) * 8;
    uint64_t lo = a & ~7ULL, hi = lo + 8;
    for (int i = 0; i < NREGS; i++)
        J->regs[i].pinned = 0;
    if (v.kind == 1)
        pin(J, v.reg);
    cval_t lv = cache_find(J, lo), hv = cache_find(J, hi);
    if (lv.kind == 1)
        pin(J, lv.reg);
    if (hv.kind == 1)
        pin(J, hv.reg);
    if (lv.kind) {
        cval_t n = masked_or(J, lv, ~(~0ULL >> k), shifted(J, v, 0, k));
        if (n.kind == 2)
            set_const(J, lo, n.val);
        else
            bind(J, n.reg, lo);
    }
    if (hv.kind) {
        cval_t n = masked_or(J, hv, ~0ULL >> k, shifted(J, v, 1, 64 - k));
        if (n.kind == 2)
            set_const(J, hi, n.val);
        else
            bind(J, n.reg, hi);
    }
}

/* emit one instruction at p, the i-th of the block; returns whether it ended
 * the block with an unconditional jump */
static int translate_insn(jit_cpu_t *J, jit_block_t *blk, uint64_t p, uint32_t i, int *nstubs, int *nsmc) {
    uint64_t start = blk->start;
    uint64_t raw[3];
    for (int f = 0; f < 3; f++)
        raw[f] = *(volatile uint64_t *)(p + 8 * f);
    uint64_t fa = __builtin_bswap64(raw[0]), fb = __builtin_bswap64(raw[1]), fc = __builtin_bswap64(raw[2]);
    int dyna = is_dynamic(J, p, start);
    int dynb = is_dynamic(J, p + 8, start);
    int dync = is_dynamic(J, p + 16, start);

    for (int k = 0; k < NREGS; k++)
        J->regs[k].pinned = 0;

    /* fields marked static: their values are baked in, and hashed as read
     * here for the check against memory later */
    for (int f = 0; f < 3; f++) {
        if (!is_dynamic(J, p + 8 * f, start)) {
            uint32_t bitno = i * 3 + f;
            blk->smask[bitno >> 6] |= 1ULL << (bitno & 63);
            blk->shash = (blk->shash ^ raw[f]) * 0x100000001b3ULL;
            mark_static((p >> 3) + f);
        }
    }

    /* dynamic fields known here as constants are static after all */
    cval_t fav = { 0, -1, 0 }, fbv = { 0, -1, 0 }, fcv = { 0, -1, 0 };
    if (dyna) {
        fav = cache_find(J, p);
        if (fav.kind == 2) { dyna = 0; fa = fav.val; }
        else if (fav.kind == 1) pin(J, fav.reg);
    }
    if (dynb) {
        fbv = cache_find(J, p + 8);
        if (fbv.kind == 2) { dynb = 0; fb = fbv.val; }
        else if (fbv.kind == 1) pin(J, fbv.reg);
    }
    if (dync) {
        fcv = cache_find(J, p + 16);
        if (fcv.kind == 2) { dync = 0; fc = fcv.val; }
        else if (fcv.kind == 1) pin(J, fcv.reg);
    }

    /* check the store target against translated code, leaving with the
     * instruction not run; static targets were mostly checked on entry, but
     * B may have changed since, or be known only here */
    int rb = -1;
    if (!dynb) {
        for (uint64_t w = fb >> 3; w <= (fb + 7) >> 3 && w < (mem_size >> 3); w++) {
            if (hoisted(blk, w))
                continue;
            test_bitmap(J, w);
            J->smcs[*nsmc].idx = i;
            J->smcs[*nsmc].pend = snap_pending(J);
            J->smcs[*nsmc].precise = 0;
            J->smcs[(*nsmc)++].site = jcc32(J, CC_NZ);
        }
    } else {
        if (fbv.kind == 1) {
            rb = fbv.reg;
        } else {
            rb = scratch(J);
            load_static(J, rb, p + 8);
            bind(J, rb, p + 8);
        }
        /* the page's byte here, the words' bits out of line */
        int t1 = scratch(J), t2 = scratch(J);
        mov_rr(J, t1, rb);
        shr_ri(J, t1, 12);
        cmp_byte_zero(J, R14, t1, (int32_t)page_off);
        smc_t *m = &J->smcs[(*nsmc)++];
        m->idx = i;
        m->pend = snap_pending(J);
        m->precise = 1;
        m->rb = rb;
        m->t1 = t1;
        m->t2 = t2;
        m->site = jcc32(J, CC_NZ);
        m->back = J->cp;
        unpin(J, t1);
        unpin(J, t2);
    }

    /* operands */
    int same = !dyna && !dynb && fa == fb;
    cval_t va = { 0, -1, 0 }, vb = { 0, -1, 0 };
    if (!same) {
        if (!dyna) {
            va = get_static(J, fa);
        } else {
            int ra;
            if (fav.kind == 1) {
                ra = fav.reg;
            } else {
                ra = scratch(J);
                load_static(J, ra, p);
                bind(J, ra, p);
            }
            flush_pending(J);
            va.kind = 1;
            va.reg = scratch(J);
            load_mem(J, va.reg, ra, 0);
        }
        if (!dynb) {
            vb = get_static(J, fb);
        } else {
            flush_pending(J);
            vb.kind = 1;
            vb.reg = scratch(J);
            load_mem(J, vb.reg, rb, 0);
        }
    }

    /* a dynamic C is read before the store, as the instruction would */
    int rc = -1;
    if (dync) {
        if (fcv.kind == 1) {
            rc = fcv.reg;
        } else {
            rc = scratch(J);
            load_static(J, rc, p + 16);
        }
    }

    /* the result, and the branch: known (taken 0 or 1) or in the flags */
    cval_t res;
    int taken = -1;
    int unchanged = 0;
    if (same) {
        res = (cval_t){ 2, -1, 0 };
        taken = 1;
    } else if (va.kind == 2 && vb.kind == 2) {
        res = (cval_t){ 2, -1, vb.val - va.val };
        taken = (int64_t)vb.val <= (int64_t)va.val;
    } else if (va.kind == 2 && va.val == 0) {
        res = vb;
        test_rr(J, res.reg, res.reg);
        unchanged = 1;
    } else {
        int rr;
        if (vb.kind == 2 && vb.val == 0) {
            rr = scratch(J);
            mov_rr(J, rr, va.reg);
            neg_r(J, rr);
        } else {
            int ib = vb.kind == 1 ? reg_index(vb.reg) : -1;
            /* compute in B's register if it holds nothing but B */
            if (ib >= 0 && (!J->regs[ib].used || (!dynb && J->regs[ib].addr == fb))) {
                rr = vb.reg;
            } else {
                rr = scratch(J);
                if (vb.kind == 2)
                    mov_imm(J, rr, vb.val);
                else
                    mov_rr(J, rr, vb.reg);
            }
            sub_rr(J, rr, to_reg(J, va));
        }
        res = (cval_t){ 1, rr, 0 };
        pin(J, rr);
    }

    /* store: to a static word it is held back, otherwise any held back
     * store goes first; subtracting 0 leaves memory as it is */
    int pendable = !dynb && cacheable(fb);
    if (!unchanged) {
        if (!pendable) {
            flush_pending(J);
            if (res.kind == 2)
                store_const(J, res.val, dynb, fb, rb);
            else
                store_reg(J, res.reg, dynb, fb, rb);
        } else if (J->pend && J->pend_addr != fb) {
            flush_pending(J);
        }
    }

    /* what the store did to the cache; an unaligned one is put together
     * after the branch, which needs the flags */
    int spanned = 0;
    if (dynb) {
        if (!unchanged)
            cache_drop_all(J);
    } else if (pendable) {
        if (res.kind == 2)
            set_const(J, fb, res.val);
        else
            bind(J, res.reg, fb);
        if (!unchanged) {
            J->pend = 1;
            J->pend_addr = fb;
        }
    } else if ((fb & 7) && cacheable(fb & ~7ULL) && cacheable((fb & ~7ULL) + 8) && !unchanged) {
        spanned = 1;
    } else if (!unchanged) {
        cache_drop(J, fb & ~7ULL);
        cache_drop(J, (fb & ~7ULL) + 8);
    }

    /* branch */
    int ended = 0;
    if (taken != 0 && (dync || fc != p + 24)) {
        stub_t *s = &J->stubs[(*nstubs)++];
        s->dyn = dync;
        s->idx = i;
        s->target = fc;
        s->creg = rc;
        s->pend = snap_pending(J);
        if (taken == 1) {
            s->site = jmp32(J);
            ended = 1;
        } else {
            s->site = jcc32(J, CC_LE);
        }
    }
    if (spanned && !ended)
        update_spanned(J, fb, res);
    return ended;
}

static jit_block_t *translate(jit_cpu_t *J, uint64_t start) {
    if ((start & 7) || start + 24 > mem_size)
        return NULL;
    if (J->code_end - J->code_ptr < CODE_SLACK || J->nblocks == J->max_blocks ||
        J->nmeta + JIT_MAX_BLOCK * 2 + 8 > J->max_meta)
        flush(J);

    /* the word copy loop is a block of its own */
    uint64_t desc[7];
    int copy = match_copy(start, desc);

    /* the extent of the block, and which of its own fields it writes */
    __builtin_memset(J->written, 0, sizeof J->written);
    uint32_t n = 0;
    for (uint64_t p = start; n < (copy ? 19 : JIT_MAX_BLOCK) && p + 24 <= mem_size; p += 24) {
        if (p != start && !copy && lookup(J, p))
            break;
        uint64_t a = rd(p), b = rd(p + 8), c = rd(p + 16);
        n++;
        if (!bit_get(dyn_map, (p + 8) >> 3)) {
            for (uint64_t x = b & ~7ULL; x < b + 8; x += 8)
                if (x >= start && x < start + 24ULL * JIT_MAX_BLOCK)
                    J->written[(x - start) >> 3] = 1;
        }
        int dyn = bit_get(dyn_map, p >> 3) || bit_get(dyn_map, (p + 8) >> 3) ||
                  bit_get(dyn_map, (p + 16) >> 3);
        if (!dyn && a == b && c != p + 24)
            break;
    }

    if (n != 19)
        copy = 0;

    /* the words stored to through static addresses, checked once on entry,
     * as nothing else gets translated while the block runs */
    uint32_t nhoist = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint64_t p = start + 24ULL * k;
        if (is_dynamic(J, p + 8, start))
            continue;
        uint64_t b = rd(p + 8);
        for (uint64_t w = b >> 3; w <= (b + 7) >> 3 && w < (mem_size >> 3); w++) {
            uint32_t j = 0;
            while (j < nhoist && J->hoist[j] != w)
                j++;
            if (j == nhoist)
                J->hoist[nhoist++] = w;
        }
    }

    jit_block_t *blk = &J->blocks[J->nblocks++];
    __builtin_memset(blk, 0, sizeof *blk);
    blk->shash = 0x9e3779b97f4a7c15ULL;
    blk->start = start;
    blk->valid = 1;
    blk->nhoist = nhoist;
    blk->hoist = meta_alloc(J, nhoist);
    __builtin_memcpy(blk->hoist, J->hoist, nhoist * sizeof(uint64_t));

    J->cp = J->code_ptr;
    blk->entry = J->cp;
    alu_imm(J, 5, RBP, 0);              /* sub rbp, n: patched below */
    uint8_t *budget_imm = J->cp - 4;
    uint8_t *budget_site = jcc32(J, CC_L);

    /* cmp [watch_addr], rax with rax = watch value */
    uint8_t *watch_site = NULL;
    if (J->watch_addr) {
        mov_imm(J, RAX, __builtin_bswap64(J->watch_value));
        if (fits_disp(J->watch_addr)) {
            rex(J, 1, RAX, 0, R15); e8(J, 0x39); mem_operand(J, RAX, R15, (int32_t)(J->watch_addr - BIAS));
        } else {
            mov_imm(J, RCX, J->watch_addr);
            rex(J, 1, RAX, 0, RCX); e8(J, 0x39); mem_operand(J, RAX, RCX, 0);
        }
        watch_site = jcc32(J, CC_NZ);
    }

    /* one test per bitmap dword */
    int nhs = 0;
    __builtin_memset(J->hoist_done, 0, nhoist);
    for (uint32_t k = 0; k < nhoist; k++) {
        if (J->hoist_done[k])
            continue;
        uint64_t d = J->hoist[k] >> 5;
        uint32_t mask = 0;
        for (uint32_t j = k; j < nhoist; j++) {
            if ((J->hoist[j] >> 5) == d) {
                mask |= 1U << (J->hoist[j] & 31);
                J->hoist_done[j] = 1;
            }
        }
        /* test dword [r14 + 4d], mask */
        rex(J, 0, 0, 0, R14); e8(J, 0xf7); mem_operand(J, 0, R14, (int32_t)(d << 2)); e32(J, mask);
        J->hoist_sites[nhs++] = jcc32(J, CC_NZ);
    }

    int nstubs = 0, nsmc = 0, ended = 0;

    /* the word copy loop: as many iterations as jit_copy() runs, taking
     * them from the budget past the first, then back to the entry or out;
     * none means the loop runs below */
    if (copy) {
        uint64_t *d = meta_alloc(J, 7);
        __builtin_memcpy(d, desc, sizeof desc);
        mov_imm(J, RDI, (uint64_t)d);
        mov_rr(J, RSI, RBP);
        mov_imm(J, RAX, (uint64_t)jit_copy);
        e8(J, 0xff); e8(J, 0xd0);                       /* call rax */
        test_rr(J, RAX, RAX);
        uint8_t *none = jcc32(J, 0x4);                  /* jz */
        e8(J, 0x48); e8(J, 0x6b); e8(J, 0xc8); e8(J, 19);  /* imul rcx, rax, 19 */
        alu_imm(J, 5, RCX, 19);
        sub_rr(J, RBP, RCX);
        test_rr(J, RDX, RDX);
        stub_t *st = &J->stubs[nstubs++];
        st->dyn = 0;
        st->idx = 18;
        st->target = start + 19 * 24;
        st->creg = -1;
        st->pend = (pending_t){ 0, -1, 0, 0 };
        st->site = jcc32(J, CC_NZ);
        patch_rel32(jmp32(J), blk->entry);
        patch_rel32(none, J->cp);
    }

    for (int k = 0; k < NREGS; k++)
        J->regs[k] = (creg_t){ 0 };
    for (int k = 0; k < NCONST; k++)
        J->consts[k].used = 0;
    J->pend = 0;

    uint32_t i;
    for (i = 0; i < n && !ended; i++)
        ended = translate_insn(J, blk, start + 24ULL * i, i, &nstubs, &nsmc);
    n = i;
    blk->n = n;
    __builtin_memcpy(budget_imm, &n, 4);
    if (!ended) {
        stub_t *s = &J->stubs[nstubs++];
        s->dyn = 0;
        s->idx = n - 1;
        s->target = start + 24ULL * n;
        s->pend = snap_pending(J);
        s->site = jmp32(J);
    }

    /* out of line: not enough budget left to run the block */
    patch_rel32(budget_site, J->cp);
    alu_imm(J, 0, RBP, (int32_t)n);
    exit_to(J, start, 0);
    /* the watched word changed */
    if (watch_site) {
        patch_rel32(watch_site, J->cp);
        alu_imm(J, 0, RBP, (int32_t)n);
        exit_to(J, start, 3);
    }
    /* entered after the block was dropped, before taking the budget */
    blk->inv_stub = J->cp;
    exit_to(J, start, 0);
    /* a static store target turned out to be translated code */
    if (nhs) {
        for (int k = 0; k < nhs; k++)
            patch_rel32(J->hoist_sites[k], J->cp);
        alu_imm(J, 0, RBP, (int32_t)n);
        exit_to(J, start, 2);
    }
    /* a computed one did, or one went to a page with translated code, whose
     * words (rb >> 3) and ((rb + 7) >> 3) tell: instruction idx not run */
    for (int k = 0; k < nsmc; k++) {
        smc_t *m = &J->smcs[k];
        patch_rel32(m->site, J->cp);
        if (m->precise) {
            uint8_t *hit[2];
            for (int h = 0; h < 2; h++) {
                if (h == 0)
                    mov_rr(J, m->t1, m->rb);
                else
                    op_mem(J, 0x8d, m->t1, m->rb, 7);
                shr_ri(J, m->t1, 3);
                mov_rr(J, m->t2, m->t1);
                shr_ri(J, m->t2, 6);
                load_sib8(J, m->t2, R14, m->t2);
                bt_rr(J, m->t2, m->t1);
                hit[h] = jcc32(J, CC_C);
            }
            patch_rel32(jmp32(J), m->back);
            patch_rel32(hit[0], J->cp);
            patch_rel32(hit[1], J->cp);
        }
        stub_store(J, &m->pend, -1);
        alu_imm(J, 0, RBP, (int32_t)(n - m->idx));
        exit_to(J, start + 24ULL * m->idx, 1);
    }
    /* branches out */
    for (int k = 0; k < nstubs; k++) {
        stub_t *s = &J->stubs[k];
        patch_rel32(s->site, J->cp);
        stub_store(J, &s->pend, s->dyn ? s->creg : -1);
        uint32_t unexec = n - s->idx - 1;
        if (unexec)
            alu_imm(J, 0, RBP, (int32_t)unexec);
        if (s->dyn) {
            mov_rr(J, R13, s->creg);
            mov_imm(J, RBX, 0);
            jmp_r(J, R12);
        } else {
            /* goes to the slow path right after until linked */
            uint8_t *site = jmp32(J);
            patch_rel32(site, J->cp);
            mov_imm(J, R13, s->target);
            e8(J, 0x48); e8(J, 0x8d); e8(J, 0x1d);     /* lea rbx, [rip + site] */
            e32(J, (uint32_t)(int32_t)(site - (J->cp + 4)));
            jmp_r(J, R12);
        }
    }

    J->stats.code_bytes += J->cp - J->code_ptr;
#ifdef JIT_HOSTED
    jit_os_translated(start, n, J->code_ptr, J->cp - J->code_ptr);
#endif
    J->code_ptr = J->cp;

    uint64_t h = hash(start);
    blk->hnext = J->htab[h];
    J->htab[h] = blk;

    /* to be checked against memory once every store that could have missed
     * the marks is visible; when too many wait, wait for the oldest */
    uint32_t next = (J->ptail + 1) % PENDING_LEN;
    while (next == J->phead) {
        revalidate(J);
        if (next == J->phead)
            asm volatile ("pause");
    }
    J->pending[J->ptail].b = blk;
    J->pending[J->ptail].stamp = __atomic_add_fetch(&epoch, 1, __ATOMIC_SEQ_CST);
    J->ptail = next;

    J->stats.translations++;
    J->stats.translated_insns += n;
    return blk;
}

/* ------------------------------------------------------------------------ */

size_t jit_init(uint64_t msize, int n, size_t max_bytes, int movbe) {
    if (n > JIT_MAX_CPUS)
        n = JIT_MAX_CPUS;
    mem_size = msize;
    ncpus = n;
    use_movbe = movbe;

    size_t map_bytes = ((msize >> 6) + 15) & ~7ULL;
    size_t page_bytes = (msize >> 12) + 1;
    size_t fixed = 2 * map_bytes + page_bytes + n * (sizeof(jit_cpu_t) + (sizeof(jit_block_t *) << HASH_BITS));
    if (max_bytes <= fixed)
        return 0;
    /* split the rest between the CPUs' code caches and block records, with
     * one block per 512 bytes of code */
    size_t per_cpu = (max_bytes - fixed) / n;
    size_t unit = 512 + sizeof(jit_block_t) + 2 * sizeof(jit_link_t) + 16 * sizeof(uint64_t);
    size_t max_blocks = per_cpu / unit;
    if (max_blocks > (1 << 18))
        max_blocks = 1 << 18;
    if (max_blocks < 4096)
        return 0;

    size_t total = 0;
    static_map = jit_os_alloc(map_bytes + page_bytes);
    dyn_map = jit_os_alloc(map_bytes);
    if (!static_map || !dyn_map)
        return 0;
    page_map = static_map + map_bytes;
    page_off = map_bytes;
    total += 2 * map_bytes + page_bytes;
    for (int c = 0; c < n; c++) {
        jit_cpu_t *J = jit_os_alloc(sizeof *J);
        if (!J)
            return 0;
        J->cpu = c;
        J->strong = 1;
#ifdef JIT_HOSTED
        J->exact = 1;
#endif
        J->max_blocks = max_blocks;
        J->max_links = 2 * max_blocks;
        J->max_meta = 16 * max_blocks;
        size_t code_bytes = 512 * max_blocks;
        J->code_base = jit_os_alloc(code_bytes);
        J->blocks = jit_os_alloc(max_blocks * sizeof(jit_block_t));
        J->links = jit_os_alloc(J->max_links * sizeof(jit_link_t));
        J->meta = jit_os_alloc(J->max_meta * sizeof(uint64_t));
        J->htab = jit_os_alloc(sizeof(jit_block_t *) << HASH_BITS);
        if (!J->code_base || !J->blocks || !J->links || !J->meta || !J->htab)
            return 0;
        J->code_ptr = J->code_base;
        J->code_end = J->code_base + code_bytes;
        J->st.bitmap = static_map;
        cpus[c] = J;
        quiet[c] = ~0ULL;
        total += sizeof *J + code_bytes + max_blocks * sizeof(jit_block_t) +
                 J->max_links * sizeof(jit_link_t) + J->max_meta * sizeof(uint64_t) +
                 (sizeof(jit_block_t *) << HASH_BITS);
    }
    jit_enabled = 1;
    return total;
}

void jit_set_copy(void (*copy)(void *, const void *, size_t)) {
    copy_fn = copy;
}

void jit_set_watch(int cpu, uint64_t addr, uint64_t value) {
    cpus[cpu]->watch_addr = addr;
    cpus[cpu]->watch_value = value;
}

void jit_set_mem_size(uint64_t msize) {
    mem_size = msize;
}

jit_stats_t *jit_get_stats(int cpu) {
    return &cpus[cpu]->stats;
}

void jit_sample(int cpu, uint64_t rip) {
    if (!jit_enabled || cpu >= ncpus)
        return;
    jit_cpu_t *J = cpus[cpu];
    uint8_t *p = (uint8_t *)rip;
    if (p < J->code_base || p >= J->code_ptr) {
        J->samples_out++;
        uint64_t line = rip & ~63ULL;
        uint32_t h = (uint32_t)((line >> 6) * 0x9e3779b1U) >> 26;
        for (int k = 0; k < 64; k++, h = (h + 1) & 63) {
            if (J->out[h].line == line || !J->out[h].count) {
                J->out[h].line = line;
                J->out[h].count++;
                break;
            }
        }
        return;
    }
    /* blocks lie in the code cache in the order they were made */
    uint32_t lo = 0, hi = J->nblocks;
    while (hi - lo > 1) {
        uint32_t mid = (lo + hi) / 2;
        if (J->blocks[mid].entry <= p)
            lo = mid;
        else
            hi = mid;
    }
    if (J->nblocks) {
        J->blocks[lo].samples++;
        J->samples++;
    }
}

int jit_top_samples(int cpu, int max, uint64_t *starts, uint32_t *counts, uint32_t *total) {
    jit_cpu_t *J = cpus[cpu];
    int n = 0;
    /* lines outside translated code count as blocks starting there */
    for (int k = 0; k < 64; k++) {
        uint32_t c = J->out[k].count;
        if (!c)
            continue;
        J->out[k].count = 0;
        int i = n < max ? n++ : max - 1;
        if (i == max - 1 && n == max && counts[i] >= c)
            continue;
        while (i > 0 && counts[i - 1] < c) {
            starts[i] = starts[i - 1];
            counts[i] = counts[i - 1];
            i--;
        }
        starts[i] = J->out[k].line;
        counts[i] = c;
    }
    for (uint32_t b = 0; b < J->nblocks; b++) {
        uint32_t c = J->blocks[b].samples;
        if (!c)
            continue;
        J->blocks[b].samples = 0;
        int k = n < max ? n++ : max - 1;
        if (k == max - 1 && n == max && counts[k] >= c)
            continue;
        while (k > 0 && counts[k - 1] < c) {
            starts[k] = starts[k - 1];
            counts[k] = counts[k - 1];
            k--;
        }
        starts[k] = J->blocks[b].start;
        counts[k] = c;
    }
    *total = J->samples + J->samples_out;
    J->samples = J->samples_out = 0;
    return n;
}

static void link_to(jit_cpu_t *J, uint8_t *site, jit_block_t *t) {
    jit_link_t *l = J->free_links;
    if (l)
        J->free_links = l->next;
    else if (J->nlinks < J->max_links)
        l = &J->links[J->nlinks++];
    else
        return;
    l->site = site;
    l->next = t->incoming;
    t->incoming = l;
    patch_rel32(site, t->entry);
    J->stats.links++;
}

uint64_t jit_run(int cpu, uint64_t eip, uint64_t budget) {
    jit_cpu_t *J = cpus[cpu];
    if (eip != J->last_eip) {
        /* resuming somewhere else, likely mid-block */
        J->strong = 0;
        J->weak_steps = 0;
    }
    J->st.eip = eip;
    J->st.budget = budget;

    for (;;) {
        __atomic_store_n(&quiet[cpu], __atomic_load_n(&epoch, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
        drain_queue(J);
        revalidate(J);

        jit_block_t *b = lookup(J, J->st.eip);
        if (!b && (J->strong || J->weak_steps >= JIT_MAX_BLOCK))
            b = translate(J, J->st.eip);
        if (!b || (J->exact && b->n > J->st.budget)) {
            if (!J->st.budget)
                break;
            uint64_t next = interp_step(J, J->st.eip);
            J->strong = next != J->st.eip + 24;
            J->weak_steps = J->strong ? 0 : J->weak_steps + 1;
            J->st.eip = next;
            J->st.budget--;
            continue;
        }
        if (b->n > J->st.budget)
            break;

        uint64_t gen = J->stats.flushes;
        jit_enter(b->entry, &J->st);
        J->stats.entries++;
        J->strong = 1;
        J->weak_steps = 0;

        if (J->st.exit == 3) {
            /* the watched word changed */
            break;
        } else if (J->st.exit == 1) {
            /* the instruction at eip stores to translated code */
            J->stats.smc_exits++;
            uint64_t next = interp_step(J, J->st.eip);
            J->strong = next != J->st.eip + 24;
            J->st.eip = next;
            J->st.budget--;
        } else if (J->st.exit == 2) {
            /* the block at eip, maybe reached through links, stores to
             * translated code through a static address: drop that code */
            J->stats.hoist_exits++;
            jit_block_t *f = lookup(J, J->st.eip);
            for (uint32_t k = 0; f && k < f->nhoist; k++)
                if (bit_get(static_map, f->hoist[k]))
                    smc_hit(J, f->hoist[k]);
        } else if (J->st.exit > 3) {
            /* link the jump that left to its target */
            uint8_t *site = (uint8_t *)J->st.exit;
            jit_block_t *t = lookup(J, J->st.eip);
            if (!t)
                t = translate(J, J->st.eip);
            if (t && gen == J->stats.flushes)
                link_to(J, site, t);
        }
    }

    __atomic_store_n(&quiet[cpu], ~0ULL, __ATOMIC_RELEASE);
    J->stats.executed += budget - J->st.budget;
    J->last_eip = J->st.eip;
    return J->st.eip;
}
