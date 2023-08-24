
# GPL3+

CC = clang -std=c99 -Weverything
#CC = gcc -std=c99 -pedantic -Wall -Wextra
CFLAGS = -g -D_GNU_SOURCE
LDFLAGS = -g

.PHONY: clean

a09 : a09.o opcodes.o

a09.o : a09.h
opcodes.o : a09.h



clean:
	$(RM) $(shell find . -name '*.o')
	$(RM) $(shell find . -name '*~')
	$(RM) $(shell find . -name '*.out')
	$(RM) a09
