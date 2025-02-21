U=.
LD_SCRIPT := $U/user.ld
OUT_DIR := out


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

CFLAGS = -Wall -O0 -Werror -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-common -nostdlib -ffunction-sections -fdata-sections
CFLAGS += -ffreestanding -nostdlib -nostdinc -I../include
LDFLAGS = -z max-page-size=4096

ASFLAGS = -I. -I ../include

default: all

# OBJS 定义
OBJS = \
	$U/sh \
	$U/cat \

# usys.S 文件生成规则
$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

# 编译 usys.o 文件
$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR):
	@mkdir -p $@



# 生成每个目标的可执行文件
$(OBJS): $U/usys.o | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $(OUT_DIR)/$@.o $@.c
	$(LD) -T $(LD_SCRIPT) -o $(OUT_DIR)/$@ $(OUT_DIR)/$@.o $^
	$(OBJDUMP) -d $(OUT_DIR)/$@ > $(OUT_DIR)/$@.asm
	@$(OBJCOPY) -S -O binary $(OUT_DIR)/$@ $(OUT_DIR)/$@
	@od -An -t x1 -v $(OUT_DIR)/$@ | awk '{for(i=1;i<=NF;i++) printf "0x%s, ", $$i; print ""}' > $(OUT_DIR)/$@.bin	

	
# 全部目标
all: $(OBJS)
	@rm -f *.o

clean:
	@rm -rf *.tex *.dvi *.idx *.aux *.log *.ind *.ilg *.o *.bin \
	usys.S $(OBJS) \
	out
