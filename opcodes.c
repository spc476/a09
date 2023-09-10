
/* GPL3+ */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

static bool op_inh(struct opcdata *opd)
{
  assert(opd     != NULL);
  assert(opd->sz == 0);
  
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
  
  opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
  return true;  
}

/**************************************************************************/

static unsigned char value_lsb(struct a09 *a09,uint16_t value,int pass)
{
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  
  unsigned char hi = value >> 8;
  unsigned char lo = value & 255;
  
  if (pass == 2)
  {
    if ((hi == 255) && (lo < 128))
      message(a09,MSG_WARNING,"16-bit value truncated");
    else if ((hi > 0) && (hi < 255))
      message(a09,MSG_WARNING,"16-bit value truncated");
  }
  
  return lo;
}

/**************************************************************************/

static bool op_8(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->sz   == 0);
  assert(opd->a09  != NULL);
  assert(opd->mode != AM_INHERENT);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  switch(opd->mode)
  {
    case AM_IMMED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_IMMED];
         opd->bytes[opd->sz++] = value_lsb(opd->a09,opd->value,opd->pass);
         return true;
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_DIRECT];
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INDEX:
         return message(opd->a09,MSG_ERROR,"unsupported");
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_EXTENDED];
         opd->bytes[opd->sz++] = opd->value >> 8;
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INHERENT:
         return message(opd->a09,MSG_ERROR,"How did this happen?");
  }
  
  return false;
}

/**************************************************************************/

static bool op_8s(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->mode != AM_INHERENT);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->mode == AM_IMMED)
    return message(opd->a09,MSG_ERROR,"immedate mode not supported");
  return op_8(opd);
}

/**************************************************************************/

static bool op_16(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->sz   == 0);
  assert(opd->a09  != NULL);
  assert(opd->mode != AM_INHERENT);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
  
  switch(opd->mode)
  {
    case AM_IMMED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_IMMED];
         opd->bytes[opd->sz++] = opd->value >> 8;
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_DIRECT];
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INDEX:
         return message(opd->a09,MSG_ERROR,"unsupported");
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_EXTENDED];
         opd->bytes[opd->sz++] = opd->value >> 8;
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INHERENT:
         return message(opd->a09,MSG_ERROR,"How did this happen?");
  }
  
  return false;
}

/**************************************************************************/

static bool op_16s(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->mode != AM_INHERENT);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->mode == AM_IMMED)
    return message(opd->a09,MSG_ERROR,"immedate mode not supported");
  return op_16(opd);
}

/**************************************************************************/

static bool op_imm(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_1(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->sz   == 0);
  assert(opd->a09  != NULL);
  assert(opd->mode != AM_INHERENT);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
  
  switch(opd->mode)
  {
    case AM_IMMED:
         return message(opd->a09,MSG_ERROR,"immediate mode not supported");
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_DIRECT];
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INDEX:
         return message(opd->a09,MSG_ERROR,"unsupported");
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_EXTENDED];
         opd->bytes[opd->sz++] = opd->value >> 8;
         opd->bytes[opd->sz++] = opd->value & 255;
         return true;
         
    case AM_INHERENT:
         return message(opd->a09,MSG_ERROR,"How did this happen?");
  }
  
  return false;
}

/**************************************************************************/

static bool op_br(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_lbr(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_lea(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_pushx(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_pullx(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_pushu(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_pullu(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool op_exg(struct opcdata *opd)
{
  (void)opd;
  return false;
}

/**************************************************************************/

static bool pseudo_equ(struct opcdata *opd)
{
  assert(opd != NULL);
  assert(opd->label != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->label == NULL)
    return message(opd->a09,MSG_ERROR,"missing label for EQU");
  
  if (opd->pass == 1)
  {
    struct symbol *sym = symbol_find(opd->a09,opd->label);
    assert(sym != NULL); /* this should always be the case */
    sym->value         = opd->value;
    sym->equ           = true;
  }
  
  return true;
}

/**************************************************************************/

static struct opcode const opcodes[] =
{
  { "ABX"   , op_inh     , { 0x3A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ADCA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ADCB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ADDA"  , op_8       , { 0x48 , 0x49 , 0x4A , 0x4B } , 0x00 , false } ,
  { "ADDB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ADDD"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ANDA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ANDB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ANDCC" , op_imm     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASL"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASLA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASLB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASR"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASRA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ASRB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BEQ"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BGE"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BGT"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BHI"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BHS"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BITA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BITB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BLE"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BLO"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BLS"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BLT"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BMI"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BNE"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BPL"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BRA"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BRN"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BSR"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "BVC"   , op_br      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CLR"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CLRA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CLRB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CMPA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CMPB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CMPD"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "CMPS"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x11 , true  } ,
  { "CMPU"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x11 , true  } ,
  { "CMPX"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "CMPY"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "COM"   , op_1       , { 0x00 , 0x03 , 0x63 , 0x73 } , 0x00 , false } ,
  { "COMA"  , op_inh     , { 0x43 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "COMB"  , op_inh     , { 0x53 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "CWAI"  , op_imm     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "DAA"   , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "DEC"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "DECA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "DECB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "EORA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "EORB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "EQU"   , pseudo_equ , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "EXG"   , op_exg     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "INC"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "INCA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "INCB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "JMP"   , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "JSR"   , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "LBEQ"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBGE"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBGT"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBHI"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBLE"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBLO"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBLS"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBLT"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBMI"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBNE"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBPL"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBRA"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBRN"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBSH"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBSR"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LBVC"  , op_lbr     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LDA"   , op_8       , { 0x00 , 0x00 , 0x00 , 0xB6 } , 0x00 , false } ,
  { "LDB"   , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "LDD"   , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "LDS"   , op_16      , { 0xCE , 0xDE , 0xEE , 0xFE } , 0x10 , true  } ,
  { "LDU"   , op_16      , { 0xCE , 0xDE , 0xEE , 0xFE } , 0x00 , true  } ,
  { "LDX"   , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "LDY"   , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "LEAS"  , op_lea     , { 0x00 , 0x00 , 0x32 , 0x00 } , 0x00 , true  } ,
  { "LEAU"  , op_lea     , { 0x00 , 0x00 , 0x33 , 0x00 } , 0x00 , true  } ,
  { "LEAX"  , op_lea     , { 0x00 , 0x00 , 0x30 , 0x00 } , 0x00 , true  } ,
  { "LEAY"  , op_lea     , { 0x00 , 0x00 , 0x31 , 0x00 } , 0x00 , true  } ,
  { "LSR"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "LSRA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "LSRB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "MUL"   , op_inh     , { 0x3D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "NEG"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "NEGA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "NEGB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "NOP"   , op_inh     , { 0x12 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ORA"   , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ORB"   , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ORCC"  , op_imm     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "PSHS"  , op_pushx   , { 0x34 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "PSHU"  , op_pullx   , { 0x36 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "PULS"  , op_pushu   , { 0x35 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "PULU"  , op_pullu   , { 0x37 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ROL"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ROLA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ROLB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "ROR"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "RORA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "RORB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "RTI"   , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "RTS"   , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SBCA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SBCB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SEX"   , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "STA"   , op_8s      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "STB"   , op_8s      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "STD"   , op_16s     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "STS"   , op_16s     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "STU"   , op_16s     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "STX"   , op_16s     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "STY"   , op_16s     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
  { "SUBA"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SUBB"  , op_8       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SUBD"  , op_16      , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
  { "SWI"   , op_inh     , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "SWI2"  , op_inh     , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x10 , false } ,
  { "SWI3"  , op_inh     , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x11 , false } ,
  { "SYNC"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "TFR"   , op_exg     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "TST"   , op_1       , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "TSTA"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  { "TSTB"  , op_inh     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
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
