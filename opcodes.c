
/* GPL3+ */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include "a09.h"

/**************************************************************************/

static bool op_inh(struct opcode const *op,struct a09 *a09)
{
  assert(op       != NULL);
  assert(a09      != NULL);
  assert(a09->out != NULL);
  
  if (op->page)
  {
    if (fwrite(&op->page,1,1,a09->out) != 1)
      return false;
  }
  if (fwrite(&op->opcode[0],1,1,a09->out) != 1)
    return false;
  return true;
}

/**************************************************************************/

static bool op_8(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_8s(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_16(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_16s(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_imm(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_1(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_1ab(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_br(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_lbr(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_lea(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_pushx(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_pullx(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_pushu(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_pullu(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool op_exg(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  (void)a09;
  return false;
}

/**************************************************************************/

static bool pseudo_ns(struct opcode const *op,struct a09 *a09)
{
  (void)op;
  assert(a09 != NULL);
  
  char *label = NULL;
  size_t len  = 0;
  int c       = skip_space(a09);
  
  if (!read_label(a09,&label,&len,c))
  {
    fprintf(stderr,"%s(%zu): syntax error---label expected\n",a09->filename,a09->lnum);
    return false;
  }
  
  if (a09->debug)
    fprintf(stderr,"DEBUG: namespace='%s'\n",label);
    
  if (!set_namespace(a09,label))
  {
    fprintf(stderr,"%s(%zu): could not set namespace '%s'\n",a09->filename,a09->lnum,label);
    free(label);
    return false;
  }
  free(label);
  return true;
}

/**************************************************************************/

static struct opcode const opcodes[] =
{
  { "ABX"   , op_inh   , { 0x3A , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ADCA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ADCB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ADDA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ADDB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ADDD"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ANDA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ANDB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ANDCC" , op_imm   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASL"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASLA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASLB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASR"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASRA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ASRB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BEQ"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BGE"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BGT"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BHI"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BHS"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BITA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BITB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BLE"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BLO"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BLS"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BLT"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BMI"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BNE"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BPL"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BRA"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BRN"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BSR"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "BVC"   , op_br    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CLR"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CLRA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CLRB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CMPA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CMPB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CMPD"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "CMPS"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x11 } ,
  { "CMPU"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x11 } ,
  { "CMPX"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CMPY"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "COM"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "COMA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "COMB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "CWAI"  , op_imm   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "DAA"   , op_inh   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "DEC"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "DECA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "DECB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "EORA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "EORB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "EXG"   , op_exg   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "INC"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "INCA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "INCB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "JMP"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "JSR"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LBEQ"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBGE"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBGT"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBHI"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBLE"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBLO"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBLS"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBLT"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBMI"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBNE"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBPL"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBRA"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBRN"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBSH"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBSR"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LBVC"  , op_lbr   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LDA"   , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDB"   , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDD"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDS"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDU"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDX"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LDY"   , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "LEAS"  , op_lea   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LEAU"  , op_lea   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LEAX"  , op_lea   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LEAY"  , op_lea   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LSR"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LSRA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "LSRB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "MUL"   , op_inh   , { 0x3D , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "NEG"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "NEGA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "NEGB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "NOP"   , op_inh   , { 0x12 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "NS"    , pseudo_ns, { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ORA"   , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ORB"   , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ORCC"  , op_imm   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "PSHS"  , op_pushx , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "PSHU"  , op_pullx , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "PULS"  , op_pushu , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "PULU"  , op_pullu , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ROL"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ROLA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ROLB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "ROR"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "RORA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "RORB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "RTI"   , op_inh   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "RTS"   , op_inh   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SBCA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SBCB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SEX"   , op_inh   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STA"   , op_8s    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STB"   , op_8s    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STD"   , op_16s   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STS"   , op_16s   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "STU"   , op_16s   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STX"   , op_16s   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "STY"   , op_16s   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "SUBA"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SUBB"  , op_8     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SUBD"  , op_16    , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SWI"   , op_inh   , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "SWI2"  , op_inh   , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x10 } ,
  { "SWI3"  , op_inh   , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x11 } ,
  { "SYNC"  , op_inh   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "TFR"   , op_exg   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "TST"   , op_1     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "TSTA"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
  { "TSTB"  , op_1ab   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 } ,
};

/**************************************************************************/

static int opcode_cmp(void const *needle,void const *haystack)
{
  char          const *key    = needle;
  struct opcode const *opcode = haystack;
  
  return (strcmp(key,opcode->name));
}

/**************************************************************************/

struct opcode const *op_find(char const *name)
{
  assert(name != NULL);

  struct opcode const *op = bsearch(
        name,
        opcodes,
        sizeof(opcodes)/sizeof(opcodes[0]),
        sizeof(opcodes[0]),
        opcode_cmp
  );

  return op;
}

/**************************************************************************/
