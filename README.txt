
                          A Motorola 6809 Assembler

  This is a standard, two-pass assembler that understands (as far as I can
tell) most of the standard pseudo operations that other 6809 assemblers
understand, plus a few others that I find handy.

NOTE: You will need to downlaod and install the following libraries to use
the program:

	https://github.com/spc476/CGILib
	https://github.com/spc476/mc6809

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

  Numbers can be specified in decimal, octal (leading '&'), binary (leading
'%') and hexadecimal (leading '$').  The use of an underscore ('_') within
numbers can be used to separate groups.  Some examples:

	122		decimal
	12_123		decimal
	%10101010	binary
	%10_101_110	binary
	&34		octal
	&23_44		octal
	$FF		hexadecimal
	$F_ff_3		hexadecimal

  There is also a special form of value for use with the ANDCC, ORCC and
CWAI instructions.  Instead of an immediate value, like:

	ORCC	#$50	; disable interrupts
	ANDCC	#$AF	; enable interrupts
	CWAI	#$AF	; enable interrupts and wait
	ORCC	#$01	; set carry flag
	ANDCC	#$FE	; reset carry flag

you can specify a "flag set", as:

	ORCC	{FI}
	ANDCC	{FI}
	CWAI	{FI}
	ORCC	{C}
	ANDCC	{C}

This form will construct the appropriate values for the instructions.  The
flags are:

	C	carry
	V	overflow
	Z	zero
	N	negative
	I	interrupt
	H	half-carry
	F	fast interrupt
	E	entire state

  The list of supported pseudo operations---if label "Non-standard", it's a
non-standard pesudo operation for most 6809 assemblers.

	.ASSERT expr [, "explanation" ]

		Assert a condition when using the test backend, otherwise
		ignored.  If the expression is true, nothing happens; if the
		expression is false, the test fails, a dianostic message is
		printed, and the assembly procedure stops.  This can appear
		outside of a unit test.

	.ENDTST

		End a unit test; ignored by other backends.

	.NOTEST

		All text up to a .ENDTST directive is ignored.  This is
		an easy way to disable a test when using the test backend.

	.OPT backend option data...

		Supply an option from within the source code instead of the
		command line.  The following options are always available:

			.OPT * DISABLE <warning>

				Disable the given warning (see list below).
				Note that W0002, W0014, W0015 and W0016
				cannot be disabled with this directive given
				the nature of when they happen.  W0002 can
				be disabled with the ".OPT * USES <label>"
				directive; the others only happen when using
				the test backend.

			.OPT * ENABLE <warning>

				Enable a given warning.  Note that upon
				program startup, all warnings are enabled by
				default.  This is typically used to
				re-enable a warning after being disabled.

			.OPT * USES <label>

				Mark a label as being used.  This is used to
				supress W0002 warnings.

		The following options are only used by the TEST backend.
		They are otherwise ignored by other backends.  The following
		options can appear inside or outside a .TEST directive
		unless otherwise specified.  If they appear outside, it
		applies to all tests; otherwise it only applies to the test
		they appear in.

			.OPT TEST MEMB address,byte

				Write the byte value to the address in the
				virtual memory for the 6809 emulator.

			.OPT TEST MEMW address,word

				Write the word (16-bit value) to the address
				in the virtual memory for the 6809 emulator.

			.OPT TEST PROT prot,address[,end-address]

				Enable memory permissions for the given
				address(es).  The permissions allowed are:

					r	allow reads
					w	allow writes
					x	allow execution
					t	trace execution and writes
					n	remove any protections


				The first four can be all be used; 'n' is
				used to remove any existing protections on a
				memory location.

					.OPT TEST PROT r,$400	; ro of $400
					.OPT TEST PROT rw,foo	; rw @ foo
					.OPT TEST PROT n,$400	; no access
					.OPT TEST PROT rxt,run

			.OPT TEST STACK address

				Set the default stack address for tests.
				This can ONLY appear outside a .TEST
				directive, given the nature of the
				assembler.

	.TEST ["name"]

		Define a unit test if using the test backend.  Any 6809 code
		is executed at the end of pass 2 of the assembler, and must
		end with a 'RTS' instruction.  All .ASSERT directives in the
		code being executed will be run.  This, and all following
		text until a .ENDTST directive, will be ignored by other
		backends.

	.TROFF

		Turn off 6809 program tracing if using the test backend. 
		Ignored by other backends.  Like the .ASSERT directive, this
		can appear outside a unit test definition.

	.TRON [timing]

		Turn on 6809 program tracing if using the test backend. 
		Each 6809 instruction is printed on stdout and includes the
		contents of the registers at that point in execution, and
		can appear outside of a unit test definition.  This is
		ignored by other backends.

		If the "timing" option is used, the code will be timed, not
		traced.  At the corresponding .TROFF, the number of CPU
		cycles will be reported.

	ALIGN expr

		(Non-standard) Align the program counter to a multiple of
		the given expression.  The expression must use defined
		equates (EQU) prior to the statement, or an error will
		occur.

	ASCII 'string'
	ASCII 'string'c
	ASCII 'string'h
	ASCII 'string'z

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
		mark.  The suffix of 'C', 'H' or 'Z' can be used:

			'C'	Make a counted string, where the first
				byte is the length of the rest of the
				string.

			'H'	The last character of the string has bit
				7 set to mark the end of the string.

			'Z'	A NUL byte is appended to the string to mark
				the end of the string.

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

  Warnings are printed for conditions that aren't exactly errors, but can
be potential problems.  The defined warnings are:

	W0001

		The label exceeds 63 characters and is thus, truncated
		internally.

	W0002

	        The label wasn't referenced by any other code.  And if the
	        label is not referenced, why have the label in the first
	        place?  It could also mean an unused variable whose removal
	        could save some space.

	W0003

		A value that is outside the range of -16 to 15 is being
		forced to a 5-bit range by the use of '<<'.  

	W0004

		A value that is outside the range of -127 to 255 is being
		forced to an 8-bit range by the use of '<'.

	W0005

		An address value can use the direct addressing mode but
		is using the extended addressing mode.  The use '<' can
		be used to force a direct addressing mode.

	W0006

		The default value of an index offset can fit in an 8-bit
		range and the use of '<' might be warranted.

	W0007

		The default value of an index offset can fit in a 5-bit
		range and the use of '<<' might be warranted.

	W0008

		The mixing of 8-bit and 16-bit registers in an EXG or TFR
		instruction was found.  This is undefined by Motorola, but
		is a warning instead of an error because of code in the wild
		that might rely upon implementation behavior.

	W0009

		The offset for a 16-bit branch instruction is inside the
		range for an 8-bit branch instruction.

	W0010

		A local label was found before a non-local label was used. 
		The resulting label will be used, but there is no way to
		reference it when the next non-local label is defined. 
		Perhaps this could be an error.

	W0011

		The 6809 indirect indexing addressing mode does not support
		5-bit offsets, and therefore, an 8-bit offset is being used.

	W0012

		A branch instruction other than BRN (or LBRN) is pointing to
		the next instruction in the program.

	W0013

		A label named 'A', 'B' or 'D' was used.  This could be an
		issue for the index addressing mode where the A, B or D
		register can be used as an index.

	W0014

		Self-modifying code was possibly detected.  This is only
		issued if using the test backend, and even then, only if
		the code with the potential problem is actually run.

	W0015

		A failed unit test.  This is only issued if using the test
		backend.

	W0016

		A write to memory that appears with a .TRON and .TROFF
		directives.  This is only issued if using the test backend.

	W0017

		The .OPT TEST STACK <address> directive can only appear
		outside a .TEST directive.

  Individual warnings can be supressed by using the appropritate command
line option.

  There are four possible backends (or formats) the assembler supports. 
They are:

	bin	binary backend

		The resulting output is a memory image.

	rsdos	Radio Shack TRS-80 Color Computer format

		The resulting output can be loaded by Color Basic using
		the CLOADM or LOADM BASIC command.

	srec	Motorola S-Record output

		A text format.

	test	The unit test backend.

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
			srec	- Motorola SREC format
			test	- unit test backend

	-d

		Reserved for debug output.

	-M

		This will generate a list of dependencies appropriate for
		make.

	-h

		Output a summary of the options supported.

  Individual backends can have their own command line options that are
activated after the '-f' option.  They are:

The SREC backend

	-R size

		This specifies the number of data bytes per line.  Valid
		values are 1 to 252, with 34 being the default.

	-0 file

		Use the given file to generate an S0 record.  Since there's
		no standard format for the S0 record, this allows you to use
		whatever format is required for your use.

	-L address

		Use the given address for the loading address if no ORG
		directive appears in the source code.

	-E address

		Use the given address for the execute address if no END
		directive appears in the source code.

	-O

		Force the use of the -L and -E options to override any
		ORG or END directives that appear in the source code.

The test backend

	-S address

		Use the address for the system stack and string pool for
		tests in the emulated 6809.  The default value for this is
		$FFF0.

	-F byte

		Use the given value to fill the 6809 memory before running
		the tests.  The default value is 1 (an illegal instruction).

	-R lowaddress[-highaddress]

		Mark the memory of the emulated 6809 as read-only.

	-W lowaddress[-highaddress]

		Mark the memory of the emulated 6809 as write-only.

	-E lowaddress[-highaddress]

		Mark the memory of the emulated 6809 as allowing execution.

	-T lowaddress[-highaddress]

		Mark the memory of the emulated 6809 as being traced.  For
		instructions, this will print the instruction and do a
		register dump.  For memory writes, this will print out the
		address and the new value.

	-D file

		Write the 6809 memory to the given file at the end of
		assembly and all tests have run.
