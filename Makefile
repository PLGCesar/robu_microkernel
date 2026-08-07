ARCH ?= x86_64
TARGET := robu_kernel
BUILD_DIR := build
SRC_DIR := src
ARCH_DIR := arch/$(ARCH)
TRACE ?= 0
BUILD_SYS ?= clang

ifeq ($(BUILD_SYS),clang)
  CC := clang
  AS := clang -x assembler-with-cpp
  LD := ld.lld
else ifeq ($(BUILD_SYS),gcc)
  CC := $(ARCH)-elf-gcc
  AS := $(ARCH)-elf-as
  LD := $(ARCH)-elf-ld
else
  $(error Unknown BUILD_SYS '$(BUILD_SYS)' -- expected 'clang' or 'gcc')
endif

CFLAGS := -ffreestanding -O2 -g -Wall -Wextra -Iinclude -I$(ARCH_DIR)/include
CFLAGS += -fno-pic -fno-pie -fno-stack-protector
CFLAGS += -fno-omit-frame-pointer
CFLAGS += -DROBU_TRACE=$(TRACE)

ifeq ($(BUILD_SYS),clang)
	CFLAGS += --target=$(ARCH)-elf
endif

LDFLAGS += -nostdlib -T $(ARCH_DIR)/linker.ld -z noexecstack

ifeq ($(ARCH),x86_64)
	CFLAGS += -mno-red-zone -mno-mmx -mno-sse -mno-sse2
endif
GEN_DIR := $(BUILD_DIR)/generated
CFLAGS += -I$(GEN_DIR)

C_SRCS := $(wildcard $(SRC_DIR)/*/*.c) $(wildcard $(SRC_DIR)/*.c) $(wildcard $(ARCH_DIR)/src/*.c)
ASM_SRCS := $(wildcard $(ARCH_DIR)/src/*.S)

OBJS := $(C_SRCS:%.c=$(BUILD_DIR)/%.c.o) $(ASM_SRCS:%.S=$(BUILD_DIR)/%.S.o)

.PHONY: all clean mlibc minibox

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/main.c.o:

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

APPS_BUILD_DIR := $(BUILD_DIR)/apps
APP_LDFLAGS := -nostdlib -static -z noexecstack

$(APPS_BUILD_DIR)/devfs/devfs.c.o: apps/devfs/devfs.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/devfs/devfs: $(APPS_BUILD_DIR)/devfs/devfs.c.o $(APP_COMMON_OBJ) apps/link/devfs.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/devfs.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/devfs/devfs.c.o $(APP_COMMON_OBJ)

$(APPS_BUILD_DIR)/ramfs/ramfs.c.o: apps/ramfs/ramfs.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/ramfs/ramfs: $(APPS_BUILD_DIR)/ramfs/ramfs.c.o $(APP_COMMON_OBJ) apps/link/ramfs.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/ramfs.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/ramfs/ramfs.c.o $(APP_COMMON_OBJ)
$(APPS_BUILD_DIR)/procfs/procfs.c.o: apps/procfs/procfs.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/procfs/procfs: $(APPS_BUILD_DIR)/procfs/procfs.c.o $(APP_COMMON_OBJ) apps/link/procfs.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/procfs.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/procfs/procfs.c.o $(APP_COMMON_OBJ)
$(APPS_BUILD_DIR)/sysfs/sysfs.c.o: apps/sysfs/sysfs.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/sysfs/sysfs: $(APPS_BUILD_DIR)/sysfs/sysfs.c.o $(APP_COMMON_OBJ) apps/link/sysfs.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/sysfs.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/sysfs/sysfs.c.o $(APP_COMMON_OBJ)

LIBC_SHIM_OBJS := $(patsubst apps/libc/src/%.c,$(APPS_BUILD_DIR)/libc/%.c.o,$(wildcard apps/libc/src/*.c)) \
                  $(APPS_BUILD_DIR)/libc/setjmp.S.o
LIBC_SHIM_CFLAGS := --target=$(ARCH)-elf -ffreestanding -O2 -fno-pic -fno-pie -fno-stack-protector \
                     -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffunction-sections -fdata-sections \
                     -Wall -Wextra -Iinclude -I$(ARCH_DIR)/include -Iapps/libc/include

$(APPS_BUILD_DIR)/libc/%.c.o: apps/libc/src/%.c
	@mkdir -p $(@D)
	$(CC) $(LIBC_SHIM_CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/libc/setjmp.S.o: apps/libc/src/setjmp.S
	@mkdir -p $(@D)
	$(CC) $(LIBC_SHIM_CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/hello_initsys/main.c.o: apps/hello_initsys/main.c
	@mkdir -p $(@D)
	$(CC) $(LIBC_SHIM_CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/hello_initsys/hello_initsys: $(APPS_BUILD_DIR)/hello_initsys/main.c.o $(LIBC_SHIM_OBJS) apps/link/hello_initsys.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/hello_initsys.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/hello_initsys/main.c.o $(LIBC_SHIM_OBJS)

MINIBOX_SRCS := $(filter-out apps/minibox/src/init.c,$(wildcard apps/minibox/src/*.c)) \
                $(wildcard apps/minibox/libmb/*.c) apps/minibox/robu-stubs.c
MINIBOX_OBJS := $(patsubst apps/minibox/%.c,$(APPS_BUILD_DIR)/minibox/%.c.o,$(MINIBOX_SRCS))
MINIBOX_CFLAGS := $(LIBC_SHIM_CFLAGS) -Iapps/minibox/include -Iapps/minibox/libmb \
                   -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-result \
                   -DVERSION=\"0.3.1\" -include apps/minibox/include/config.h

$(MINIBOX_OBJS): $(APPS_BUILD_DIR)/minibox/%.c.o: apps/minibox/%.c
	@mkdir -p $(@D)
	$(CC) $(MINIBOX_CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/minibox/minibox: $(MINIBOX_OBJS) $(LIBC_SHIM_OBJS) apps/link/minibox.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/minibox.ld -e _start -o $@ \
	    $(MINIBOX_OBJS) $(LIBC_SHIM_OBJS)

minibox: $(APPS_BUILD_DIR)/minibox/minibox

MLIBC_DIR := apps/mlibc
MLIBC_BUILD_DIR := $(BUILD_DIR)/mlibc
MLIBC_SYSROOT := $(abspath $(BUILD_DIR)/mlibc-sysroot)
MLIBC_CROSS := apps/mlibc-robu-cross.ini

mlibc:
	@if [ ! -f $(MLIBC_BUILD_DIR)/build.ninja ]; then \
	    meson setup $(MLIBC_BUILD_DIR) $(MLIBC_DIR) --cross-file $(MLIBC_CROSS) \
	        -Dlibgcc_dependency=false -Ddefault_library=static --prefix=/usr; \
	fi
	ninja -C $(MLIBC_BUILD_DIR)
	DESTDIR=$(MLIBC_SYSROOT) ninja -C $(MLIBC_BUILD_DIR) install

APP_BINS := $(APPS_BUILD_DIR)/procfs/procfs $(APPS_BUILD_DIR)/sysfs/sysfs \
            $(APPS_BUILD_DIR)/hello_initsys/hello_initsys $(APPS_BUILD_DIR)/minibox/minibox \

