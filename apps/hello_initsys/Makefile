ARCH ?= x86_64
TARGET := helloinit
BUILD_SYS ?= clang

.PHONY: all clean

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

ROOT := ../..
LIBC_DIR := ../libc
BUILD_DIR := build

CFLAGS := --target=$(ARCH)-elf -ffreestanding -O2 -fno-pic -fno-pie -fno-stack-protector \
          -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra \
          -I$(ROOT)/include -I$(ROOT)/arch/$(ARCH)/include -I$(LIBC_DIR)/include

LDFLAGS := -nostdlib -static -z noexecstack -T $(ROOT)/apps/link/hello_initsys.ld -e _start

LIBC_SRCS := $(wildcard $(LIBC_DIR)/src/*.c)
LIBC_OBJS := $(patsubst $(LIBC_DIR)/src/%.c,$(BUILD_DIR)/libc/%.c.o,$(LIBC_SRCS))
LIBC_OBJS += $(BUILD_DIR)/libc/setjmp.S.o

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/main.c.o $(LIBC_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/main.c.o $(LIBC_OBJS)

$(BUILD_DIR)/main.c.o: main.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc/%.c.o: $(LIBC_DIR)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc/setjmp.S.o: $(LIBC_DIR)/src/setjmp.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)