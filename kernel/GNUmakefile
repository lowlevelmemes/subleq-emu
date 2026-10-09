# Nuke built-in rules.
.SUFFIXES:

# Delete the target of a failed recipe.
.DELETE_ON_ERROR:

# This is the name that our final executable will have.
# Change as needed.
override OUTPUT := kernel

# Install prefix; /usr/local is a good, standard default pick.
PREFIX := /usr/local

# User controllable toolchain and toolchain prefix.
TOOLCHAIN :=
TOOLCHAIN_PREFIX :=
ifneq ($(TOOLCHAIN),)
    ifeq ($(TOOLCHAIN_PREFIX),)
        TOOLCHAIN_PREFIX := $(TOOLCHAIN)-
    endif
endif

# User controllable C compiler command.
ifneq ($(TOOLCHAIN_PREFIX),)
    CC := $(TOOLCHAIN_PREFIX)gcc
else
    CC := cc
endif

# Defaults overrides for variables if using "llvm" as toolchain.
ifeq ($(TOOLCHAIN),llvm)
    CC := clang
endif

# User controllable C flags.
CFLAGS := -g -O2 -pipe

# User controllable C preprocessor flags. We set none by default.
CPPFLAGS :=

# User controllable nasm flags.
NASMFLAGS := -g

# User controllable linker flags. We set none by default.
LDFLAGS :=

# Ensure the dependencies have been obtained.
ifneq ($(filter-out clean distclean,$(or $(MAKECMDGOALS),default)),)
    ifeq ($(wildcard .deps-obtained),)
        $(error Please run the ./get-deps script first)
    endif
endif

# Stop on a problem the checks below find, unless only cleaning, which needs
# no toolchain.
ifneq ($(filter-out clean distclean,$(or $(MAKECMDGOALS),default)),)
    override TOOLCHAIN_ERROR = $(error $(1))
else
    override TOOLCHAIN_ERROR =
endif

# Check if CC is Clang.
override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep -q '^Target: '; echo $$?)

# Macros to check if a flag is supported, used as $(call MACRO,flag), and
# expanding to 1 or 0. A second argument names another spelling.

# The name of a flag, up to any "=", with the dashes after the first made
# optional, as compilers leave the value out or spell the flag differently.
override FLAG_NAME = -$(subst -,-?,$(patsubst -%,%,$(firstword $(subst =, ,$(1)))))

# Look for a diagnostic naming the flag, in the untranslated C locale. The
# name has to stand on its own, as "-pie" would otherwise be found inside
# a complaint about "-no-pie".
override FLAG_IS_NAMED = grep -qE '(error|warning|note):.*[^[:alnum:]]($(call FLAG_NAME,$(1))$(if $(2),|$(call FLAG_NAME,$(2))))([^-[:alnum:]]|$$)'

# Check if the compiler supports a flag when compiling.
override CC_HAS_COMPILE_FLAG = $(shell LC_ALL=C $(CC) $(CFLAGS) $(1) -c -x c /dev/null -o /dev/null 2>&1 >/dev/null | $(call FLAG_IS_NAMED,$(1),$(2)); echo $$?)

# Check if the compiler supports a flag when linking, linking nothing.
override CC_HAS_LINK_FLAG = $(shell LC_ALL=C $(CC) $(CFLAGS) $(LDFLAGS) $(1) -Wl,--version 2>&1 >/dev/null | $(call FLAG_IS_NAMED,$(1),$(2)); echo $$?)

# Check if the linker supports a flag by searching its help, as older GNU
# ld take unknown flags for different ones (-no-pie for -n -o -pie).
override LD_HAS_FLAG = $(shell ! LC_ALL=C $(CC) $(CFLAGS) $(LDFLAGS) -Wl,--help 2>/dev/null | grep -qE '(^|[[:space:]])-?$(1)([[:space:],=[]|$$)'; echo $$?)

# Internal C flags that should not be changed by the user.
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-common \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -m64 \
    -march=x86-64 \
    -mno-mmx \
    -mno-sse \
    -mno-red-zone \
    -mcmodel=kernel

# Internal C preprocessor flags that should not be changed by the user.
override CPPFLAGS := \
    -nostdinc \
    -I src \
    -I limine-protocol/include \
    -isystem freestanding-c-hdrs/include \
    $(CPPFLAGS) \
    -MMD \
    -MP

# Internal nasm flags that should not be changed by the user.
override NASMFLAGS := \
    -f elf64 \
    $(patsubst -g,-g -F dwarf,$(NASMFLAGS)) \
    -i src/ \
    -Wall

# Always ask for a build ID, naming its style, as compilers and linkers
# differ on both. Comes before LDFLAGS, which can override it.
override LDFLAGS := \
    -Wl,--build-id=sha1 \
    $(LDFLAGS)

# If the C compiler is Clang, set the target as needed.
ifeq ($(CC_IS_CLANG),1)
    override CC += \
        -target x86_64-unknown-none-elf
    # Use LLD unless LDFLAGS picks another linker.
    override LDFLAGS := \
        -fuse-ld=lld \
        $(LDFLAGS)
endif

# Internal linker flags that should not be changed by the user.
override LDFLAGS += \
    -nostdlib \
    -static \
    -Wl,-z,max-page-size=0x1000 \
    -Wl,-z,noexecstack \
    -Wl,--gc-sections \
    -Wl,-T,linker-scripts/x86_64.lds \
    -Wl,-m,elf_x86_64

# Check that the compiler works with the given flags, as the checks below
# would otherwise blame a missing feature.
override CC_COMPILE_ERROR := $(shell LC_ALL=C $(CC) $(CFLAGS) -c -x c /dev/null -o /dev/null 2>&1 >/dev/null | grep -i 'error:' | head -n 1)
ifneq ($(CC_COMPILE_ERROR),)
    $(call TOOLCHAIN_ERROR,The compiler does not work with the given CFLAGS: $(CC_COMPILE_ERROR))
endif

# Check that the compiler emits ELF objects. Go by the macro for ELF targets,
# as GNU as cannot write an object to standard output. Clang is given an ELF
# target, and before version 17 did not define the macro for all of them.
ifneq ($(CC_IS_CLANG),1)
    override CC_EMITS_ELF := $(shell ! $(CC) $(CFLAGS) -dM -E -x c /dev/null 2>/dev/null | grep -q 'define __ELF__ 1'; echo $$?)
    ifneq ($(CC_EMITS_ELF),1)
        $(call TOOLCHAIN_ERROR,The compiler does not emit ELF objects)
    endif
endif

ifeq ($(CC_IS_CLANG),1)
    # Clang hands freestanding links over to another compiler driver on
    # some targets. Look for the linker flags being passed on as they
    # are, which a linker Clang runs itself would be given translated,
    # as the driver it picks need not be named "gcc" at all. The check
    # is made again below, so it runs on each use. The print flag is
    # kept in a variable, as older makes take a "#" in a call for a
    # comment.
    override CLANG_PRINT_COMMANDS := -\#\#\#
    override CC_LINKS_VIA_DRIVER = $(shell ! $(CC) $(CFLAGS) $(LDFLAGS) -Wl,--version $(CLANG_PRINT_COMMANDS) 2>&1 | grep '^ "' | tail -n 1 | grep -q -- '-Wl,'; echo $$?)

    # If it does, link as for the Linux target, where Clang runs the
    # linker itself, and keep that target's configuration files out. The
    # flag is looked for by compiling, as linking is not checked yet.
    ifeq ($(CC_LINKS_VIA_DRIVER),1)
        override LDFLAGS += \
            -target x86_64-linux-gnu
        ifeq ($(call CC_HAS_COMPILE_FLAG,--no-default-config),1)
            override LDFLAGS += \
                --no-default-config
        endif

        # Check that this worked, as the linker flags would otherwise
        # reach a compiler driver that knows nothing of them.
        ifeq ($(CC_LINKS_VIA_DRIVER),1)
            $(call TOOLCHAIN_ERROR,The compiler still hands the link over to another compiler driver)
        endif
    endif
endif

# The same for linking, once Clang has a linker it can run.
override CC_LINK_ERROR := $(shell LC_ALL=C $(CC) $(CFLAGS) $(LDFLAGS) -Wl,--version 2>&1 >/dev/null | grep -i 'error:' | head -n 1)
ifneq ($(CC_LINK_ERROR),)
    $(call TOOLCHAIN_ERROR,The linker does not work with the given LDFLAGS: $(CC_LINK_ERROR))
endif

# Some drivers ignore -fuse-ld on some targets, so check that the banner
# of the linker that runs names the one asked for, in any case.
override CC_FUSE_LD := $(lastword $(filter -fuse-ld=%,$(CFLAGS) $(LDFLAGS)))
ifneq ($(CC_FUSE_LD),)
    override CC_FUSE_LD_NAME := $(firstword $(subst -, ,$(patsubst ld.%,%,$(notdir $(patsubst -fuse-ld=%,%,$(CC_FUSE_LD))))))
    # GNU ld calls itself ld, not bfd.
    override CC_LD_BANNER_NAME := $(patsubst bfd,ld,$(CC_FUSE_LD_NAME))
    override CC_LD_VERSION := $(shell LC_ALL=C $(CC) $(CFLAGS) $(LDFLAGS) -Wl,--version 2>/dev/null | head -n 1)
    override CC_RUNS_FUSE_LD := $(shell ! printf '%s\n' '$(subst ',,$(CC_LD_VERSION))' | grep -qiE '(^|[^[:alnum:]])$(CC_LD_BANNER_NAME)([^[:alnum:]]|$$)'; echo $$?)
    ifneq ($(CC_RUNS_FUSE_LD),1)
        $(call TOOLCHAIN_ERROR,The compiler ignores $(CC_FUSE_LD) and runs another linker: $(CC_LD_VERSION))
    endif
endif

ifeq ($(call CC_HAS_COMPILE_FLAG,-fno-stack-clash-protection),1)
    override CFLAGS += \
        -fno-stack-clash-protection
endif

ifeq ($(call CC_HAS_COMPILE_FLAG,-fcf-protection=none),1)
    override CFLAGS += \
        -fcf-protection=none
endif

# GCC needs this flag for interrupt handlers and Clang below 3.9 has no
# name for it, where only a long double could reach the x87.
ifeq ($(call CC_HAS_COMPILE_FLAG,-mno-80387),1)
    override CFLAGS += \
        -mno-80387
endif

# Some compilers link static PIEs even when asked for -static, so pass
# them -no-pie when linking if they support it.
ifeq ($(call CC_HAS_LINK_FLAG,-no-pie),1)
    override LDFLAGS += \
        -no-pie
endif

# Tell the linker as well, as the compiler does not always pass this on.
ifeq ($(call LD_HAS_FLAG,-no-pie),1)
    override LDFLAGS += \
        -Wl,-no-pie
endif

# Use "find" to glob all *.c, *.S, and *.asm files in the tree and obtain the
# object and header dependency file names.
override SRCFILES := $(shell find -L src cc-runtime/src -type f 2>/dev/null | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d) $(NASMFILES:.asm=.asm.d))

# Default target. This must come first, before header dependencies.
.PHONY: all
all: bin/$(OUTPUT)

# Include header dependencies.
-include $(HEADER_DEPS)

# Link rules for the final executable.
bin/$(OUTPUT): GNUmakefile linker-scripts/x86_64.lds $(OBJ)
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

# The compiler may emit calls to the memory functions and the compiler
# runtime that LTO does not account for, so never build those with LTO.
obj/src/memory.c.o obj/cc-runtime/src/cc-runtime.c.o: override CFLAGS += -fno-lto

# Compilation rules for *.c files.
obj/%.c.o: %.c GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.S files.
obj/%.S.o: %.S GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: %.asm GNUmakefile
	mkdir -p "$(dir $@)"
	nasm $(NASMFLAGS) -MD $(@:.o=.d) -MP $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf bin obj

# Remove everything built and generated including downloaded dependencies.
.PHONY: distclean
distclean: clean
	rm -rf .deps-obtained .cache compile_commands.json freestanding-c-hdrs cc-runtime limine-protocol

# Install the final built executable to its final on-root location. Not
# install(1), which takes other arguments on illumos and Solaris.
.PHONY: install
install: all
	mkdir -p "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	cp bin/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/"
	chmod 644 "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)"

# Try to undo whatever the "install" target did.
.PHONY: uninstall
uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)"
	-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
