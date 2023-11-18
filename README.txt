
                          A Motorola 6809 Assembler

  This is a standard, two-pass assembler that understands (as far as I can
tell) most of the standard pseudo operations that other 6809 assemblers
understand, plus a few others that I find handy.

  Labels are restricted to 63 charaters in length, which I think is larger
than most other 6809 assemblers.  I also support a type of 'local' label
that is easier to show than explain:
			
	random		pshs	x,d
			ldd	#-1
	.smaller_mask	cmpd	,s
			blo	.got_mask
			lsra
			rorb
			bra	.smaller_mask
	.got_mask	lslb
			rolb
			orb	#1
			std	2,s
	.generate	ldd	lfsr
			lsra
			rorb
			bcc	.nofeedback
			eora	#$B4
			eora	#$00
	.nofeedback	std	lfsr
			anda	2,s
			andb	3,s
			cmpd	,s
			bhi	.generate
			leas	4,s
			rts

Labels that start with '.' are considered 'local' to a previous label not
starting with a '.'.  Internally, the assembler will translate the these
labels:

	.smaller_mask
	.got_mask
	.generate
	.nofeedback

to

	random.smaller_mask
	random.got_mask
	random.generate
	random.nofeedback

  The total length of a label, plus a 'local' label, must not exceed 63
chararacters, but I feel that such a feature, along with the generous limit
for label length, can make for more readable code.  One more example:

	clear_bytes	clra
	.loop		sta	,x+
			decb
			bne	.loop
			rts

	clear_words	stb	,-s
			clra
			clrb
	.loop		std	,x++
			dec	,s
			bne	.loop
			rts

  The four labels defined are:

	clear_bytes
	clear_bytes.loop
	clear_words
	clear_words.loop

The two instances of '.loop' do not interfere, nor are considered duplicate
labels, due to the internal expansion.

  Expressions can be prefixed with '>' to force a 16-bit value; with '<' to
force an 8-bit value, or '<<' to force a 5-bit value (useful for the index
addressing mode).  The use of '>' and '<' is standard for other 6809
assemblers, but as far as I know, the use of '<<' is unique to this one. 
An example:

foo     equ     $01

        lda     foo	; knows to use direct (8-bit) address
        lda     >foo	; force extended (16-bit) address
        lda     bar	; used extended, beause of forward reference
        lda     <bar	; force use of direct address
        lda     foo,x	; uses 5-bit offset because value is known 
        lda     <foo,x	; force use of 8-bit offset
        lda     >foo,x	; force use of 16-bit offset
        lda     bar,x	; uses 16-bit offset because of forward reference
        lda     <<bar,x	; force 5-bit offset
        lda     <bar,x	; force 8-bit offset

bar     equ     $02

And here's the generated machine code for the above code:

                       1 | foo     equ     $01
                       2 |
0000: 96    01         3 |         lda     foo
0002: B6    0001       4 |         lda     >foo
0005: B6    0002       5 |         lda     bar
0008: 96    02         6 |         lda     <bar
000A: A6    01         7 |         lda     foo,x
000C: A6    8801       8 |         lda     <foo,x
000F: A6    890001     9 |         lda     >foo,x
0013: A6    890002    10 |         lda     bar,x
0017: A6    02        11 |         lda     <<bar,x
0019: A6    8802      12 |         lda     <bar,x
                      13 |
                      14 | bar     equ     $02

And the warnings the assembler generated from the above code:

	example.a:5: warning: W0005: address could be 8-bits, maybe use '<'?
	example.a:10: warning: W0006: offset could be 5-bits, maybe use '<<'?

(The reason we can't just apply those transformations is due to speed
considerations, and complexity---each such change requires another pass, and
that would lead a more complex assembler.)

  The list of supported pseudo operations---if label "Non-standard", it's a
non-standard pesudo operation for most 6809 assemblers.

	ALIGN expr

		(Non-standard) Align the program counter to a multiple of
		the given expression.  The expression must use defined
		equates (EQU) prior to the statement, or an error will
		occur.

	ASCII 'string'

		(Non-standard) Place the ASCII string into the program. 
		Unlike FCC, this understands the following escape sequences:

			\a	ASCII character BEL (7)
			\b	ASCII character BS  (8)
			\t	ASCII character HT  (9)
			\n	ASCII character LF  (10)
			\v	ASCII character VT  (11)
			\f	ASCII character FF  (12)
			\r	ASCII character CR  (13)
			\e	ASCII character ESC (27)
			\"	ASCII character quotation mark
			\'	ASCII character apostrophe
			\\	ASCII character reversed solidus

		The string can be delimited by the apostrophe or quotation
		mark.

	ASCIIH 'string'

		(Non-standard) Place the ASCII string into the program,
		with the last character's bit-7 set.  This, like ASCII,
		understands escaped characters.

	ASCIIZ 'string'

		(Non-standafd) Place the ASCII string into the program,
		terminated by a NUL ('\0') character.  Like ASCII, this
		supports escaped characters.

	END [label]

		Mark the end of the assembly file, with an optional label.
		This has full support with the 'rsdos' backend.

	label EQU expr

		Set the label to the value of expr.  This value, once set,
		cannot be changed.

	EXTDP label

		(Non-standard) The given label is an external reference to
		the direct page.  This is accepted and the label will be
		"defined" but otherwise, this currently does nothing.

	EXTERN label

		(Non-standard) The given label is an external reference. 
		This is accepted and the label will be "defined" but
		othersise, this current does nothing.

	FCB expr[,expr...]

		Form Contant Byte

	FCC /string/

		Place the ASCII string, delimited by the first non-space
		character, into the program.

	FCS /string/

		(Standard for OS-9) Place the ASCII string, delimited by the
		first non-space character, into the program.  The last
		character in the string has bit-7 set to indicate the end
		of the string.

	FDB expr[,expr...]

		Form Double Byte

	INCBIN "filename"

		(Non-standard) Include verbatim the given file into the
		program.  No interpretation of the contents is done on the
		input file.

	INCLUDE "filename"

		(Non-standard) Open and assemble, at the current location,
		the given filename.

	ORG expr

		Start the assembly process at the address specified by expr.
		If not specified, assembly starts at address 0.

	PUBLIC label

		(Non-standard) Mark a label as public.  This is accepted,
		but not currently used.

	RMB expr

		Reserve expr bytes in the program.

	label SET expr

		Set the label to the value of expr.  Unlike EQU, the label
		can be reassigned a new value later in the code.

	SETDP expr

		Tells the assembler where the DP (direct page) is in memory.

  The following command line options are supported:

	-n Wxxxx[,Wyyyy...]

		Supress warnings from the assembler.  This option can be
		specified multiple times, and multiple warnings can be
		specified per option.  See the file 'Errors.txt' for the
		warning tags to use.

	-o filename

		Specify the output file name.  Defaults to 'a09.obj'.

	-l listfile

		Specify the listing file.  If not given, no listing file
		will be generated.

	-f format

		Specify the output format.  Two formats are currently
		supported:

			bin	- binary output
			rsdos	- executable format for Coco BASIC

	-d

		Reserved for debug output.

	-M

		This will generate a list of dependencies appropriate for
		make.

	-h

		Output a summary of the options supported.

