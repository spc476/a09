
;***************************************************************************
; Example of .FLOAT and .FLOATD directives for IEEE-754.
; GPL3+ Copyright 2024 by Sean Conner.
;***************************************************************************

		org	$1000

	;------------------------------------------
	; Single precision floating point numbers
	;------------------------------------------

		.float	1e38
		.float	4e10
		.float	2e10
		.float	1e10
		.float	1
		.float	0.5
		.float	0.25
		.float	1e-4
		.float	1e-37
		.float	1e-38
		.float	1e-39
		.float	0
		.float	-1
		.float	-10

		.float	-((2 * 3.1415926535) ** 11) / 11!
		.float	 ((2 * 3.1415926535) **  9) /  9!
		.float	-((2 * 3.1415926535) **  7) /  7!
		.float	 ((2 * 3.1415926535) **  5) /  5!
		.float	-((2 * 3.1415926535) **  3) /  3!
		.float	   2 * 3.1415926535

		.float	99999999.93
		.float	999999999.3
		.float	1e9

		.float	-1/23
		.float	 1/21
		.float	-1/19
		.float	 1/17
		.float	-1/15
		.float	 1/13
		.float	-1/11
		.float	 1/ 9
		.float	-1/ 7
		.float	 1/ 5
		.float	-1/ 3

		.float	(2 / 7) * (1 / 0.69314718065)
		.float	(2 / 5) * (1 / 0.69314718065)
		.float	(2 / 3) * (1 / 0.69314718065)
		.float	(2 / 1) * (1 / 0.69314718065)
		.float	1 / 2**0.5
		.float	2**0.5
		.float	-0.5
		.float	0.69314718065

		.float	1.442695041
		.float	1 / (7! * 1.442695041**7)
		.float	1 / (6! * 1.442695041**6)
		.float	1 / (5! * 1.442695041**5)
		.float	1 / (4! * 1.442695041**4)
		.float	1 / (3! * 1.442695041**3)
		.float	1 / (2! * 1.442695041**2)
		.float	1 / (1! * 1.442695041**1)

	;-----------------------------------------
	; Double precicion floating point numbers
	;-----------------------------------------

		.floatd	1e38
		.floatd	4e10
		.floatd	2e10
		.floatd	1e10
		.floatd	1
		.floatd	0.5
		.floatd	0.25
		.floatd	1e-4
		.floatd	1e-37
		.floatd	1e-38
		.floatd	1e-39
		.floatd	0
		.floatd	-1
		.floatd	-10

		.floatd	-((2 * 3.1415926535) ** 11) / 11!
		.floatd	 ((2 * 3.1415926535) **  9) /  9!
		.floatd	-((2 * 3.1415926535) **  7) /  7!
		.floatd	 ((2 * 3.1415926535) **  5) /  5!
		.floatd	-((2 * 3.1415926535) **  3) /  3!
		.floatd	   2 * 3.1415926535

		.floatd	99999999.93
		.floatd	999999999.3
		.floatd	1e9

		.floatd	-1/23
		.floatd	 1/21
		.floatd	-1/19
		.floatd	 1/17
		.floatd	-1/15
		.floatd	 1/13
		.floatd	-1/11
		.floatd	 1/ 9
		.floatd	-1/ 7
		.floatd	 1/ 5
		.floatd	-1/ 3

		.floatd	(2 / 7) *(1 / 0.69314718065)
		.floatd	(2 / 5) *(1 / 0.69314718065)
		.floatd	(2 / 3) *(1 / 0.69314718065)
		.floatd	(2 / 1) *(1 / 0.69314718065)
		.floatd	1 / 2**0.5
		.floatd	2**0.5
		.floatd	-0.5
		.floatd	0.69314718065

		.floatd	1.442695041
		.floatd	1 / (7! * 1.442695041**7)
		.floatd	1 / (6! * 1.442695041**6)
		.floatd	1 / (5! * 1.442695041**5)
		.floatd	1 / (4! * 1.442695041**4)
		.floatd	1 / (3! * 1.442695041**3)
		.floatd	1 / (2! * 1.442695041**2)
		.floatd	1 / (1! * 1.442695041**1)

		end
