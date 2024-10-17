
;***************************************************************************
; Example of unit tests.
; GPL3+ Copyright (C) 2024 by Sean Conner.
;***************************************************************************

lfsr		equ	$F6

		org	$4000
start		bsr	random
		rts

the.byte	fcb	$55
.word		fdb	$AAAA
.addr		fdb	.value
.value		fcb	$33

		fdb	the.byte
		fdb	the.word
		fdb	the.addr

;***********************************************
;	RANDOM		Generate a random number
;Entry:	none
;Exit:	B - random number (1 - 255)
;***********************************************

random		ldb	lfsr
	.assert	/b = @lfsr , "should always be true"
		andb	#1
		negb
		andb	#$B4
	.assert	/b = -(@lfsr & 1) & $B4
		stb	,-s
		ldb	lfsr
		lsrb
		eorb	,s+
		stb	lfsr
		rts

;-----------------------------------------------

	.test	"RaNDoM"
random.test
	.opt	test	prot	rw,lfsr
	.opt	test	poke	lfsr,2
	.opt	test	prot	rxt,random,random.test

		ldx	#.result_array
		clra
		clrb
.setmem		sta	,x+
		decb
		bne	.setmem
		ldx	#.result_array + 128
		lda	#255
	.tron
.loop		lbsr	random
	.assert	/B <> 0 , "degenerate LFSR"
	.assert	@/b,x = 0 , "non-repeating b,x"
		tst	b,x
	.assert	/CC.z = 1 , "non-repeating"
		inc	b,x
		deca
		bne	.loop
	.assert @the.byte = $55 && @@the.word = $AAAA , "tis a silly test"
	.assert @@the.addr    = the.value , "another silly test"
	.assert	@the.value    = $33 , "another comment"
	.assert	@(@@the.addr) = $33
		tfr	cc,a
	.assert	/a    = /cc
	.assert	/cc   = /a
	.assert	/cc   = $54
	.assert	/cc.f =  1
	.assert	/cc.i =  1
	.assert /cc.z =  1
	.assert /cc.n <> 1
	.assert /cc.n =  0

		rts
	.troff
.result_array	rmb	256

	.endtst

		nop

;***********************************************

		end	start
