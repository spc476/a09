
/* GPL3+ */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

static unsigned char value_5bit(struct a09 *a09,uint16_t value,int pass)
{
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  
  if ((pass == 2) && (value >= 16) && (value <= 65519))
    message(a09,MSG_WARNING,"16-bit value truncated to 5 bits");
  return value & 31;
}

/**************************************************************************/

static unsigned char value_lsb(struct a09 *a09,uint16_t value,int pass)
{
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  
  if ((pass == 2) && (value >= 256) && (value <= 65407))
    message(a09,MSG_WARNING,"16-bit value truncated to 8 bits");
    
  return value & 255;
}

/**************************************************************************/

static bool collect_string(struct opcdata *opd,struct buffer *buf,char delim)
{
  char c;
  
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert(buf         != NULL);
  
  memset(buf->buf,0,sizeof(buf->buf));
  buf->widx = 0;
  buf->ridx = 0;
  
  while((c = opd->buffer->buf[opd->buffer->ridx++]) != delim)
  {
    if (c == '\0')
      return message(opd->a09,MSG_ERROR,"unexpected end of string");
    if (c == '\\')
    {
      c = opd->buffer->buf[opd->buffer->ridx++];
      switch(c)
      {
        case 'a':  c = '\a'; break;
        case 'b':  c = '\b'; break;
        case 't':  c = '\t'; break;
        case 'n':  c = '\n'; break;
        case 'v':  c = '\v'; break;
        case 'f':  c = '\f'; break;
        case 'r':  c = '\r'; break;
        case 'e':  c = '\033'; break;
        case '"':  c = '"';  break;
        case '\'': c = '\''; break;
        case '\\': c = '\\'; break;
        case '\0': return message(opd->a09,MSG_ERROR,"unexpected end of string");
        default:   return message(opd->a09,MSG_ERROR,"invalid escape character");
      }
    }
    
    if (buf->widx == sizeof(buf->buf)-1)
      return false;
    buf->buf[buf->widx++] = c;
  }
  
  assert(buf->widx < sizeof(buf->buf));
  buf->buf[buf->widx] = '\0';
  return true;
}

/**************************************************************************/

static bool parse_string(struct opcdata *opd,struct buffer *buf)
{
  assert(opd         != NULL);
  assert(opd->buffer != NULL);
  assert(buf         != NULL);
  
  char c = skip_space(opd->buffer);
  
  if ((c == '"') || (c == '\''))
    return collect_string(opd,buf,c);
  else
    return message(opd->a09,MSG_ERROR,"not a string");
}

/**************************************************************************/

static bool parse_dirext(struct opcdata *opd)
{
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
  if (opd->value.defined && ((opd->value.value >> 8) == opd->a09->dp))
    opd->mode = AM_DIRECT;
  else
    opd->mode = AM_EXTENDED;
  return true;
}

/**************************************************************************/

static unsigned char index_register(char c)
{
  switch(toupper(c))
  {
    case 'X': return 0x00;
    case 'Y': return 0x20;
    case 'U': return 0x40;
    case 'S': return 0x60;
    default: abort();
  }
}
/**************************************************************************/

static unsigned char acc_register(char c)
{
  switch(toupper(c))
  {
    case 'A': return 0x86;
    case 'B': return 0x85;
    case 'D': return 0x8B;
    default: abort();
  }
}

/**************************************************************************/

static bool parse_operand(struct opcdata *opd)
{
  assert(opd != NULL);
  
  bool  indexindirect = false;
  char  c             = skip_space(opd->buffer);
  
  if ((c == ';') || (c == '\0'))
  {
    opd->mode = AM_INHERENT;
    return true;
  }
  
  if (c == '#')
  {
    if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
      return false;
    opd->mode = AM_IMMED;
    return true;
  }
  
  if (c == '[')
  {
    indexindirect = true;
    c = skip_space(opd->buffer);
  }
  
  if (c == ',')
  {
    opd->mode           = AM_INDEX;
    opd->bits           = 0;
    opd->value.postbyte = 0x80;
    c                   = toupper(skip_space(opd->buffer));
    
    switch(c)
    {
      case 'X':
      case 'Y':
      case 'U':
      case 'S': opd->value.postbyte |= index_register(c); break;
      
      case '-':
           c = opd->buffer->buf[opd->buffer->ridx];
           if (c == '-')
           {
             opd->value.postbyte |= 0x03;
             c                    = opd->buffer->buf[++opd->buffer->ridx];
           }
           else
             opd->value.postbyte |= 0x02;
             
           switch(toupper(c))
           {
             case 'X':
             case 'Y':
             case 'U':
             case 'S': opd->value.postbyte |= index_register(c); break;
             default: return message(opd->a09,MSG_ERROR,"invalid index register");
           }
           
           if (opd->buffer->buf[++opd->buffer->ridx] == ']')
           {
             if (!indexindirect)
               return message(opd->a09,MSG_ERROR,"end of indirection without start of indirection error");
             opd->value.postbyte |= 0x10;
           }
           return true;
           
      default:
           return message(opd->a09,MSG_ERROR,"invalid index register");
    }
    
    c = skip_space(opd->buffer);
    
    if ((c == ';') || (c == '\0') || isspace(c))
    {
      if (indexindirect)
        return message(opd->a09,MSG_ERROR,"missing end of index indirect mode");
      opd->value.postbyte |= 0x04;
      return true;
    }
    else if (c == ']')
    {
      if (!indexindirect)
        return message(opd->a09,MSG_ERROR,"end of indirection without start of indirection error");
      opd->value.postbyte |= 0x14;
      return true;
    }
    else if (c != '+')
      return message(opd->a09,MSG_ERROR,"syntax error in post-increment index mode");
      
    c = opd->buffer->buf[opd->buffer->ridx++];
    if (c == '+')
    {
      opd->value.postbyte |= 0x01;
      if (opd->buffer->buf[opd->buffer->ridx++] == ']')
      {
        if (!indexindirect)
          return message(opd->a09,MSG_ERROR,"end of indirection without start of indirection error");
        opd->value.postbyte |= 0x10;
      }
      return true;
    }
    else if ((c == ';') || (c == '\0') || isspace(c))
    {
      if (indexindirect)
        return message(opd->a09,MSG_ERROR,"missing end of indirection error");
      return true;
    }
    else
      return message(opd->a09,MSG_ERROR,"syntax error in index mode");
  }
  
  opd->buffer->ridx--; // ungetc()
  
  /*------------------------------------------------------------------------
  ; Special case here, A,r B,r or D,r needs to be checked, if the 2nd next
  ; character is ',', then we have one-character to check, which could be a
  ; register.  Handle that case here.
  ;------------------------------------------------------------------------*/
  
  if (opd->buffer->buf[opd->buffer->ridx+1] == ',')
  {
    opd->mode = AM_INDEX;
    opd->bits = 0;
    c         = toupper(opd->buffer->buf[opd->buffer->ridx]);
    
    switch(c)
    {
      case 'A':
      case 'B':
      case 'D':
           opd->value.postbyte  = acc_register(c);
           opd->buffer->ridx   += 2; // skip comma
           c = toupper(opd->buffer->buf[opd->buffer->ridx]);
           switch(c)
           {
             case 'X':
             case 'Y':
             case 'U':
             case 'S': opd->value.postbyte |= index_register(c); break;
             default: return message(opd->a09,MSG_ERROR,"invalid index register");
           }
           c = opd->buffer->buf[++opd->buffer->ridx];
           if (c == ']')
           {
             if (!indexindirect)
               return message(opd->a09,MSG_ERROR,"end of indirection without start of indirection error");
             opd->value.postbyte |= 0x10;
             c = opd->buffer->buf[++opd->buffer->ridx];
           }
           
           if ((c == ';') || (c == '\0') || isspace(c))
             return true;
             
           return message(opd->a09,MSG_ERROR,"invalid accumulator register in index mode");
           
      default:
           break;
    }
  }
  
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
    
  c = skip_space(opd->buffer);
  
  if ((c == ';') || (c == '\0'))
  {
    if (indexindirect)
      return message(opd->a09,MSG_ERROR,"missing end of indirection error");
    if ((opd->value.bits == 5) || (opd->value.bits == 8))
    {
      opd->mode = AM_DIRECT;
      return true;
    }
    if (opd->value.bits == 16)
    {
      opd->mode = AM_EXTENDED;
      return true;
    }
    assert(opd->value.bits == 0);
    if (opd->value.defined && ((opd->value.value >> 8) == opd->a09->dp))
      opd->mode = AM_DIRECT;
    else
      opd->mode = AM_EXTENDED;
    return true;
  }
  
  if (c == ']')
  {
    if (!indexindirect)
      return message(opd->a09,MSG_ERROR,"end of indirection without start of indirection error");
    opd->value.postbyte = 0x9F;
    opd->value.bits     = 16;
    opd->mode           = AM_INDEX;
    return true;
  }
  
  if (c != ',')
    return message(opd->a09,MSG_ERROR,"missing expected comma");
    
  opd->value.postbyte = 0;
  opd->mode           = AM_INDEX;
  c                   = skip_space(opd->buffer);
  
  switch(toupper(c))
  {
    case 'X':
    case 'Y':
    case 'U':
    case 'S':
         opd->value.postbyte |= index_register(c);
         if (opd->value.bits == 5)
         {
           opd->value.postbyte |= value_5bit(opd->a09,opd->value.value,opd->pass);
           opd->bits            = 5;
         }
         else if (opd->value.bits == 8)
         {
           opd->value.postbyte |= 0x88;
           opd->bits            = 8;
         }
         else if ((opd->value.value < 16) || (opd->value.value > 65519))
         {
           if (indexindirect)
           {
             opd->value.postbyte |= 0x88;
             opd->bits            = 8;
           }
           else
           {
             opd->value.postbyte |= opd->value.value & 0x1F;
             opd->bits            = 5;
           }
         }
         else if ((opd->value.value < 0x80) || (opd->value.value > 0xFF7F))
         {
           opd->value.postbyte |= 0x88;
           opd->bits            = 8;
         }
         else
         {
           opd->value.postbyte |= 0x89;
           opd->bits            = 16;
         }
         break;
         
    case 'P':
         if (toupper(opd->buffer->buf[opd->buffer->ridx]) != 'C')
           return message(opd->a09,MSG_ERROR,"invalid index register");
         opd->pcrel = true;
         opd->buffer->ridx++;
         
         if (opd->value.bits == 0)
         {
           if (opd->value.unknownpass1)
           {
             opd->value.postbyte = 0x8D;
             opd->bits           = 16;
           }
           else
           {
             uint16_t pc = opd->a09->pc + 2 + (opd->op->page != 0);
             uint16_t delta = opd->value.value - pc;
             if ((delta < 0x80) || (delta > 0xFF7F))
             {
               opd->value.postbyte = 0x8C;
               opd->bits           = 8;
             }
             else
             {
               opd->value.postbyte = 0x8D;
               opd->bits           = 16;
             }
           }
         }
         else if (opd->value.bits == 8)
         {
           opd->value.postbyte = 0x8C;
           opd->bits           = 8;
         }
         else
         {
           opd->value.postbyte = 0x8D;
           opd->bits           = 16;
         }
         break;
         
    default:
         return message(opd->a09,MSG_ERROR,"invalid index regster");
  }
  
  c = skip_space(opd->buffer);
  if (c == ']')
  {
    if (!indexindirect)
      return message(opd->a09,MSG_ERROR,"end of indirection without start indirection error");
    opd->value.postbyte |= 0x10;
    if (opd->bits == 5)
      opd->bits = 8;
  }
  
  return true;
}

/**************************************************************************/

static bool op_inh(struct opcdata *opd)
{
  assert(opd     != NULL);
  assert(opd->sz == 0);
  
  if (!parse_operand(opd))
    return false;
  if (opd->mode != AM_INHERENT)
    return message(opd->a09,MSG_ERROR,"operands found for op with no operands");
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
  return true;
}

/**************************************************************************/

static bool finish_index_bytes(struct opcdata *opd)
{
  assert(opd != NULL);
  assert(opd->mode == AM_INDEX);
  
  opd->bytes[opd->sz++] = opd->op->opcode[AM_INDEX];
  opd->bytes[opd->sz++] = opd->value.postbyte;
  if (opd->bits == 8)
  {
    if (opd->pcrel)
    {
      uint16_t pc      = opd->a09->pc + opd->sz + 1;
      uint16_t dt      = opd->value.value - pc;
      opd->value.value = dt;
    }
    opd->bytes[opd->sz++] = opd->value.value & 255;
  }
  else if (opd->bits == 16)
  {
    if (opd->pcrel)
      opd->value.value = opd->value.value - (opd->a09->pc + opd->sz + 2);
    opd->bytes[opd->sz++] = opd->value.value >> 8;
    opd->bytes[opd->sz++] = opd->value.value & 255;
  }
  return true;
}

/**************************************************************************/

static bool op_idie(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->sz   == 0);
  assert(opd->a09  != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_operand(opd))
    return false;
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  switch(opd->mode)
  {
    case AM_IMMED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_IMMED];
         if (opd->op->bit16)
         {
           opd->bytes[opd->sz++] = opd->value.value >> 8;
           opd->bytes[opd->sz++] = opd->value.value & 255;
         }
         else
           opd->bytes[opd->sz++] = value_lsb(opd->a09,opd->value.value,opd->pass);
         return true;
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_DIRECT];
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INDEX:
         return finish_index_bytes(opd);
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_EXTENDED];
         opd->bytes[opd->sz++] = opd->value.value >> 8;
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INHERENT:
         return message(opd->a09,MSG_ERROR,"How did this happen?");
  }
  
  return false;
}

/**************************************************************************/

static bool op_die(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->sz   == 0);
  assert(opd->a09  != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_operand(opd))
    return false;
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  switch(opd->mode)
  {
    case AM_IMMED:
         return message(opd->a09,MSG_ERROR,"immediate mode not supported for opcode");
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_DIRECT];
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INDEX:
         return finish_index_bytes(opd);
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode[AM_EXTENDED];
         opd->bytes[opd->sz++] = opd->value.value >> 8;
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INHERENT:
         return message(opd->a09,MSG_ERROR,"How did this happen again?");
  }
  
  return false;
}

/**************************************************************************/

static bool op_imm(struct opcdata *opd)
{
  assert(opd != NULL);
  
  if (!parse_operand(opd))
    return false;
    
  if (opd->mode != AM_IMMED)
    return message(opd->a09,MSG_ERROR,"instruction only supports immediate mode");
    
  assert(opd->op->page == 0);
  opd->bytes[opd->sz++] = opd->op->opcode[AM_IMMED];
  opd->bytes[opd->sz++] = value_lsb(opd->a09,opd->value.value,opd->pass);
  return true;
}

/**************************************************************************/

static bool op_br(struct opcdata *opd)
{
  assert(opd           != NULL);
  assert(opd->a09      != NULL);
  assert(opd->sz       == 0);
  assert(opd->op->page == 0);
  
  if (!parse_operand(opd))
    return false;
  if ((opd->mode != AM_DIRECT) && (opd->mode != AM_EXTENDED))
    return false;
    
  if (opd->value.external)
  {
    opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
    opd->bytes[opd->sz++] = 0;
  }
  else
  {
    uint16_t delta = opd->value.value - (opd->a09->pc + 2);
    
    if ((opd->pass == 2) && (delta > 0x007F) && (delta < 0xFF80))
      return message(opd->a09,MSG_ERROR,"target exceeds 8-bit range");
      
    opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
    opd->bytes[opd->sz++] = delta & 255;
  }
  return true;
}

/**************************************************************************/

static bool op_lbr(struct opcdata *opd)
{
  assert(opd             != NULL);
  assert(opd->a09        != NULL);
  assert(opd->sz         == 0);
  
  if (!parse_operand(opd))
    return false;
  if ((opd->mode != AM_DIRECT) && (opd->mode != AM_EXTENDED))
    return false;
    
  if (opd->op->page)
  {
    uint16_t delta = opd->value.external
                   ? 0
                   : opd->value.value - (opd->a09->pc + 4);
                   
    opd->bytes[opd->sz++] = opd->op->page;
    opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
    opd->bytes[opd->sz++] = delta >> 8;
    opd->bytes[opd->sz++] = delta & 255;
  }
  else
  {
    uint16_t delta = opd->value.external
                   ? 0
                   : opd->value.value - (opd->a09->pc + 3);
    opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
    opd->bytes[opd->sz++] = delta >> 8;
    opd->bytes[opd->sz++] = delta & 255;
  }
  return true;
}

/**************************************************************************/

static bool op_lea(struct opcdata *opd)
{
  assert(opd     != NULL);
  assert(opd->op != NULL);
  
  if (!parse_operand(opd))
    return false;
    
  if (opd->mode != AM_INDEX)
    return message(opd->a09,MSG_ERROR,"bad operand");
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
  return finish_index_bytes(opd);
}

/**************************************************************************/

static bool sop_findreg(struct indexregs const **ir,char const *src,char skip)
{
  static struct indexregs const index[] =
  {
    { "\2pc" , 0x80 , 0x50 , 0x05 , true  } ,
    { "\1s"  , 0x40 , 0x40 , 0x04 , true  } ,
    { "\1u"  , 0x40 , 0x30 , 0x03 , true  } ,
    { "\1y"  , 0x20 , 0x20 , 0x02 , true  } ,
    { "\1x"  , 0x10 , 0x10 , 0x01 , true  } ,
    { "\2dp" , 0x08 , 0xB0 , 0x0B , false } ,
    { "\1b"  , 0x04 , 0x90 , 0x09 , false } ,
    { "\1a"  , 0x02 , 0x80 , 0x08 , false } ,
    { "\2cc" , 0x01 , 0xA0 , 0x0A , false } ,
    { "\1d"  , 0x06 , 0x00 , 0x00 , true  } ,
    { "\2PC" , 0x80 , 0x50 , 0x05 , true  } ,
    { "\1S"  , 0x40 , 0x40 , 0x04 , true  } ,
    { "\1U"  , 0x40 , 0x30 , 0x03 , true  } ,
    { "\1Y"  , 0x20 , 0x20 , 0x02 , true  } ,
    { "\1X"  , 0x10 , 0x10 , 0x01 , true  } ,
    { "\2DP" , 0x08 , 0xB0 , 0x0B , false } ,
    { "\1B"  , 0x04 , 0x90 , 0x09 , false } ,
    { "\1A"  , 0x02 , 0x80 , 0x08 , false } ,
    { "\2CC" , 0x01 , 0xA0 , 0x0A , false } ,
    { "\1D"  , 0x06 , 0x00 , 0x00 , true  } ,
  };
  
  for (size_t i = 0 ; i < sizeof(index) / sizeof(index[0]) ; i++)
  {
    if (memcmp(src,&index[i].reg[1],index[i].reg[0]) == 0)
    {
      if (index[i].reg[1] == skip)
        return false;
      *ir = &index[i];
      return true;
    }
  }
  return false;
}

/**************************************************************************/

static bool op_pshpul(struct opcdata *opd)
{
  assert(opd           != NULL);
  assert(opd->op       != NULL);
  assert(opd->op->page == 0);
  
  unsigned char operand = 0;
  char          skip    = tolower(opd->op->name[3]); /* yes, I know! */
  char          c       = skip_space(opd->buffer);
  
  if (c == '\0')
    return false;
    
  opd->buffer->ridx--;
  
  while(!isspace(c) && (c != ';') && (c != '0'))
  {
    struct indexregs const *reg;
    
    if (!sop_findreg(&reg,&opd->buffer->buf[opd->buffer->ridx],skip))
      return message(opd->a09,MSG_ERROR,"bad register name");
    opd->buffer->ridx += reg->reg[0];
    operand |= reg->pushpull;
    c = skip_space(opd->buffer);
    if (c == '\0')
      break;
    if (c == ';')
      break;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"missing comma in register list");
  }
  
  opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
  opd->bytes[opd->sz++] = operand;
  return true;
}

/**************************************************************************/

static bool op_exg(struct opcdata *opd)
{
  assert(opd           != NULL);
  assert(opd->op       != NULL);
  assert(opd->op->page == 0);
  
  struct indexregs const *reg1;
  struct indexregs const *reg2;
  unsigned char           operand = 0;
  char                    c       = skip_space(opd->buffer);
  
  if (c == '\0')
    return false;
    
  opd->buffer->ridx--;
  
  if (!sop_findreg(&reg1,&opd->buffer->buf[opd->buffer->ridx],'\0'))
    return message(opd->a09,MSG_ERROR,"bad register name");
  opd->buffer->ridx += reg1->reg[0];
  operand |= reg1->tehi;
  if (opd->buffer->buf[opd->buffer->ridx] != ',')
    return message(opd->a09,MSG_ERROR,"missing comma");
  opd->buffer->ridx++;
  
  if (!sop_findreg(&reg2,&opd->buffer->buf[opd->buffer->ridx],'\0'))
    return message(opd->a09,MSG_ERROR,"bad register name");
  if (reg1->b16 != reg2->b16)
    message(opd->a09,MSG_WARNING,"ext/tfr mixed sized registers");
  operand |= reg2->telo;
  
  opd->bytes[opd->sz++] = opd->op->opcode[AM_OPERAND];
  opd->bytes[opd->sz++] = operand;
  return true;
}

/**************************************************************************/

static bool pseudo_equ(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"missing label for EQU");
    
  if (opd->pass == 1)
  {
    struct symbol *sym = symbol_find(opd->a09,&opd->label);
    assert(sym != NULL); /* this should always be the case */
    sym->value         = opd->value.value;
    sym->type          = SYM_EQU;
    sym->bits          = opd->value.value < 256 ? 8 : 16;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_set(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"missing label for SET");
    
  struct symbol  *sym = symbol_find(opd->a09,&opd->label);
  assert(sym != NULL);
  sym->value          = opd->value.value;
  sym->type           = SYM_SET;
  sym->bits           = opd->value.value < 256 ? 8 : 16;
  
  return true;
}

/**************************************************************************/

static bool pseudo_rmb(struct opcdata *opd)
{
  assert(opd != NULL);
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
  opd->data   = true;
  opd->datasz = opd->value.value;
  if (opd->pass == 2)
  {
    if (fseek(opd->a09->out,opd->value.value,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"%s",strerror(errno));
  }
  return true;
}

/**************************************************************************/

static bool pseudo_org(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"missing value for ORG");
    
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool pseudo_setdp(struct opcdata *opd)
{
  assert(opd != NULL);
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
  opd->a09->dp = value_lsb(opd->a09,opd->value.value,opd->pass);
  return true;
}

/**************************************************************************/

static bool pseudo_end(struct opcdata *opd)
{
  assert(opd != NULL);
  return message(opd->a09,MSG_ERROR,"END unsupported");
}

/**************************************************************************/

static bool pseudo_fcb(struct opcdata *opd)
{
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    opd->data = true;
    while(true)
    {
      char c = skip_space(opd->buffer);
      opd->buffer->ridx--;
      opd->datasz++;
      if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
        return false;
      c = skip_space(opd->buffer);
      if ((c == ';') || (c == '\0'))
        return true;
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"missing comma");
    }
  }
  else
  {
    opd->data = true;
    
    while(true)
    {
      char c = skip_space(opd->buffer);
      opd->buffer->ridx--;
      opd->datasz++;
      if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
        return false;
      unsigned char byte = value_lsb(opd->a09,opd->value.value,opd->pass);
      if (opd->sz < sizeof(opd->bytes))
        opd->bytes[opd->sz++] = byte;
      fwrite(&byte,1,1,opd->a09->out);
      if (ferror(opd->a09->out))
        return message(opd->a09,MSG_ERROR,"failed writing object file");
      c = skip_space(opd->buffer);
      
      if ((c == ';') || (c == '\0'))
        return true;
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"missing comma");
    }
  }
}

/**************************************************************************/

static bool pseudo_fdb(struct opcdata *opd)
{
  if (opd->pass == 1)
  {
    opd->data = true;
    while(true)
    {
      char c = skip_space(opd->buffer);
      opd->buffer->ridx--;
      opd->datasz += 2;
      if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
        return false;
      c = skip_space(opd->buffer);
      if ((c == ';') || (c == '\0'))
        return true;
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"missing comma");
    }
  }
  else
  {
    opd->data = true;
    
    while(true)
    {
      char c = skip_space(opd->buffer);
      opd->buffer->ridx--;
      opd->datasz += 2;
      if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
        return false;
      unsigned char word[2] =
      {
        opd->value.value >> 8 ,
        opd->value.value & 255
      };
      
      if (opd->sz < sizeof(opd->bytes))
      {
        opd->bytes[opd->sz++] = word[0];
        opd->bytes[opd->sz++] = word[1];
      }
      fwrite(word,1,2,opd->a09->out);
      if (ferror(opd->a09->out))
        return message(opd->a09,MSG_ERROR,"failed writing object file");
      c = skip_space(opd->buffer);
      
      if ((c == ';') || (c == '\0'))
        return true;
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"missing comma");
    }
  }
}

/**************************************************************************/

static bool pseudo_fcc(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  char          c = skip_space(opd->buffer);
  
  if (c == '\0')
    return message(opd->a09,MSG_ERROR,"expected end of input");
  if (!collect_string(opd,&textstring,c))
    return false;
    
  opd->data   = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (fwrite(textstring.buf,1,textstring.widx,opd->a09->out) != textstring.widx)
      return message(opd->a09,MSG_ERROR,"truncated output to object file");
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"error writing object file");
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_ascii(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  
  if (!parse_string(opd,&textstring))
    return false;
    
  opd->data = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (fwrite(textstring.buf,1,textstring.widx,opd->a09->out) != textstring.widx)
      return message(opd->a09,MSG_ERROR,"truncate output to object file");
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"error writing object file");
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_asciih(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  
  if (!parse_string(opd,&textstring))
    return false;
    
  opd->data = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    textstring.buf[textstring.widx - 1] |= 0x80;
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (fwrite(textstring.buf,1,textstring.widx,opd->a09->out) != textstring.widx)
      return message(opd->a09,MSG_ERROR,"truncate output to object file");
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"error writing object file");
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_asciiz(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  
  if (!parse_string(opd,&textstring))
    return false;
    
  opd->data = true;
  opd->datasz = textstring.widx + 1;
  
  if (opd->pass == 2)
  {
    textstring.widx++;
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (fwrite(textstring.buf,1,textstring.widx,opd->a09->out) != textstring.widx)
      return message(opd->a09,MSG_ERROR,"truncate output to object file");
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"error writing object file");
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_include(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer filename;
  bool          rc;
  struct a09    new = *opd->a09;
  
  if (!parse_string(opd,&filename))
    return false;
    
  new.label  = (label){ .s = 0 , .text = { '\0' } };
  new.inbuf  = (struct buffer){ .buf = {0}, .widx = 0 , .ridx = 0 };
  new.infile = filename.buf;
  new.in     = fopen(filename.buf,"r");
  
  if (new.in == NULL)
    return message(opd->a09,MSG_ERROR,"%s: %s",filename.buf,strerror(errno));
    
  if ((opd->pass == 2) && (new.list != NULL))
  {
    print_list(opd->a09,opd,false);
    fprintf(new.list,"                         | FILE %s\n",filename.buf);
    opd->includehack = true;
  }
  rc = assemble_pass(&new,opd->pass);
  
  if ((opd->pass == 2) && (new.list != NULL))
    fprintf(new.list,"                         | ENF-OF-LINE\n");
    
  fclose(new.in);
  opd->a09->pc     = new.pc;
  opd->a09->symtab = new.symtab;
  opd->a09->list   = new.list;
  return rc;
}

/**************************************************************************/

static bool pseudo_incbin(struct opcdata *opd)
{
  assert(opd != NULL);
  
  struct buffer  filename;
  FILE          *fp;
  bool           fill = false;
  
  if (!parse_string(opd,&filename))
    return false;
    
  if (opd->pass == 1)
  {
    fp = fopen(filename.buf,"rb");
    if (fp == NULL)
      return message(opd->a09,MSG_ERROR,"%s: '%s'",filename.buf,strerror(errno));
    if (fseek(fp,0,SEEK_END) == -1)
      return message(opd->a09,MSG_ERROR,"%s: '%s'",filename.buf,strerror(errno));
    long fsize = ftell(fp);
    if (fsize < 0)
      return message(opd->a09,MSG_ERROR,"%s: '%s'",filename.buf,strerror(errno));
    if (fsize > UINT16_MAX - opd->a09->pc)
      return message(opd->a09,MSG_ERROR,"%s: file too big",filename.buf);
    opd->data   = true;
    opd->datasz = fsize;
    fclose(fp);
  }
  else
  {
    char   buffer[BUFSIZ];
    size_t bsz;
    
    fp = fopen(filename.buf,"rb");
    if (fp == NULL)
      return message(opd->a09,MSG_ERROR,"%s: '%s'",filename.buf,strerror(errno));
      
    do
    {
      bsz = fread(buffer,1,sizeof(buffer),fp);
      if (ferror(fp))
        return message(opd->a09,MSG_ERROR,"%s: failed reading",filename.buf);
      if (!fill)
      {
        opd->sz   = min(bsz,sizeof(opd->bytes));
        opd->data = true;
        fill      = true;
        memcpy(opd->bytes,buffer,opd->sz);
      }
      
      fwrite(buffer,1,bsz,opd->a09->out);
      if (ferror(opd->a09->out))
        return message(opd->a09,MSG_ERROR,"failed writing objectfile");
      opd->datasz += bsz;
    } while (bsz > 0);
    fclose(fp);
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_extdp(struct opcdata *opd)
{
  struct symbol *sym;
  label          label;
  char           c;
  
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return message(opd->a09,MSG_ERROR,"EXTDP missing label");
    opd->buffer->ridx--;
    if (!parse_label(&label,opd->buffer,opd->a09))
      return false;
    sym = symbol_add(opd->a09,&label,0x80);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"out of memory");
    sym->type = SYM_EXTERN;
    sym->bits = 8;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_extern(struct opcdata *opd)
{
  struct symbol *sym;
  label          label;
  char           c;
  
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return message(opd->a09,MSG_ERROR,"EXTERN missing label");
    opd->buffer->ridx--;
    if (!parse_label(&label,opd->buffer,opd->a09))
      return false;
    sym = symbol_add(opd->a09,&label,0x8000);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"out of memory");
    sym->type = SYM_EXTERN;
    sym->bits = 16;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_public(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    struct symbol *sym = symbol_find(opd->a09,&opd->label);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"missing label or expression for PUBLIC");
    sym->type = SYM_PUBLIC;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo__code(struct opcdata *opd)
{
  (void)opd;
  message(opd->a09,MSG_WARNING,".CODE not finished");
  return true;
}

/**************************************************************************/

static bool pseudo__dp(struct opcdata *opd)
{
  (void)opd;
  message(opd->a09,MSG_WARNING,".DP not finished");
  return true;
}

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
  static struct opcode const opcodes[] =
  {
    { ".CODE"   , pseudo__code   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { ".DP"     , pseudo__dp     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ABX"     , op_inh         , { 0x3A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ADCA"    , op_idie        , { 0x89 , 0x99 , 0xA9 , 0xB9 } , 0x00 , false } ,
    { "ADCB"    , op_idie        , { 0xC9 , 0xD9 , 0xE9 , 0xF9 } , 0x00 , false } ,
    { "ADDA"    , op_idie        , { 0x8B , 0x9B , 0xAB , 0xBB } , 0x00 , false } ,
    { "ADDB"    , op_idie        , { 0xCB , 0xDB , 0xEB , 0xFB } , 0x00 , false } ,
    { "ADDD"    , op_idie        , { 0xC3 , 0xD3 , 0xE3 , 0xF3 } , 0x00 , true  } ,
    { "ANDA"    , op_idie        , { 0x84 , 0x94 , 0xA4 , 0xB4 } , 0x00 , false } ,
    { "ANDB"    , op_idie        , { 0xC4 , 0xD4 , 0xE4 , 0xF4 } , 0x00 , false } ,
    { "ANDCC"   , op_imm         , { 0x1C , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASCII"   , pseudo_ascii   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASCIIH"  , pseudo_asciih  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASCIIZ"  , pseudo_asciiz  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASL"     , op_die         , { 0x00 , 0x08 , 0x68 , 0x78 } , 0x00 , false } ,
    { "ASLA"    , op_inh         , { 0x48 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASLB"    , op_inh         , { 0x58 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASR"     , op_die         , { 0x00 , 0x07 , 0x67 , 0x77 } , 0x00 , false } ,
    { "ASRA"    , op_inh         , { 0x47 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ASRB"    , op_inh         , { 0x57 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BCC"     , op_br          , { 0x24 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BCS"     , op_br          , { 0x25 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BEQ"     , op_br          , { 0x27 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BGE"     , op_br          , { 0x2C , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BGT"     , op_br          , { 0x2E , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BHI"     , op_br          , { 0x22 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BHS"     , op_br          , { 0x24 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BITA"    , op_idie        , { 0x85 , 0x95 , 0xA5 , 0xB5 } , 0x00 , false } ,
    { "BITB"    , op_idie        , { 0xC5 , 0xD5 , 0xE5 , 0xF5 } , 0x00 , false } ,
    { "BLE"     , op_br          , { 0x2F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BLO"     , op_br          , { 0x25 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BLS"     , op_br          , { 0x23 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BLT"     , op_br          , { 0x2D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BMI"     , op_br          , { 0x2B , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BNE"     , op_br          , { 0x26 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BPL"     , op_br          , { 0x2A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BRA"     , op_br          , { 0x20 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BRN"     , op_br          , { 0x21 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BSR"     , op_br          , { 0x8D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BVC"     , op_br          , { 0x28 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "BVS"     , op_br          , { 0x29 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "CLR"     , op_die         , { 0x00 , 0x0F , 0x6F , 0x7F } , 0x00 , false } ,
    { "CLRA"    , op_inh         , { 0x4F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "CLRB"    , op_inh         , { 0x5F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "CMPA"    , op_idie        , { 0x81 , 0x91 , 0xA1 , 0xB1 } , 0x00 , false } ,
    { "CMPB"    , op_idie        , { 0xC1 , 0xD1 , 0xE1 , 0xF1 } , 0x00 , false } ,
    { "CMPD"    , op_idie        , { 0x83 , 0x93 , 0xA3 , 0xB3 } , 0x10 , true  } ,
    { "CMPS"    , op_idie        , { 0x8C , 0x9C , 0xAC , 0xBC } , 0x11 , true  } ,
    { "CMPU"    , op_idie        , { 0x83 , 0x93 , 0xA3 , 0xB3 } , 0x11 , true  } ,
    { "CMPX"    , op_idie        , { 0x8C , 0x9C , 0xAC , 0xBC } , 0x00 , true  } ,
    { "CMPY"    , op_idie        , { 0x8C , 0x9C , 0xAC , 0xBC } , 0x10 , true  } ,
    { "COM"     , op_die         , { 0x00 , 0x03 , 0x63 , 0x73 } , 0x00 , false } ,
    { "COMA"    , op_inh         , { 0x43 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "COMB"    , op_inh         , { 0x53 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "CWAI"    , op_imm         , { 0x3C , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "DAA"     , op_inh         , { 0x19 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "DEC"     , op_die         , { 0x00 , 0x0A , 0x6A , 0x7A } , 0x00 , false } ,
    { "DECA"    , op_inh         , { 0x4A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "DECB"    , op_inh         , { 0x5A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "END"     , pseudo_end     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "EORA"    , op_idie        , { 0x88 , 0x98 , 0xA8 , 0xB8 } , 0x00 , false } ,
    { "EORB"    , op_idie        , { 0xC8 , 0xD8 , 0xE8 , 0xF8 } , 0x00 , false } ,
    { "EQU"     , pseudo_equ     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "EXG"     , op_exg         , { 0x1E , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "EXTDP"   , pseudo_extdp   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "EXTERN"  , pseudo_extern  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "FCB"     , pseudo_fcb     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "FCC"     , pseudo_fcc     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "FCS"     , pseudo_asciih  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "FDB"     , pseudo_fdb     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "INC"     , op_die         , { 0x00 , 0x0C , 0x6C , 0x7C } , 0x00 , false } ,
    { "INCA"    , op_inh         , { 0x4C , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "INCB"    , op_inh         , { 0x5C , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "INCBIN"  , pseudo_incbin  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "INCLUDE" , pseudo_include , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "JMP"     , op_idie        , { 0x00 , 0x0E , 0x6E , 0x7E } , 0x00 , false } ,
    { "JSR"     , op_idie        , { 0x00 , 0x9D , 0xAD , 0xBD } , 0x00 , false } ,
    { "LBCC"    , op_lbr         , { 0x24 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBCS"    , op_lbr         , { 0x25 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBEQ"    , op_lbr         , { 0x27 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBGE"    , op_lbr         , { 0x2C , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBGT"    , op_lbr         , { 0x2E , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBHI"    , op_lbr         , { 0x22 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBHS"    , op_lbr         , { 0x24 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBLE"    , op_lbr         , { 0x2F , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBLO"    , op_lbr         , { 0x25 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBLS"    , op_lbr         , { 0x23 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBLT"    , op_lbr         , { 0x2D , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBMI"    , op_lbr         , { 0x2B , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBNE"    , op_lbr         , { 0x26 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBPL"    , op_lbr         , { 0x2A , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBRA"    , op_lbr         , { 0x16 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
    { "LBRN"    , op_lbr         , { 0x21 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBSR"    , op_lbr         , { 0x17 , 0x00 , 0x00 , 0x00 } , 0x00 , true  } ,
    { "LBVC"    , op_lbr         , { 0x28 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LBVS"    , op_lbr         , { 0x29 , 0x00 , 0x00 , 0x00 } , 0x10 , true  } ,
    { "LDA"     , op_idie        , { 0x86 , 0x96 , 0xA6 , 0xB6 } , 0x00 , false } ,
    { "LDB"     , op_idie        , { 0xC6 , 0xD6 , 0xE6 , 0xF6 } , 0x00 , false } ,
    { "LDD"     , op_idie        , { 0xCC , 0xDC , 0xEC , 0xFC } , 0x00 , true  } ,
    { "LDS"     , op_idie        , { 0xCE , 0xDE , 0xEE , 0xFE } , 0x10 , true  } ,
    { "LDU"     , op_idie        , { 0xCE , 0xDE , 0xEE , 0xFE } , 0x00 , true  } ,
    { "LDX"     , op_idie        , { 0x8E , 0x9E , 0xAE , 0xBE } , 0x00 , true  } ,
    { "LDY"     , op_idie        , { 0x8E , 0x9E , 0xAE , 0xBE } , 0x10 , true  } ,
    { "LEAS"    , op_lea         , { 0x00 , 0x00 , 0x32 , 0x00 } , 0x00 , true  } ,
    { "LEAU"    , op_lea         , { 0x00 , 0x00 , 0x33 , 0x00 } , 0x00 , true  } ,
    { "LEAX"    , op_lea         , { 0x00 , 0x00 , 0x30 , 0x00 } , 0x00 , true  } ,
    { "LEAY"    , op_lea         , { 0x00 , 0x00 , 0x31 , 0x00 } , 0x00 , true  } ,
    { "LSL"     , op_die         , { 0x00 , 0x08 , 0x68 , 0x78 } , 0x00 , false } ,
    { "LSLA"    , op_inh         , { 0x48 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "LSLB"    , op_inh         , { 0x58 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "LSR"     , op_die         , { 0x00 , 0x04 , 0x64 , 0x74 } , 0x00 , false } ,
    { "LSRA"    , op_inh         , { 0x44 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "LSRB"    , op_inh         , { 0x54 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "MUL"     , op_inh         , { 0x3D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "NEG"     , op_die         , { 0x00 , 0x00 , 0x60 , 0x70 } , 0x00 , false } ,
    { "NEGA"    , op_inh         , { 0x40 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "NEGB"    , op_inh         , { 0x50 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "NOP"     , op_inh         , { 0x12 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ORA"     , op_idie        , { 0x8A , 0x9A , 0xAA , 0xBA } , 0x00 , false } ,
    { "ORB"     , op_idie        , { 0xCA , 0xDA , 0xEA , 0xFA } , 0x00 , false } ,
    { "ORCC"    , op_imm         , { 0x1A , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ORG"     , pseudo_org     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "PSHS"    , op_pshpul      , { 0x34 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "PSHU"    , op_pshpul      , { 0x36 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "PUBLIC"  , pseudo_public  , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "PULS"    , op_pshpul      , { 0x35 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "PULU"    , op_pshpul      , { 0x37 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "RMB"     , pseudo_rmb     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ROL"     , op_die         , { 0x00 , 0x09 , 0x69 , 0x79 } , 0x00 , false } ,
    { "ROLA"    , op_inh         , { 0x49 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ROLB"    , op_inh         , { 0x59 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "ROR"     , op_die         , { 0x00 , 0x06 , 0x66 , 0x76 } , 0x00 , false } ,
    { "RORA"    , op_inh         , { 0x46 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "RORB"    , op_inh         , { 0x56 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "RTI"     , op_inh         , { 0x3B , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "RTS"     , op_inh         , { 0x39 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "SBCA"    , op_idie        , { 0x82 , 0x92 , 0xA2 , 0xB2 } , 0x00 , false } ,
    { "SBCB"    , op_idie        , { 0xC2 , 0xD2 , 0xE2 , 0xF2 } , 0x00 , false } ,
    { "SET"     , pseudo_set     , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "SETDP"   , pseudo_setdp   , { 0x00 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "SEX"     , op_inh         , { 0x1D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "STA"     , op_die         , { 0x00 , 0x97 , 0xA7 , 0xB7 } , 0x00 , false } ,
    { "STB"     , op_die         , { 0x00 , 0xD7 , 0xE7 , 0xF7 } , 0x00 , false } ,
    { "STD"     , op_die         , { 0x00 , 0xDD , 0xED , 0xFD } , 0x00 , true  } ,
    { "STS"     , op_die         , { 0x00 , 0xDF , 0xEF , 0xFF } , 0x10 , true  } ,
    { "STU"     , op_die         , { 0x00 , 0xDF , 0xEF , 0xFF } , 0x00 , true  } ,
    { "STX"     , op_die         , { 0x00 , 0x9F , 0xAF , 0xBF } , 0x00 , true  } ,
    { "STY"     , op_die         , { 0x00 , 0x9F , 0xAF , 0xBF } , 0x10 , true  } ,
    { "SUBA"    , op_idie        , { 0x80 , 0x90 , 0xA0 , 0xB0 } , 0x00 , false } ,
    { "SUBB"    , op_idie        , { 0xC0 , 0xD0 , 0xE0 , 0xF0 } , 0x00 , false } ,
    { "SUBD"    , op_idie        , { 0x83 , 0x93 , 0xA3 , 0xB3 } , 0x00 , true  } ,
    { "SWI"     , op_inh         , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "SWI2"    , op_inh         , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x10 , false } ,
    { "SWI3"    , op_inh         , { 0x3F , 0x00 , 0x00 , 0x00 } , 0x11 , false } ,
    { "SYNC"    , op_inh         , { 0x13 , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "TFR"     , op_exg         , { 0x1F , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "TST"     , op_die         , { 0x00 , 0x0D , 0x6D , 0x7D } , 0x00 , false } ,
    { "TSTA"    , op_inh         , { 0x4D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
    { "TSTB"    , op_inh         , { 0x5D , 0x00 , 0x00 , 0x00 } , 0x00 , false } ,
  };
  
  assert(name != NULL);
  
  return bsearch(
        name,
        opcodes,
        sizeof(opcodes)/sizeof(opcodes[0]),
        sizeof(opcodes[0]),
        opcode_cmp
  );
}

/**************************************************************************/
