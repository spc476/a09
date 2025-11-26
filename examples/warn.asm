; * - only with the test format

		org	$0

.start		lda	<<b16,x		; W0002, W0003, W0010
		ldb	#$FF12		; W0004
		std	foobar		; W0005
		lda	b5,u		; W0006
badwrite	lda	badwrite
		sta	badwrite	; W0014*
		clr	chkwrite	; W0016*
		ldb	b8,s		; W0007
		tfr	a,x		; W0008, W0015*
		lbsr	a_really_long_label_that_exceeds_the_internal_limit_its_quite_long
					; W0001, W0009
		sta	[<<b5,y]	; W0011
		bra	another_long_label_that_is_good

a_really_long_label_that_exceeds_the_internal_limit_its_quite_long ; W0001
		rts

another_long_label_that_is_good
		clra
.but_this_makes_it_too_long_to_use	; W0001
		decb
		bne	.but_this_makes_it_too_long_to_use ; W0001

		bra	next8		; W0012
next8		lbra	next16		; W0012
next16		brn	next8b		; okay (BRN exempted W0012)
next8b		lbrn	next16b		; okay (BRN exempted W0009, W0012)
next16b		rts

		bsr	next16b
		rts			; W0020
		jsr	<next16b
		rts			; W0020
		jsr	>next16b
		rts			; W0020
		jsr	32,x
		rts			; W0020

		puls	a,b
		rts			; W0021
		pulu	a,b
		rts			; no warning

		rts
		nop			; W0022
		puls	pc		; W0024
		pulu	pc		; W0024
		
		bsr	next16b
lab1		rts			; no warning
		puls	a,b
lab2		rts			; no warning

		.opt	test tron	; W0023

		.opt	* real msfp
		.floatd	1e38		; W0019


foobar		equ	$20
b16		equ	$8080
b5		equ	3
b8		equ	25

a		equ	0		; W0013
b		equ	1		; W0013
d		equ	2		; W0013

;***********************************************************

	.test	"test"
	.opt	test	stacksize 1024	; W0018
	.opt	test	stack	$F000	; W0017
		lbsr	badwrite
		rts

	.endtst
	.tron
chkwrite	fcb	55

	; -------------------------------------
	; This is needed for W0014 .. W0016
	; -------------------------------------

	.opt	test	prot	rwx,0,next16b
	.opt	test	prot	rwt,chkwrite
