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

APP_COMMON_OBJ := $(APPS_BUILD_DIR)/common/minilibc.c.o

$(APP_COMMON_OBJ): apps/common/minilibc.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/service/hello_service.c.o: apps/service/hello_service.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/service/hello_service: $(APPS_BUILD_DIR)/service/hello_service.c.o $(APP_COMMON_OBJ) apps/link/service.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/service.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/service/hello_service.c.o $(APP_COMMON_OBJ)

$(APPS_BUILD_DIR)/task/hello_task.c.o: apps/task/hello_task.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/task/hello_task: $(APPS_BUILD_DIR)/task/hello_task.c.o $(APP_COMMON_OBJ) apps/link/task.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/task.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/task/hello_task.c.o $(APP_COMMON_OBJ)

$(APPS_BUILD_DIR)/root_task/root_task.c.o: apps/root_task/root_task.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/root_task/child_payload.S.o: apps/root_task/child_payload.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/root_task/root_task: $(APPS_BUILD_DIR)/root_task/root_task.c.o \
                                            $(APPS_BUILD_DIR)/root_task/child_payload.S.o \
                                            $(APP_COMMON_OBJ) apps/link/root.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/root.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/root_task/root_task.c.o \
	    $(APPS_BUILD_DIR)/root_task/child_payload.S.o $(APP_COMMON_OBJ)

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
$(APPS_BUILD_DIR)/benchserver/benchserver.c.o: apps/benchserver/benchserver.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/benchserver/benchserver: $(APPS_BUILD_DIR)/benchserver/benchserver.c.o $(APP_COMMON_OBJ) apps/link/benchserver.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/benchserver.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/benchserver/benchserver.c.o $(APP_COMMON_OBJ)

$(APPS_BUILD_DIR)/benchclient/benchclient.c.o: apps/benchclient/benchclient.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/benchclient/benchclient: $(APPS_BUILD_DIR)/benchclient/benchclient.c.o $(APP_COMMON_OBJ) apps/link/benchclient.ld
	$(LD) $(APP_LDFLAGS) -T apps/link/benchclient.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/benchclient/benchclient.c.o $(APP_COMMON_OBJ)

TOYBOX_LIBC_SRCS := $(wildcard apps/libc/src/*.c) $(wildcard apps/libc/src/*.S) \
                     $(wildcard apps/libc/include/*.h) $(wildcard apps/libc/include/*/*.h)

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
$(APPS_BUILD_DIR)/spawntest/spawntest.c.o: apps/spawntest/spawntest.c
	@mkdir -p $(@D)
	$(CC) $(LIBC_SHIM_CFLAGS) -c $< -o $@

$(APPS_BUILD_DIR)/spawntest/spawntest: $(APPS_BUILD_DIR)/spawntest/spawntest.c.o $(LIBC_SHIM_OBJS) $(APP_COMMON_OBJ) apps/link/spawntest.ld
	$(LD) $(APP_LDFLAGS) --gc-sections -T apps/link/spawntest.ld -e _start -o $@ \
	    $(APPS_BUILD_DIR)/spawntest/spawntest.c.o $(LIBC_SHIM_OBJS) $(APP_COMMON_OBJ)


APP_BINS := $(APPS_BUILD_DIR)/root_task/root_task $(APPS_BUILD_DIR)/devfs/devfs \
            $(APPS_BUILD_DIR)/benchserver/benchserver $(APPS_BUILD_DIR)/benchclient/benchclient \
            $(APPS_BUILD_DIR)/toybox_true/true $(APPS_BUILD_DIR)/toybox_false/false \
            $(APPS_BUILD_DIR)/procfs/procfs $(APPS_BUILD_DIR)/sysfs/sysfs \
            $(APPS_BUILD_DIR)/toybox_pwd/pwd $(APPS_BUILD_DIR)/toybox_touch/touch \
            $(APPS_BUILD_DIR)/toybox_cat/cat $(APPS_BUILD_DIR)/toybox_tail/tail \
            $(APPS_BUILD_DIR)/toybox_file/file $(APPS_BUILD_DIR)/toybox_find/find \
            $(APPS_BUILD_DIR)/toybox_cp/cp $(APPS_BUILD_DIR)/toybox_mv/mv \
            $(APPS_BUILD_DIR)/toybox_ls/ls \
            $(APPS_BUILD_DIR)/toybox_sh/sh $(APPS_BUILD_DIR)/toybox_echo/echo

$(BUILD_DIR)/rootfs.tar: $(APP_BINS)
	@mkdir -p $(@D)
	COPYFILE_DISABLE=1 tar -cf $@ --format ustar \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/devfs devfs \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/benchserver benchserver \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/benchclient benchclient \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_true true \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_false false \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/ramfs ramfs \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/procfs procfs \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/sysfs sysfs \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_pwd pwd \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_touch touch \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_cat cat \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_tail tail \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_file file \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_find find \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_cp cp \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_mv mv \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_ls ls \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_sh sh \
	    -C $(CURDIR)/$(APPS_BUILD_DIR)/toybox_echo echo

clean:
	rm -rf $(BUILD_DIR)
QEMU_BOOT_ARGS := -initrd $(BUILD_DIR)/rootfs.tar \
                  -append "apps=hello_service,hello_task root=root_task" \
                  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
                  -cpu qemu64,+rdrand

QEMU_SMP ?= 2

run: all $(BUILD_DIR)/rootfs.tar
	qemu-system-x86_64 -kernel $(BUILD_DIR)/$(TARGET) -smp $(QEMU_SMP) $(QEMU_BOOT_ARGS)

run-serial: all $(BUILD_DIR)/rootfs.tar
	qemu-system-x86_64 -kernel $(BUILD_DIR)/$(TARGET) -smp $(QEMU_SMP) -nographic $(QEMU_BOOT_ARGS)

shell: all $(BUILD_DIR)/rootfs.tar
	qemu-system-x86_64 -kernel $(BUILD_DIR)/$(TARGET) -smp $(QEMU_SMP) -nographic \
	    -initrd $(BUILD_DIR)/rootfs.tar \
	    -append "apps=hello_service,hello_task root=root_task quiet=1" \
	    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
	    -cpu qemu64,+rdrand

bench: all $(BUILD_DIR)/rootfs.tar
	@ARCH=$(ARCH) scripts/bench.sh
