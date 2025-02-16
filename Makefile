# 变量定义
BUILD_DIR := ./build
SRC_DIR := .
BOOT := ./boot
KERNEL := Kernel

# 查找源文件，排除 tools 目录
SRCS_C := $(shell find $(SRC_DIR) -type f -name '*.c' -not -path "$(SRC_DIR)/tools/*")
SRCS_S := $(shell find $(SRC_DIR) -type f -name '*.S' -not -path "$(SRC_DIR)/tools/*")

# 目标文件列表
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS_C))
OBJS += $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(SRCS_S))

# 依赖文件列表
# DEPS := $(OBJS:.o=.d)
# -include $(DEPS)

# 目录列表
DIRS := $(sort $(dir $(OBJS)))

# 尝试自动推断正确的工具链前缀
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'make TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

CC = $(TOOLPREFIX)gcc
GDB = $(TOOLPREFIX)gdb
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -O -Werror -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-common -nostdlib
CFLAGS += -ffreestanding -nostdlib -nostdinc -I./include
# CFLAGS += -MMD -MP
LDFLAGS = -z max-page-size=4096


ASFLAGS =  -I. -I./include

# 请确保这里与 include/param.h NCPU 一致，至少 CPUS >= NCPU
# 少的话会导致我们在添加任务时候是轮转添加的，没有CPU编号，任务无法启动
CPUS := 4

QEMU = qemu-system-riscv64
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
QEMUOPTS = -machine virt -bios none -kernel $(BUILD_DIR)/$(KERNEL) -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -serial mon:stdio
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=virtio_disk.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
# QEMUOPTS += -device pci-bridge,id=virtio-mmio-bus.0,chassis=1

QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
        then echo "-gdb tcp::$(GDBPORT)"; \
        else echo "-s -p $(GDBPORT)"; fi)

# 默认目标
default: $(BUILD_DIR)/$(KERNEL)

# 创建必要目录
$(DIRS):
	@mkdir -p $@

# 编译
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(DIRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(DIRS)
	$(CC) $(CFLAGS) -c -o $@ $<

# 链接内核
$(BUILD_DIR)/$(KERNEL): $(DIRS) $(OBJS) $(BOOT)/kernel.ld
	$(LD) $(LDFLAGS) -T $(BOOT)/kernel.ld -o $@ $(OBJS)
	$(OBJDUMP) -j .text -S $@ > $(BUILD_DIR)/kernel.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD_DIR)/kernel.sym

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# 启动内核
qemu: $(BUILD_DIR)/$(KERNEL)
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(BUILD_DIR)/$(KERNEL)
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

gdb: $(BUILD_DIR)/$(KERNEL)
	$(GDB) $(BUILD_DIR)/$(KERNEL) -q -ex "target remote :$(GDBPORT)" -ex "layout split"

fs: virtio_disk.img
	hexdump -C virtio_disk.img | less

mkfs:
	cd tools && make clean && make

