
# GPL3+

A09      = ../a09
A09FLAGS = -l $(*F).list

%.bin : %.asm
	$(A09) $(A09FLAGS) -o $@ $<

.PHONY: all clean

all : $(patsubst %.asm,%.bin,$(wildcard *.asm))
clean:
	$(RM) $(shell find . -name '*~')
	$(RM) $(shell find . -name '*.bin')
	$(RM) $(shell find . -name '*.list')

float-decb.bin : override A09FLAGS += -frsdos
warn.bin       : override A09FLAGS += -t
wrap.bin       : override A09FLAGS += -fbasic
