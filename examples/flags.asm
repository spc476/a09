
;***************************************************************************
; Example of using the CC flags notation.
; GPL3+ Copyright (C) 2024 by Sean Conner.
;***************************************************************************

		org	$1000

	;------------------------------------------------------------------
	; Starting with the new notation.  The new notation only works for
	; the following instructions.
	;------------------------------------------------------------------

start		orcc	{cvznihfe}
		andcc	{cvznihfe}
		andcc	{c}
		orcc	{c}
		orcc	{CVZN}
		andcc	{ZNVC}

		orcc	{fi}
		andcc	{fi}
		cwai	{fi}

	;------------------------------------
	; And with the traditional notation
	;------------------------------------

		orcc	#$FF	; set all flags
		andcc	#0	; reset all flags
		andcc	#$FE	; reset the carry bit
		orcc	#1	; set the carry bit
		orcc	#$0F	; set the arith bits
		andcc	#$F0	; reset the arith bits

		orcc	#$50	; disable interrupts
		andcc	#$AF	; enable interrupts
		cwai	#$AF	; enable interrupts and wait

		end	start
