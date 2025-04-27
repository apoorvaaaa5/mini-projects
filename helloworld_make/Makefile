SHELL		:= /bin/bash
V 		?= 1
SRC		:= src
OUT 		?= out
PROJECT		:= Helloworld

TC		:= ../tools/bin/riscv64-unknown-elf
CC 		:= $(TC)-gcc
AS		:= $(TC)-as	#fixed names
LD		:= $(TC)-ld
OD		:= $(TC)-objdump
C_SRC		:= $(wildcard $(SRC)/*.c)
S_SRC		:= $(wildcard $(SRC)/*.S)
C_OBJ		:= $(C_SRC:.c=.o)
S_OBJ		:= $(S_SRC:.S=.o)
C_OBJ		:= $(addprefix $(OUT)/,$(C_OBJ))
S_OBJ		:= $(addprefix $(OUT)/,$(S_OBJ))

LDTEMPLATE	:= $(SRC)/link.ld
F_OUT		:= $(OUT)/$(PROJECT).elf

FLAG_TARGET	:= -march=rv32i -mabi=ilp32
FLAG_C		:= $(FLAG_TARGET) -Wall -Werror -nostdinc -nostdlib -fno-builtin
FLAG_AS		:= $(FLAG_TARGET)
FLAG_LD		:= -dT $(LDTEMPLATE) -b elf32-littleriscv -nostdlib

ifeq ($(V),1)
.SILENT:
endif

default: $(F_OUT)

.SECONDEXPANSION:
$(F_OUT): $(C_OBJ) $(S_OBJ) | $$(@D)/
	@echo linking $(@F)
	$(LD) $^ -o $@ $(FLAG_LD)
	$(OD) $@ -Dx > $(@:.elf=.lst)

$(C_OBJ): $(OUT)/%.o: %.c | $$(@D)/
	@echo compiling $<
	$(CC) -c $(FLAG_C) $< -o $@

$(S_OBJ): $(OUT)/%.o: %.S | $$(@D)/
	@echo assembling $<
	$(AS) $(FLAG_AS) $< -o $@

%/:
	mkdir -p $@

clean:
	rm -rf $(OUT)


