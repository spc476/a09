
# GPL3+

AS09 = ../a09
AS09FLAGS = -l $(*F).list

%.bin : %.asm
	$(AS09) $(AS09FLAGS) -o $@ $<

.PHONY: all clean

all : $(patsubst %.asm,%.bin,$(wildcard *.asm))
clean:
	$(RM) $(shell find . -name '*~')
	$(RM) $(shell find . -name '*.bin')
	$(RM) $(shell find . -name '*.list')

float-decb.bin : override AS09FLAGS += -frsdos
warn.bin : override AS09FLAGS += -ftest
