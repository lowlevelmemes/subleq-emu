#ifndef __PMM_H__
#define __PMM_H__

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

/* Page sizes */
#define PAGE_SIZE_4K            0x1000
#define PAGE_SIZE_2M            0x200000

/* Default page size for PMM */
#define PAGE_SIZE               PAGE_SIZE_4K

/* HHDM offset is provided by Limine at runtime */
extern uint64_t hhdm_offset;
#define PHYS_MEM_OFFSET hhdm_offset

/* Kernel physical and virtual base addresses from Limine */
extern uint64_t kernel_phys_base;
extern uint64_t kernel_virt_base;

/* Limine memory map for paging setup */
extern struct limine_memmap_response *limine_memmap;

/* Kernel ELF file from executable file request */
extern struct limine_file *kernel_file;

/* ELF64 types for PHDR parsing */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

#define PT_LOAD 1

/* Physical memory allocation */
void *kmalloc(size_t pages);
void kmfree(void *ptr, size_t pages);

/* Initialize PMM from Limine memory map */
void pmm_init(void);

#endif
