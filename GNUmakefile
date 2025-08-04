#############################################################################
#
#   Structures and function definitions for as09 (6809 assembler)
#   Copyright (C) 2023 Sean Conner
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   Comments, questions and criticisms can be sent to: sean@conman.org
#
#############################################################################

CC      = gcc -std=c99 -pedantic -Wall -Wextra -Wwrite-strings
CFLAGS  = -g
LDFLAGS = -g
LDLIBS  = -lcgi8 -lmc6809 -lm

.PHONY: clean

a09 : a09.o cmdline.o opcodes.o symbol.o expr.o rexpr.o fbin.o frsdos.o fsrec.o fbasic.o fdefault.o reals.o tests.o

a09.o      : a09.h
cmdline.o  : a09.h
opcodes.o  : a09.h
symbol.o   : a09.h
expr.o     : a09.h
rexpr.o    : a09.h
fbin.o     : a09.h
frsdos.o   : a09.h
fsrec.o    : a09.h
fbasic.o   : a09.h
fdefault.o : a09.h
tests.o    : a09.h

clean:
	$(RM) $(shell find . -name '*.o')
	$(RM) $(shell find . -name '*~')
	$(RM) $(shell find . -name '*.obj') $(shell find . -name '*.list')
	$(RM) a09
