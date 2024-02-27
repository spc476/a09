
;***************************************************************************
; Example of .FLOAT and .FLOATD directives for [Disk Extended] Color BASIC.
; GPL3+ Copyright 2024 by Sean Conner.
;
; All values come from _Color Basic Unravelled_ and _Extended Color Basic
; Unravelled_, both by Walter K.  Zydhek.  Color BASIC and Extended Color
; BASIC are written by Microsoft, Corp.
;
; The comments for each line designate the bytes of the result in their
; respective ROMs.  The values of each .FLOAT directive generate the proper
; byte values.
;
; The values after the second semicolon are the values listed in the
; respective _Unravelled_ books and appear to be wildly optimistic in their
; values.  Some of this might be wrong values (in the lower bits) as
; calculated by Microsoft and then used in the expressions (possible
; speculation; I haven't actually done the calculations to prove this) or
; just an educated guess by W. K. Zydhek.
;
; NOTE: This should be assembled using the '-frsdos' option.
;***************************************************************************
 
		org	$1000

	;-------------------------------------------------------------------
	; These examples are directly from _Color Basic Unravelled_, pg 12. 
	; The larger values don't match exactly in the lower few bits, with
	; the sole exception of 1e-39 which is so far off base that I need
	; to look into it.
	;-------------------------------------------------------------------

		.float	1e38	; $FF $16 $76 $99 $52 !  FF167699 50
		.float	4e10	; $A4 $15 $02 $F9 $00
		.float	2e10	; $A3 $15 $02 $F9 $00
		.float	1e10	; $A2 $15 $02 $F9 $00
		.float	1	; $81 $00 $00 $00 $00
		.float	0.5	; $80 $00 $00 $00 $00
		.float	0.25	; $7F $00 $00 $00 $00
		.float	1e-4	; $73 $51 $B7 $59 $59
		.float	1e-37	; $06 $08 $1C $14 $14 !  06081C EA 14
		.float	1e-38	; $02 $59 $C7 $EE $EE !  0259C7 DCED
		.float	1e-39	; $00 $02 $00 $00 $00 !! FF2E397D8A
		.float	0	; $00 $00 $00 $00 $00
		.float	-1	; $81 $80 $00 $00 $00
		.float	-10	; $84 $A0 $00 $00 $00

	;-------------------------------------------------------------------
	; Table @ $BFC7 (CB 1.2)
	;-------------------------------------------------------------------

		.float	-14.381390673	; $84 $E6 $1A $2D $1B ; -((2 * 3.1415926535) ** 11) / 11!
		.float	 42.007797130	; $86 $28 $07 $FB $F8 ;  ((2 * 3.1415926535) **  9) /  9!
		.float	-76.704170270	; $87 $99 $68 $89 $01 ; -((2 * 3.1415926535) **  7) /  7!
		.float	 81.605223700	; $87 $23 $35 $DF $E1 ;  ((2 * 3.1415926535) **  5) /  5!
		.float	-41.341702110	; $86 $A5 $5D $E7 $28 ; -((2 * 3.1415926535) **  3) /  3!
		.float	  6.283185307	; $83 $49 $0F $DA$A2  ;    2 * 3.1415926535

	;----------------------------------
	; Table @ $B0B6 (CB 1.2)
	;----------------------------------

		.float	99999999.93	; $98 $3E $BC $1F $FD @ B0B6
		.float	999999999.3	; $9E $6E $6B $27 $FD @ B0BB
		.float	1e9		; $9E $6E $6B $28 $00 @ B0C0

	;--------------------------------------------------------------
	; Table @ $83E0 (ECB 1.1)
	;
	; NOTE: the decimal value is correctly listed in the book, however,
	; the following expression doesn't apprear to be correct.
	;-------------------------------------------------------------

		.float	-0.000684793912 ; $76 $B3 $83 $BD $D3 ; -1/23
		.float	+0.004850942157	; $79 $1E $F4 $A6 $F5 ;  1/21
		.float	-0.016111701850	; $7B $83 $FC $B0 $10 ; -1/19
		.float	+0.034209638050	; $7C $0C $1F $67 $CA ;  1/17
		.float	-0.054279132770	; $7C $DE $53 $CB $C1 ; -1/15
		.float	+0.072457196550	; $7D $14 $64 $70 $4C ;  1/13
		.float	-0.089802395400	; $7D $B7 $EA $51 $7A ; -1/11
		.float	+0.110932413450	; $7D $63 $30 $88 $7E ;  1/ 9
		.float	-0.142839807700	; $7E $92 $44 $99 $3A ; -1/ 7
		.float	+0.199999120500	; $7E $4C $CC $91 $C7 ;  1/ 5
		.float	-0.333333315700	; $7F $AA $AA $AA $13 ; -1/ 3

	;---------------------------------
	; Table @ $841D (ECB 1.1)
	;
	; NOTE: The LN2 constant by Microsoft is incorrect---it should be
	; 0.693147180_55, not _65 as listed.
	;---------------------------------

		.float	0.4342559420	; $7F $5E $56 $CB $79 ; (2/7)*(1/LN2)
		.float	0.5765845413	; $80 $13 $9B $0B $64 ; (2/5)*(1/LN2)
		.float	0.9618007593	; $80 $76 $38 $93 $16 ; (2/3)*(1/LN2)
		.float	2.8853900735	; $82 $38 $AA $3B $20 ; (2/1)*(1/LN2)
		.float	1 / 2**0.5	; $80 $35 $04 $F3 $34 ; 1 / SQRT(2)
		.float	2**0.5		; $81 $35 $04 $F3 $34 ; SQRT(2)
		.float	-0.5		; $80 $80 $00 $00 $00 ; 
		.float	0.69314718065   ; $80 $31 $72 $17 $F8 ; LN2

	;---------------------------------
	; Table starts at $84C9 (ECB 1.1)
	;---------------------------------

		.float	1.442695041000000 ; $81 $38 $AA $3B $29 ; (CF)
		.float	0.000021498763705 ; $71 $34 $58 $3E $56 ; 1/(7! * CF**7)
		.float	0.000143523140400 ; $74 $16 $7E $B3 $1B ; 1/(6! * CF**6)
		.float	0.001342263482500 ; $77 $2F $EE $E3 $85 ; 1/(5! * CF**5)
		.float	0.009614017015000 ; $7A $1D $84 $1C $2A ; 1/(4! * CF**4)
		.float	0.055505126870000 ; $7C $63 $59 $58 $0A ; 1/(3! * CF**3)
		.float	0.240226384650000 ; $7E $75 $FD $E7 $C6 ; 1/(2! * CF**2)
		.float	0.693147186300000 ; $80 $31 $72 $18 $10 ; 1/(1! * CF**1)


	;-----------------------------------------------------------------
	; The following are unpacked values of the above.  These are six
	; bytes in length, with the sign bit expanded into the last byte,
	; and the mantissa has an explicit 1 bit set.
	;-----------------------------------------------------------------

		.floatd	1e38	; $FF $16 $76 $99 $52 !  FF167699 50
		.floatd	4e10	; $A4 $15 $02 $F9 $00
		.floatd	2e10	; $A3 $15 $02 $F9 $00
		.floatd	1e10	; $A2 $15 $02 $F9 $00
		.floatd	1	; $81 $00 $00 $00 $00
		.floatd	0.5	; $80 $00 $00 $00 $00
		.floatd	0.25	; $7F $00 $00 $00 $00
		.floatd	1e-4	; $73 $51 $B7 $59 $59
		.floatd	1e-37	; $06 $08 $1C $14 $14 !  06081C EA 14
		.floatd	1e-38	; $02 $59 $C7 $EE $EE !  0259C7 DCED
		.floatd	1e-39	; $00 $02 $00 $00 $00 !! FF2E397D8A
		.floatd	0	; $00 $00 $00 $00 $00
		.floatd	-1	; $81 $80 $00 $00 $00
		.floatd	-10	; $84 $A0 $00 $00 $00

	;-------------------------------------------------------------------
	; Table @ $BFC7 (CB 1.2)
	;-------------------------------------------------------------------

		.floatd	-14.381390673	; $84 $E6 $1A $2D $1B ; -((2 * 3.1415926535) ** 11) / 11!
		.floatd	 42.007797130	; $86 $28 $07 $FB $F8 ;  ((2 * 3.1415926535) **  9) /  9!
		.floatd	-76.704170270	; $87 $99 $68 $89 $01 ; -((2 * 3.1415926535) **  7) /  7!
		.floatd	 81.605223700	; $87 $23 $35 $DF $E1 ;  ((2 * 3.1415926535) **  5) /  5!
		.floatd	-41.341702110	; $86 $A5 $5D $E7 $28 ; -((2 * 3.1415926535) **  3) /  3!
		.floatd	  6.283185307	; $83 $49 $0F $DA$A2  ;    2 * 3.1415926535

	;----------------------------------
	; Table @ $B0B6 (CB 1.2)
	;----------------------------------

		.floatd	99999999.93	; $98 $3E $BC $1F $FD @ B0B6
		.floatd	999999999.3	; $9E $6E $6B $27 $FD @ B0BB
		.floatd	1e9		; $9E $6E $6B $28 $00 @ B0C0

	;--------------------------------------------------------------
	; Table @ $83E0 (ECB 1.1)
	;
	; NOTE: the decimal value is correctly listed in the book, however,
	; the following expression doesn't apprear to be correct.
	;-------------------------------------------------------------

		.floatd	-0.000684793912 ; $76 $B3 $83 $BD $D3 ; -1/23
		.floatd	+0.004850942157	; $79 $1E $F4 $A6 $F5 ;  1/21
		.floatd	-0.016111701850	; $7B $83 $FC $B0 $10 ; -1/19
		.floatd	+0.034209638050	; $7C $0C $1F $67 $CA ;  1/17
		.floatd	-0.054279132770	; $7C $DE $53 $CB $C1 ; -1/15
		.floatd	+0.072457196550	; $7D $14 $64 $70 $4C ;  1/13
		.floatd	-0.089802395400	; $7D $B7 $EA $51 $7A ; -1/11
		.floatd	+0.110932413450	; $7D $63 $30 $88 $7E ;  1/ 9
		.floatd	-0.142839807700	; $7E $92 $44 $99 $3A ; -1/ 7
		.floatd	+0.199999120500	; $7E $4C $CC $91 $C7 ;  1/ 5
		.floatd	-0.333333315700	; $7F $AA $AA $AA $13 ; -1/ 3

	;---------------------------------
	; Table @ $841D (ECB 1.1)
	;
	; NOTE: The LN2 constant by Microsoft is incorrect---it should be
	; 0.693147180_55, not _65 as listed.
	;---------------------------------

		.floatd	0.4342559420	; $7F $5E $56 $CB $79 ; (2/7)*(1/LN2)
		.floatd	0.5765845413	; $80 $13 $9B $0B $64 ; (2/5)*(1/LN2)
		.floatd	0.9618007593	; $80 $76 $38 $93 $16 ; (2/3)*(1/LN2)
		.floatd	2.8853900735	; $82 $38 $AA $3B $20 ; (2/1)*(1/LN2)
		.floatd	1 / 2**0.5	; $80 $35 $04 $F3 $34 ; 1 / SQRT(2)
		.floatd	2**0.5		; $81 $35 $04 $F3 $34 ; SQRT(2)
		.floatd	-0.5		; $80 $80 $00 $00 $00 ; 
		.floatd	0.69314718065   ; $80 $31 $72 $17 $F8 ; LN2

	;---------------------------------
	; Table starts at $84C9 (ECB 1.1)
	;---------------------------------

		.floatd	1.442695041000000 ; $81 $38 $AA $3B $29 ; (CF)
		.floatd	0.000021498763705 ; $71 $34 $58 $3E $56 ; 1/(7! * CF**7)
		.floatd	0.000143523140400 ; $74 $16 $7E $B3 $1B ; 1/(6! * CF**6)
		.floatd	0.001342263482500 ; $77 $2F $EE $E3 $85 ; 1/(5! * CF**5)
		.floatd	0.009614017015000 ; $7A $1D $84 $1C $2A ; 1/(4! * CF**4)
		.floatd	0.055505126870000 ; $7C $63 $59 $58 $0A ; 1/(3! * CF**3)
		.floatd	0.240226384650000 ; $7E $75 $FD $E7 $C6 ; 1/(2! * CF**2)
		.floatd	0.693147186300000 ; $80 $31 $72 $18 $10 ; 1/(1! * CF**1)

		end
