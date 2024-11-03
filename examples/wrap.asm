
;************************************************************************
; Not a vector, but two very well known routines to interface with BASIC
;************************************************************************

INTCVT		equ	$B3ED	; convert CB.fpa0 to register D
GIVBF		equ	$B4F3	; convert register B to CB.fpa0
GIVABF		equ	$B4F4	; convert register D to CB.fpa0

;************************************************************************
; Color Computer BASIC string descriptor.  Five bytes, only three are used,
; which is defined below:
;************************************************************************

_STRLEN		equ	0
_STRPTR		equ	2

;************************************************************************
; To test routines interfacing with BASIC, we set INTCVT to just return, so
; set D to the value required.  We also set GIVBF and GIVABF to just
; basically return as well.  The other routines?  Not so much.
;************************************************************************

		.opt	test	poke INTCVT,$39 ; RTS
		.opt	test	poke GIVBF ,$4F ; CLRA
		.opt	test	poke GIVABF,$39 ; RTS

		.opt	test	prot rx,INTCVT
		.opt	test	prot rx,GIVBF
		.opt	test	prot rx,GIVABF

;*********************************************************************
; Set some options for the BASIC backend
;*********************************************************************

		.opt	basic defusr0 wrap
		.opt	basic defusr1 upper
		.opt	basic line 1
		.opt	basic incr 1
		.opt	basic strspace 256

		org	$7F00

;*********************************************************************

L_strptr	equ	2
L_strlen	equ	0

wrap		jsr	INTCVT
		pshs	x,d		; save X and what were given
		tfr	d,x		; point to string descriptor
		leas	-4,s		; create some temp space

		ldd	_STRPTR,x	; get pointer
		std	L_strptr,s	; save into local
		clra
		ldb	_STRLEN,x	; save length into
		std	L_strlen,s	; local
		ldx	L_strptr,s	; point to actual string

.nextline	ldb	L_strlen + 1,s	; get length
		cmpb	#32		; less than 32?  No more to do.
		blo	.done
		leax	31,x		;adjust to 31, we want an offset here

.scanspace	lda	,x		; get character
		cmpa	#32		; check for space
		beq	.wrap		; if so, wrap here
		leax	-1,x		; back up one
		cmpx	L_strptr,s	; have we backed up all the way?
		bne	.scanspace	; if not, keep checking
		leax	32,x
		bra	.accept		; if we got here, no breaking point

.wrap		lda	#13		; add CR to string
		sta	,x+
.accept		tfr	x,d		; length = (end - start)
		subd	L_strptr,s	; 
		std	,--s		; temp save
	.assert	/d <= /L_strlen+2,s
		ldd	L_strlen + 2,s	; len = len - tmplen
		subd	,s++
		std	L_strlen,s
		stx	L_strptr,s	; new line start here
		bra	.nextline	; continue
		
.done		leas	4,s		; let's blow this taco stand
		puls	x,d		; restore what we were given, and X
		jmp	GIVABF

	;=============================

	.test
		ldd	#._textdescr
		jsr	wrap
 .assert ._text    = "A naked blonde walks into a bar\rwith a poodle under one arm and\ra two foot salami under the\rother. She "
 .assert @._textow = 2
 .assert /d        = ._textdescr
		rts

._textdescr	fcb	._textlen
		fcb	0
		fdb	._text
		fcb	0

		align	16
._text		ascii	'A naked blonde walks into a bar with a poodle under one arm and a two foot salami under the other. She '
._textlen	equ	* - ._text
._textow	fcb	2
	.endtst

	;=============================

	.test
		ldd	#._text2descr
		jsr	wrap
 .assert ._text2    = "I can see that it works in\rpractice but will it work in\rtheory?"
 .assert @._text2ow = 4
 .assert /d         = ._text2descr
		rts

._text2descr	fcb	._text2len
		fcb	0
		fdb	._text2
		fcb	0

		align	16
._text2		ascii	'I can see that it works in practice but will it work in theory?'
._text2len	equ	* - ._text2
._text2ow	fcb	4
	.endtst

	;==============================

	.test
		ldd	#._text3descr
		jsr	wrap
 .assert ._text3    = "Everyone's first vi session:\r^C^C^X^X^X^XquitqQ!qdammit[esc]qwertyuiopasdfghjkl;:xwhat"
 .assert @._text3ow = 5
 .assert /d         = ._text3descr
		rts

._text3descr	fcb	._text3len
		fcb	0
		fdb	._text3
		fcb	0

		align	16
._text3		ascii	"Everyone's first vi session: ^C^C^X^X^X^XquitqQ!qdammit[esc]qwertyuiopasdfghjkl;:xwhat"
._text3len	equ	* - ._text3
._text3ow	fcb	5
	.endtst

;*******************************************************************

upper		jsr	INTCVT		; get PTR
		pshs	x,d		; save X and what we were given
		tfr	d,x
		ldb	_STRLEN,x	; get length
		ldx	_STRPTR,x	; get string data

.loop		lda	,x		; get character
		cmpa	#'a'		; skip if < 'a'
		blo	.next
		cmpa	#'z'		; skip if > 'z'
		bhi	.next
		anda	#$5F		; lower case
.next		sta	,x+		; save back into string
		decb
		bne	.loop
		puls	x,d		; what we were given, and X
	.assert	/pc <= $7FFD
		jmp	GIVABF

	;===============================

	.test
		ldd	#._textdescr
		jsr	upper
	.assert ._text    = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~"
	.assert @._textow = 3
	.assert /d        = ._textdescr
		rts

._textdescr	fcb	._textlen
		fcb	0
		fdb	._text
		fcb	0

		align	16
._text		ascii	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
._textlen	equ	* - ._text
._textow	fcb	3

	.endtst

		end
