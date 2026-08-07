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

.PHONY: all clean run run-serial shell test

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
	$(CC) $(CFLAG) -c $< -o $@

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


$(APPS_BUILD_DIR)/ramfstest/ramfstest: $(APPS_BUILD_DIR)/ramfstest/ramfstest.c.o $(LIBC_SHIM_OBJS) $(APP_COMMON_OBJ) apps/link/ramfstest.ld
	$(LD) $(APP_LDFLAGS) --gc-sections -T apps/link/ramfstest.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/ramfstest/ramfstest.c.o $(LIBC_SHIM_OBJS) $(APP_COMMON_OBJ)

APP_BINS := $(APPS_BUILD_DIR)/toybox_true/true $(APPS_BUILD_DIR)/toybox_false/false \
            $(APPS_BUILD_DIR)/procfs/procfs $(APPS_BUILD_DIR)/sysfs/sysfs \
