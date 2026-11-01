SHELL		:= /bin/bash
V 		?= 1
SRC		:= src
INC		:= inc
OUT 		?= build
PROJECT		:= SHELL_SCHED_SPLIT_SCREEN_RP2040

TC		:= ../../tools/arm-toolchain/bin/arm-none-eabi
CC 		:= $(TC)-gcc
AS		:= $(TC)-as
LD		:= $(TC)-ld
OD		:= $(TC)-objdump
CP		:= $(TC)-objcopy

C_SRC		:= $(wildcard $(SRC)/*.c)
S_SRC		:= $(wildcard $(SRC)/*.S)
C_OBJ		:= $(C_SRC:.c=.o)
S_OBJ		:= $(S_SRC:.S=.o)
C_OBJ		:= $(addprefix $(OUT)/,$(C_OBJ))
S_OBJ		:= $(addprefix $(OUT)/,$(S_OBJ))

LDTEMPLATE	:= $(SRC)/link.ld
F_OUT		:= $(OUT)/$(PROJECT).elf
F_BIN		:= $(OUT)/$(PROJECT).bin
F_UF2		:= $(OUT)/$(PROJECT).uf2

PICOTOOL	:= ../../tools/picotool/bin/picotool

FLAG_TARGET	:= -mcpu=cortex-m0plus -mthumb
FLAG_C		:= $(FLAG_TARGET) -Wall -Werror -nostdinc -nostdlib -fno-builtin -I $(INC) -O1
FLAG_AS		:= $(FLAG_TARGET)
FLAG_LD		:= -T $(LDTEMPLATE) -nostdlib

LIBGCC		:= $(shell $(CC) $(FLAG_TARGET) -print-libgcc-file-name)

ifeq ($(V),1)
.SILENT:
endif

default: $(F_OUT) $(F_BIN) $(F_UF2)

.SECONDEXPANSION:
$(F_OUT): $(C_OBJ) $(S_OBJ) | $$(@D)/
	@echo linking $(@F)
	$(LD) $^ $(LIBGCC) -o $@ $(FLAG_LD)
	$(OD) $@ -Dx > $(@:.elf=.lst)

$(F_BIN): $(F_OUT)
	@echo creating binary $(@F)
	$(CP) -O binary $< $@

$(F_UF2): $(F_OUT)
	@echo creating UF2 $(@F)
	$(PICOTOOL) uf2 convert $< $@ --family rp2040

flash: $(F_UF2)
	@echo flashing $(F_UF2) to RP2040...
	$(PICOTOOL) load -x $<

$(C_OBJ): $(OUT)/%.o: %.c | $$(@D)/
	@echo compiling $<
	$(CC) -c $(FLAG_C) $< -o $@

$(S_OBJ): $(OUT)/%.o: %.S | $$(@D)/
	@echo assembling $<
	$(CC) -c $(FLAG_C) $< -o $@

%/:
	mkdir -p $@

clean:
	rm -rf $(OUT)

