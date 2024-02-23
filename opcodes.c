/****************************************************************************
*
*   Process each opcode and pseudo-opcode
*   Copyright (C) 2023 Sean Conner
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Comments, questions and criticisms can be sent to: sean@conman.org
*
****************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>

#include "a09.h"

/**************************************************************************/

static unsigned char value_5bit(struct a09 *a09,uint16_t value,int pass)
{
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  
  if ((pass == 2) && (value >= 16) && (value <= 65519))
    message(a09,MSG_WARNING,"W0003: 16-bit value truncated to 5 bits");
  return value & 31;
}

/**************************************************************************/

static unsigned char value_lsb(struct a09 *a09,uint16_t value,int pass)
{
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  
  if ((pass == 2) && (value >= 256) && (value <= 65407))
    message(a09,MSG_WARNING,"W0004: 16-bit value truncated to 8 bits");
    
  return value & 255;
}

/**************************************************************************/

static bool collect_string(
        struct a09              *a09,
        struct buffer *restrict  wbuf,
        struct buffer *restrict  rbuf,
        char                     delim
)
{
  char c;
  
  assert(a09  != NULL);
  assert(wbuf != NULL);
  assert(rbuf != NULL);
  assert(isgraph(delim));
  
  memset(wbuf->buf,0,sizeof(wbuf->buf));
  wbuf->widx = 0;
  wbuf->ridx = 0;
  
  while((c = rbuf->buf[rbuf->ridx++]) != delim)
  {
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0013: unexpected end of string");
    wbuf->buf[wbuf->widx++] = c;
    assert(wbuf->widx < sizeof(wbuf->buf));
  }
  
  return true;
}

/**************************************************************************/

bool collect_esc_string(
        struct a09              *a09,
        struct buffer *restrict  wbuf,
        struct buffer *restrict  rbuf,
        char                     delim
)
{
  char c;
  
  assert(a09  != NULL);
  assert(wbuf != NULL);
  assert(rbuf != NULL);
  assert((delim == '"') || (delim == '\''));
  
  memset(wbuf->buf,0,sizeof(wbuf->buf));
  wbuf->widx = 0;
  wbuf->ridx = 0;
  
  while((c = rbuf->buf[rbuf->ridx++]) != delim)
  {
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0013: unexpected end of string");
    if (c == '\\')
    {
      c = rbuf->buf[rbuf->ridx++];
      switch(c)
      {
        case '"':  c = '"';    break;
        case '\'': c = '\'';   break;
        case '\\': c = '\\';   break;
        case 'a':  c = '\a';   break;
        case 'b':  c = '\b';   break;
        case 'e':  c = '\033'; break;
        case 'f':  c = '\f';   break;
        case 'n':  c = '\n';   break;
        case 'r':  c = '\r';   break;
        case 't':  c = '\t';   break;
        case 'v':  c = '\v';   break;
        case '\0': return message(a09,MSG_ERROR,"E0013: unexpected end of string");
        default:   return message(a09,MSG_ERROR,"E0014: invalid escape character");
      }
    }
    
    wbuf->buf[wbuf->widx++] = c;
    assert(wbuf->widx < sizeof(wbuf->buf));
  }
  
  c = toupper(rbuf->buf[rbuf->ridx]);
  switch(c)
  {
    case 'C':
         assert(wbuf->widx < sizeof(wbuf->buf));
         if (wbuf->widx > UCHAR_MAX)
           return message(a09,MSG_ERROR,"E0071: string too long");
         memmove(&wbuf->buf[1],&wbuf->buf[0],wbuf->widx);
         wbuf->buf[0] = (char)wbuf->widx;
         wbuf->widx++;
         rbuf->ridx++;
         break;
         
    case 'H':
         wbuf->buf[wbuf->widx-1] |= 0x80;
         rbuf->ridx++;
         break;
         
    case 'Z':
         assert(wbuf->widx < sizeof(wbuf->buf));
         wbuf->buf[wbuf->widx++] = '\0';
         rbuf->ridx++;
         break;
         
    default:
         break;
  }
  
  return true;
}

/**************************************************************************/

bool parse_string(
        struct a09              *a09,
        struct buffer *restrict  wbuf,
        struct buffer *restrict  rbuf
)
{
  assert(a09  != NULL);
  assert(wbuf != NULL);
  assert(rbuf != NULL);
  
  char c = skip_space(rbuf);
  
  if ((c == '"') || (c == '\''))
    return collect_esc_string(a09,wbuf,rbuf,c);
  else
    return message(a09,MSG_ERROR,"E0015: not a string");
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
      
    c = skip_space(opd->buffer);
    if ((c != ';') && (c != '\0'))
    {
      if (c == ',')
        return message(opd->a09,MSG_ERROR,"E0059: syntax error---mixing up immedate and indexed modes?");
      else
        return message(opd->a09,MSG_ERROR,"E0060: syntax error");
    }
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
             default: return message(opd->a09,MSG_ERROR,"E0016: invalid index register");
           }
           
           if (opd->buffer->buf[++opd->buffer->ridx] == ']')
           {
             if (!indexindirect)
               return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
             opd->value.postbyte |= 0x10;
           }
           return true;
           
      default:
           return message(opd->a09,MSG_ERROR,"E0018: invalid index register");
    }
    
    c = skip_space(opd->buffer);
    
    if ((c == ';') || (c == '\0') || isspace(c))
    {
      if (indexindirect)
        return message(opd->a09,MSG_ERROR,"E0019: missing end of index indirect mode");
      opd->value.postbyte |= 0x04;
      return true;
    }
    else if (c == ']')
    {
      if (!indexindirect)
        return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
      opd->value.postbyte |= 0x14;
      return true;
    }
    else if (c != '+')
      return message(opd->a09,MSG_ERROR,"E0020: syntax error in post-increment index mode");
      
    c = opd->buffer->buf[opd->buffer->ridx++];
    if (c == '+')
    {
      opd->value.postbyte |= 0x01;
      if (opd->buffer->buf[opd->buffer->ridx++] == ']')
      {
        if (!indexindirect)
          return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
        opd->value.postbyte |= 0x10;
      }
      return true;
    }
    else if ((c == ';') || (c == '\0') || isspace(c))
    {
      if (indexindirect)
        return message(opd->a09,MSG_ERROR,"E0019: missing end of index indirect mode");
      return true;
    }
    else
      return message(opd->a09,MSG_ERROR,"E0021: syntax error in index mode");
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
             default: return message(opd->a09,MSG_ERROR,"E0018: invalid index register");
           }
           c = opd->buffer->buf[++opd->buffer->ridx];
           if (c == ']')
           {
             if (!indexindirect)
               return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
             opd->value.postbyte |= 0x10;
             c = opd->buffer->buf[++opd->buffer->ridx];
           }
           
           if ((c == ';') || (c == '\0') || isspace(c))
             return true;
             
           return message(opd->a09,MSG_ERROR,"E0022: invalid accumulator register in index mode");
           
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
      return message(opd->a09,MSG_ERROR,"E0019: missing end of index indirect mode");
      
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
    
    if (opd->value.unknownpass1)
    {
      opd->mode = AM_EXTENDED;
      if (opd->pass == 2)
      {
        if ((opd->value.value >> 8) == opd->a09->dp)
          message(opd->a09,MSG_WARNING,"W0005: address could be 8-bits, maybe use '<'?");
      }
      return true;
    }
    
    if (opd->value.defined && ((opd->value.value >> 8) == opd->a09->dp))
      opd->mode = AM_DIRECT;
    else
      opd->mode = AM_EXTENDED;
    return true;
  }
  
  if (c == ']')
  {
    if (!indexindirect)
      return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
      
    opd->value.postbyte = 0x9F;
    opd->value.bits     = 16;
    opd->mode           = AM_INDEX;
    return true;
  }
  
  if (c != ',')
    return message(opd->a09,MSG_ERROR,"E0023: missing expected comma");
    
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
           if (indexindirect)
           {
             opd->value.postbyte |= 0x88;
             opd->bits            = 8;
             if (opd->pass == 2)
               message(opd->a09,MSG_WARNING,"W0011: 5-bit offset upped to 8 bits for indirect mode");
           }
           else
           {
             opd->value.postbyte |= value_5bit(opd->a09,opd->value.value,opd->pass);
             opd->bits            = 5;
           }
         }
         else if (opd->value.bits == 8)
         {
           opd->value.postbyte |= 0x88;
           opd->bits            = 8;
         }
         else if (opd->value.bits == 16)
         {
           opd->value.postbyte |= 0x89;
           opd->bits            = 16;
         }
         else
         {
           assert(opd->value.bits == 0);
           if (opd->value.unknownpass1)
           {
             opd->value.postbyte |= 0x89;
             opd->bits            = 16;
             if (opd->pass == 2)
             {
               if ((opd->value.value < 16) || (opd->value.value > 65519))
               {
                 if (indexindirect)
                   message(opd->a09,MSG_WARNING,"W0007: offset could be 8-bits, maybe use '<'?");
                 else
                   message(opd->a09,MSG_WARNING,"W0006: offset could be 5-bits, maybe use '<<'?");
               }
               else if ((opd->value.value < 0x80) || (opd->value.value > 0xFF7F))
                 message(opd->a09,MSG_WARNING,"W0007: offset could be 8-bits, maybe use '<'?");
             }
           }
           else if ((opd->value.value < 16) || (opd->value.value > 65519))
           {
             /*------------------------------------------------------------
             ; if the bit size is unspecified, and we have a 0 offset, use
             ; the ',reg' mode instead of '0,reg'---it's the same size, but
             ; executes one cycle faster.  If you want '0,reg', then use a
             ; size override on the index.
             ;-------------------------------------------------------------*/
             
             if (opd->value.value == 0)
             {
               opd->value.postbyte |= 0x84;
               opd->bits            = 0;
             }
             else
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
         }
         break;
         
    case 'P':
         if (toupper(opd->buffer->buf[opd->buffer->ridx]) != 'C')
           return message(opd->a09,MSG_ERROR,"E0016: invalid index register");
         opd->pcrel = true;
         opd->buffer->ridx++;
         
         if (opd->value.bits == 0)
         {
           uint16_t pc    = opd->a09->pc + 2 + (opd->op->page != 0);
           uint16_t delta = opd->value.value - pc;
           
           if (opd->value.unknownpass1)
           {
             opd->value.postbyte = 0x8D;
             opd->bits           = 16;
             if (opd->pass == 2)
             {
               if ((delta < 0x80) || (delta > 0xFF7F))
                 message(opd->a09,MSG_WARNING,"W0007: offset could be 8-bits, maybe use '<'?");
             }
           }
           else
           {
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
         return message(opd->a09,MSG_ERROR,"E0016: invalid index register");
  }
  
  c = skip_space(opd->buffer);
  if (c == ']')
  {
    if (!indexindirect)
      return message(opd->a09,MSG_ERROR,"E0017: end of indirection without start of indirection error");
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
    return message(opd->a09,MSG_ERROR,"E0024: operands found for op with no operands");
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  opd->bytes[opd->sz++] = opd->op->opcode;
  return true;
}

/**************************************************************************/

static bool finish_index_bytes(struct opcdata *opd)
{
  assert(opd       != NULL);
  assert(opd->mode == AM_INDEX);
  
  unsigned char opcode = opd->op->opcode;
  
  if (opcode < 0x30)
    opcode += 0x60;
  else if (opcode >= 0x80)
    opcode += 0x20;
    
  opd->bytes[opd->sz++] = opcode;
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
         opd->bytes[opd->sz++] = opd->op->opcode;
         if (opd->op->bit16)
         {
           opd->bytes[opd->sz++] = opd->value.value >> 8;
           opd->bytes[opd->sz++] = opd->value.value &  255;
         }
         else
           opd->bytes[opd->sz++] = value_lsb(opd->a09,opd->value.value,opd->pass);
         return true;
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode  + 0x10;
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INDEX:
         return finish_index_bytes(opd);
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode  +  0x30;
         opd->bytes[opd->sz++] = opd->value.value >> 8;
         opd->bytes[opd->sz++] = opd->value.value &  255;
         return true;
         
    case AM_INHERENT:
    case AM_BRANCH:
         return message(opd->a09,MSG_ERROR,"E0025: instruction not inherent");
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
         return message(opd->a09,MSG_ERROR,"E0026: immediate mode not supported for opcode");
         
    case AM_DIRECT:
         opd->bytes[opd->sz++] = opd->op->opcode  + ((opd->op->opcode < 0x80) ? 0x00 : 0x10);
         opd->bytes[opd->sz++] = opd->value.value & 255;
         return true;
         
    case AM_INDEX:
         return finish_index_bytes(opd);
         
    case AM_EXTENDED:
         opd->bytes[opd->sz++] = opd->op->opcode  +  ((opd->op->opcode < 0x80) ? 0x70 : 0x30);
         opd->bytes[opd->sz++] = opd->value.value >> 8;
         opd->bytes[opd->sz++] = opd->value.value &  255;
         return true;
         
    case AM_INHERENT:
    case AM_BRANCH:
         return message(opd->a09,MSG_ERROR,"E0027: Internal error---how did this happen?");
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
    return message(opd->a09,MSG_ERROR,"E0028: instruction only supports immediate mode");
    
  assert(opd->op->page == 0);
  opd->bytes[opd->sz++] = opd->op->opcode;
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
  
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
    
  if (opd->value.external)
  {
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = 0;
  }
  else
  {
    uint16_t delta = opd->value.value - (opd->a09->pc + 2);
    
    if ((opd->pass == 2) && (opd->op->opcode != 0x21)) /* BRN is exempted */
    {
      if (delta == 0)
        message(opd->a09,MSG_WARNING,"W0012: branch to next location, maybe remove?");
      else if ((delta > 0x007F) && (delta < 0xFF80))
        return message(opd->a09,MSG_ERROR,"E0029: target exceeds 8-bit range");
    }
    
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = delta & 255;
  }
  
  opd->mode  = AM_BRANCH;
  opd->pcrel = true;
  return true;
}

/**************************************************************************/

static bool op_lbr(struct opcdata *opd)
{
  assert(opd             != NULL);
  assert(opd->a09        != NULL);
  assert(opd->sz         == 0);
  
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
    
  if (opd->value.external)
  {
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = 0;
    opd->bytes[opd->sz++] = 0;
  }
  else
  {
    uint16_t delta = opd->value.value - (opd->a09->pc + (opd->op->page ? 4 : 3));
    
    if ((opd->pass == 2) && (opd->op->opcode != 0x21))
    {
      if (delta == 0)
        message(opd->a09,MSG_WARNING,"W0012: branch to next location, maybe remove?");
      else if ((delta < 0x80) || (delta > 0xFF80))
        message(opd->a09,MSG_WARNING,"W0009: offset could be 8-bits, maybe use short branch?");
    }
    
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = delta >> 8;
    opd->bytes[opd->sz++] = delta & 255;
  }
  
  opd->mode  = AM_BRANCH;
  opd->pcrel = true;
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
    return message(opd->a09,MSG_ERROR,"E0030: bad operand");
    
  if (opd->op->page)
    opd->bytes[opd->sz++] = opd->op->page;
  return finish_index_bytes(opd);
}

/**************************************************************************/

static bool sop_findreg(struct indexregs const **ir,char const *src,char skip)
{
  static struct indexregs const index[] =
  {
    { "\2PC" , 0x80 , 0x50 , 0x05 , WORD } ,
    { "\1S"  , 0x40 , 0x40 , 0x04 , WORD } ,
    { "\1U"  , 0x40 , 0x30 , 0x03 , WORD } ,
    { "\1Y"  , 0x20 , 0x20 , 0x02 , WORD } ,
    { "\1X"  , 0x10 , 0x10 , 0x01 , WORD } ,
    { "\2DP" , 0x08 , 0xB0 , 0x0B , BYTE } ,
    { "\1B"  , 0x04 , 0x90 , 0x09 , BYTE } ,
    { "\1A"  , 0x02 , 0x80 , 0x08 , BYTE } ,
    { "\2CC" , 0x01 , 0xA0 , 0x0A , BYTE } ,
    { "\1D"  , 0x06 , 0x00 , 0x00 , WORD } ,
  };
  
  char what[3] = { toupper(src[0]) , toupper(src[1]) , '\0' };
  
  for (size_t i = 0 ; i < sizeof(index) / sizeof(index[0]) ; i++)
  {
    if (memcmp(what,&index[i].reg[1],index[i].reg[0]) == 0)
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
    
  if (c != '-')
  {
    opd->buffer->ridx--;
    
    while(!isspace(c) && (c != ';') && (c != '0'))
    {
      struct indexregs const *reg;
      
      if (!sop_findreg(&reg,&opd->buffer->buf[opd->buffer->ridx],skip))
        return message(opd->a09,MSG_ERROR,"E0031: bad register name for PSH/PUL");
      opd->buffer->ridx += reg->reg[0];
      operand |= reg->pushpull;
      c = skip_space(opd->buffer);
      if (c == '\0')
        break;
      if (c == ';')
        break;
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"E0032: missing comma in register list");
    }
  }
    
  opd->bytes[opd->sz++] = opd->op->opcode;
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
    return message(opd->a09,MSG_ERROR,"E0033: bad register name for EXG/TFR");
  opd->buffer->ridx += reg1->reg[0];
  operand |= reg1->tehi;
  if (opd->buffer->buf[opd->buffer->ridx] != ',')
    return message(opd->a09,MSG_ERROR,"E0034: missing comma");
  opd->buffer->ridx++;
  
  if (!sop_findreg(&reg2,&opd->buffer->buf[opd->buffer->ridx],'\0'))
    return message(opd->a09,MSG_ERROR,"E0033: bad register name for EXG/TFR");
  if ((opd->pass == 1) && (reg1->bit16 != reg2->bit16))
    message(opd->a09,MSG_WARNING,"W0008: ext/tfr mixed sized registers");
  operand |= reg2->telo;
  
  opd->bytes[opd->sz++] = opd->op->opcode;
  opd->bytes[opd->sz++] = operand;
  return true;
}

/**************************************************************************/

static bool pseudo_equ(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"E0062: missing value for EQU");
    
  if (opd->pass == 1)
  {
    struct symbol *sym = symbol_find(opd->a09,&opd->label);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"E0035: missing label for EQU");
    if (sym->type != SYM_ADDRESS)
      return message(opd->a09,MSG_ERROR,"E0036: trying to EQU a SET value");
      
    sym->value = opd->value.value;
    sym->type  = SYM_EQU;
    sym->bits  = opd->value.value < 256 ? 8 : 16;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_set(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"E0063: missing value for SET");
    
  struct symbol *sym = symbol_find(opd->a09,&opd->label);
  
  if (opd->pass == 1)
  {
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"E0037: missing label for SET");
    assert(opd->a09->lnum == sym->ldef);
    sym->type = SYM_SET;
  }
  else
  {
    assert(sym       != NULL);
    assert(sym->type == SYM_SET);
    sym->ldef = opd->a09->lnum;
  }
  
  sym->value = opd->value.value;
  sym->bits  = opd->value.value < 256 ? 8 : 16;
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
  if (opd->a09->obj)
    return opd->a09->format.rmb(&opd->a09->format,opd);
  else
    return true;
}

/**************************************************************************/

static bool pseudo_org(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (!parse_dirext(opd))
    return message(opd->a09,MSG_ERROR,"E0039: missing value for ORG");
    
  if (opd->a09->obj)
    return opd->a09->format.org(&opd->a09->format,opd);
  else
  {
    opd->a09->pc = opd->value.value;
    return true;
  }
}

/**************************************************************************/

static bool pseudo_setdp(struct opcdata *opd)
{
  assert(opd != NULL);
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
  opd->a09->dp = value_lsb(opd->a09,opd->value.value,opd->pass);
  return opd->a09->format.setdp(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo_end(struct opcdata *opd)
{
  struct symbol *sym;
  label          label;
  char           c;
  
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  c = skip_space(opd->buffer);
  if ((c == ';') || (c == '\0'))
    return true;
    
  opd->buffer->ridx--;
  
  if (!parse_label(&label,opd->buffer,opd->a09,opd->pass))
    return message(opd->a09,MSG_ERROR,"E0050: not a label");
  sym = symbol_find(opd->a09,&label);
  if (sym == NULL)
    return message(opd->a09,MSG_ERROR,"E0052: missing label for END");
  if (opd->pass == 2)
    sym->refs++;
  return opd->a09->format.end(&opd->a09->format,opd,sym);
}

/**************************************************************************/

static bool pseudo_fcb(struct opcdata *opd)
{
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  opd->data = true;
  
  while(true)
  {
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    opd->datasz++;
    if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
      return false;
      
    if (opd->pass == 2)
    {
      unsigned char byte = value_lsb(opd->a09,opd->value.value,opd->pass);
      if (opd->sz < sizeof(opd->bytes))
        opd->bytes[opd->sz++] = byte;
      if (opd->a09->obj)
      {
        if (!opd->a09->format.data_write(&opd->a09->format,opd,(char *)&byte,1))
          return false;
      }
    }
    
    char c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return true;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"E0034: missing comma");
  }
}

/**************************************************************************/

static bool pseudo_fdb(struct opcdata *opd)
{
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  opd->data = true;
  
  while(true)
  {
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    opd->datasz += 2;
    if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
      return false;
      
    if (opd->pass == 2)
    {
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
      if (opd->a09->obj)
      {
        if (!opd->a09->format.data_write(&opd->a09->format,opd,(char *)word,2))
          return false;
      }
    }
    
    char c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return true;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"E0034: missing comma");
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
    return message(opd->a09,MSG_ERROR,"E0010: unexpected end of input");
  if (!collect_string(opd->a09,&textstring,opd->buffer,c))
    return false;
    
  opd->data   = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (opd->a09->obj)
    {
      if (!opd->a09->format.data_write(&opd->a09->format,opd,textstring.buf,textstring.widx))
        return false;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_ascii(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  
  if (!parse_string(opd->a09,&textstring,opd->buffer))
    return false;
    
  opd->data   = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (opd->a09->obj)
    {
      if (!opd->a09->format.data_write(&opd->a09->format,opd,textstring.buf,textstring.widx))
        return false;
    }
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
  
  if (!parse_string(opd->a09,&filename,opd->buffer))
    return false;
    
  assert(filename.widx < sizeof(filename.buf));
  filename.buf[filename.widx++] = '\0';
  
  new.label  = (label){ .s = 0 , .text = { '\0' } };
  new.inbuf  = (struct buffer){ .buf = {0}, .widx = 0 , .ridx = 0 };
  new.infile = add_file_dep(&new,filename.buf);
  new.in     = fopen(filename.buf,"r");
  
  if (new.in == NULL)
    return message(opd->a09,MSG_ERROR,"E0042: %s: '%s'",filename.buf,strerror(errno));
    
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
  opd->a09->deps   = new.deps;
  opd->a09->ndeps  = new.ndeps;
  return rc;
}

/**************************************************************************/

static bool pseudo_incbin(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct buffer  filename;
  FILE          *fp;
  int            c;
  long           fsize;
  long           start = 0;
  long           len   = 0;
  
  if (!parse_string(opd->a09,&filename,opd->buffer))
    return false;
    
  assert(filename.widx < sizeof(filename.buf));
  filename.buf[filename.widx++] = '\0';
  
  /*-------------------------------------------
  ; check for starting offset and length
  ;--------------------------------------------*/
  
  c = skip_space(opd->buffer);
  if (c == ',')
  {
    struct value v;
    if (!expr(&v,opd->a09,opd->buffer,opd->pass))
      return false;
    if ((opd->pass == 2) && !v.defined)
      return message(opd->a09,MSG_ERROR,"E0094: value not defined");
    start = v.value;
    
    c = skip_space(opd->buffer);
    if (c == ',')
    {
      bool neg = false;
      
      c = skip_space(opd->buffer);
      if (c == '-')
        neg = true;
      else
        opd->buffer->ridx--;
        
      if (!expr(&v,opd->a09,opd->buffer,opd->pass))
        return false;
      if ((opd->pass == 2) && !v.defined)
        return message(opd->a09,MSG_ERROR,"E0094: value not defined");
      if (v.value == 0)
        return message(opd->a09,MSG_ERROR,"E0096: size can't be 0");
      len = v.value;
      if (neg)
        len = -len;
    }
  }
  
  fp = fopen(filename.buf,"rb");
  if (fp == NULL)
    return message(opd->a09,MSG_ERROR,"E0042: %s: '%s'",filename.buf,strerror(errno));
  if (fseek(fp,0,SEEK_END) == -1)
    return message(opd->a09,MSG_ERROR,"E0042: %s: '%s'",filename.buf,strerror(errno));
  fsize = ftell(fp);
  if (fsize < 0)
    return message(opd->a09,MSG_ERROR,"E0042: %s: '%s'",filename.buf,strerror(errno));
    
  if (fsize == 0)
    return message(opd->a09,MSG_ERROR,"E0097: %s: contains no data",filename.buf);
    
  if (start >= fsize)
    return message(opd->a09,MSG_ERROR,"E0095: %s: not enough data",filename.buf);
    
  if (len == 0)
    len = fsize - start;
  else if (len > 0)
  {
    if (len > fsize - start)
      return message(opd->a09,MSG_ERROR,"E0095: %s: not enough data",filename.buf);
  }
  else
  {
    len = (fsize + len) - start; /* len is < 0, so the + */
    if (len <= 0)
      return message(opd->a09,MSG_ERROR,"E0095: %s: not enough data",filename.buf);
  }
  
  if (len > UINT16_MAX - opd->a09->pc)
    return message(opd->a09,MSG_ERROR,"E0043: %s: file too big",filename.buf);
    
  assert(len   > 0);
  assert(start < fsize);
  assert(start + len <= fsize);
  
  opd->data   = true;
  opd->datasz = len;
  
  if (opd->pass == 1)
    add_file_dep(opd->a09,filename.buf);
  else if (opd->pass == 2)
  {
    size_t bsz;
    bool   fill = false;
    
    if (fseek(fp,start,SEEK_SET) == -1)
      return message(opd->a09,MSG_ERROR,"E0042: %s: '%s'",filename.buf,strerror(errno));
      
    do
    {
      assert(len >= 0);
      
      char   buffer[BUFSIZ];
      size_t amount = min(sizeof(buffer),(size_t)len);
      bsz           = fread(buffer,1,amount,fp);
      
      if (ferror(fp))
        return message(opd->a09,MSG_ERROR,"E0044: %s: failed reading",filename.buf);
        
      assert(amount <= (unsigned)len);
      len -= (long)amount;
      
      if (!fill)
      {
        opd->sz   = min(bsz,sizeof(opd->bytes));
        opd->data = true;
        fill      = true;
        memcpy(opd->bytes,buffer,opd->sz);
      }
      
      if (opd->a09->obj)
      {
        if (!opd->a09->format.data_write(&opd->a09->format,opd,buffer,bsz))
          return false;
      }
    } while (bsz > 0);
  }
  
  fclose(fp);
  return true;
}

/**************************************************************************/

static bool pseudo_extdp(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    struct symbol *sym;
    label          label;
    char           c = skip_space(opd->buffer);
    
    if ((c == ';') || (c == '\0'))
      return message(opd->a09,MSG_ERROR,"E0045: EXTDP missing label");
    opd->buffer->ridx--;
    if (!parse_label(&label,opd->buffer,opd->a09,opd->pass))
      return false;
    sym = symbol_find(opd->a09,&label);
    if (sym != NULL)
      return message(opd->a09,MSG_ERROR,"E0064: symbol '%.*s' already defined",sym->name.s,sym->name.text);
    sym = symbol_add(opd->a09,&label,0x80);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"E0046: out of memory");
    sym->type = SYM_EXTERN;
    sym->bits = 8;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo_extern(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
  {
    struct symbol *sym;
    label          label;
    char           c = skip_space(opd->buffer);
    
    if ((c == ';') || (c == '\0'))
      return message(opd->a09,MSG_ERROR,"E0047: EXTERN missing label");
    opd->buffer->ridx--;
    if (!parse_label(&label,opd->buffer,opd->a09,opd->pass))
      return false;
    sym = symbol_find(opd->a09,&label);
    if (sym != NULL)
      return message(opd->a09,MSG_ERROR,"E0064: symbol '%.*s' already defined",sym->name.s,sym->name.text);
    sym = symbol_add(opd->a09,&label,0x8000);
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"E0046: out of memory");
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
      return message(opd->a09,MSG_ERROR,"E0048: missing label or expression for PUBLIC");
    sym->type = SYM_PUBLIC;
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo__code(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
    message(opd->a09,MSG_WARNING,"W9999: FEATURE NOT FINISHED");
  return opd->a09->format.code(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__dp(struct opcdata *opd)
{
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 1)
    message(opd->a09,MSG_WARNING,"W9999: FEATURE NOT FINISHED");
  return opd->a09->format.dp(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo_align(struct opcdata *opd)
{
  uint16_t rem;
  
  assert(opd != NULL);
  if (!expr(&opd->value,opd->a09,opd->buffer,opd->pass))
    return false;
    
  if (opd->value.unknownpass1)
    return message(opd->a09,MSG_ERROR,"E0051: value for ALIGN must be known in pass 1");
    
  if (opd->value.value == 0)
    return message(opd->a09,MSG_ERROR,"E0008: divide by 0 error");
    
  rem = opd->a09->pc % opd->value.value;
  if (rem == 0)
    return true;
    
  opd->data   = true;
  opd->datasz = opd->value.value - rem;
  
  return opd->a09->format.align(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo_fcs(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  
  struct buffer textstring;
  char          c = skip_space(opd->buffer);
  
  if (c == '\0')
    return message(opd->a09,MSG_ERROR,"E0010: unexpected end of input");
  if (!collect_string(opd->a09,&textstring,opd->buffer,c))
    return false;
    
  opd->data   = true;
  opd->datasz = textstring.widx;
  
  if (opd->pass == 2)
  {
    textstring.buf[textstring.widx - 1] |= 0x80;
    opd->sz = min(textstring.widx,sizeof(opd->bytes));
    memcpy(opd->bytes,textstring.buf,opd->sz);
    if (opd->a09->obj)
    {
      if (!opd->a09->format.data_write(&opd->a09->format,opd,textstring.buf,textstring.widx))
        return false;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool pseudo__assert(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return opd->a09->format.Assert(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__endtst(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return opd->a09->format.endtst(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__notest(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return fdefault_test(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__test(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return opd->a09->format.test(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__troff(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return opd->a09->format.troff(&opd->a09->format,opd);
}

/**************************************************************************/

static bool pseudo__tron(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return opd->a09->format.tron(&opd->a09->format,opd);
}

/**************************************************************************/

static void uses_all(tree__s *tree,char const *filename)
{
  assert(filename != NULL);
  
  if (tree != NULL)
  {
    uses_all(tree->left,filename);
    struct symbol *sym = tree2sym(tree);
    if (sym->filename == filename)
      sym->refs++;
    uses_all(tree->right,filename);
  }
}

/**************************************************************************/

static bool pseudo__opt(struct opcdata *opd)
{
  assert(opd      != NULL);
  assert(opd->a09 != NULL);
  assert((opd->pass == 1) | (opd->pass == 2));
  
  label tmp;
  char  c = skip_space(opd->buffer);
  
  if (c != '*')
  {
    opd->buffer->ridx--;
    return opd->a09->format.opt(&opd->a09->format,opd);
  }
  
  c = skip_space(opd->buffer);
  read_label(opd->buffer,&tmp,c);
  upper_label(&tmp);
  
  if ((tmp.s == 4) && (memcmp(tmp.text,"USES",4) == 0))
  {
    if (opd->pass == 2)
    {
      struct symbol *sym;
      
      c = skip_space(opd->buffer);
      
      if (c == '*')
      {
        uses_all(opd->a09->symtab,opd->a09->infile);
        return true;
      }
      
      opd->buffer->ridx--;
      if (!parse_label(&tmp,opd->buffer,opd->a09,opd->pass))
        return false;
      sym = symbol_find(opd->a09,&tmp);
      if (sym == NULL)
        return message(opd->a09,MSG_ERROR,"E0076: label not defined");
      sym->refs++;
    }
    
    return true;
  }
  
  else if ((tmp.s == 7) && (memcmp(tmp.text,"DISABLE",7) == 0))
  {
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    return disable_warning(opd->a09,&opd->buffer->buf[opd->buffer->ridx]);
  }
  
  else if ((tmp.s == 6) && (memcmp(tmp.text,"ENABLE",6) == 0))
  {
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    return enable_warning(opd->a09,&opd->buffer->buf[opd->buffer->ridx]);
  }
  
  else if ((tmp.s == 3) && (memcmp(tmp.text,"OBJ",3) == 0))
  {
    c = skip_space(opd->buffer);
    read_label(opd->buffer,&tmp,c);
    upper_label(&tmp);
    if ((tmp.s == 5) && (memcmp(tmp.text,"FALSE",5) == 0))
      opd->a09->obj = false;
    else if ((tmp.s == 4) && (memcmp(tmp.text,"TRUE",4) == 0))
      opd->a09->obj = true;
    else
      return message(opd->a09,MSG_ERROR,"E0086: boolean value must be 'true' or 'false'");
      
    return true;
  }
  
  else
    return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
}

/**************************************************************************/

static bool ccflags(unsigned char *p,struct a09 *a09,struct buffer *buffer)
{
  assert(p      != NULL);
  assert(buffer != NULL);
  
  unsigned char cc = 0;
  
  while(buffer->buf[buffer->ridx] != '}')
  {
    switch(toupper(buffer->buf[buffer->ridx++]))
    {
      case 'C': cc |= 0x01; break;
      case 'V': cc |= 0x02; break;
      case 'Z': cc |= 0x04; break;
      case 'N': cc |= 0x08; break;
      case 'I': cc |= 0x10; break;
      case 'H': cc |= 0x20; break;
      case 'F': cc |= 0x40; break;
      case 'E': cc |= 0x80; break;
      default: return message(a09,MSG_ERROR,"E0085: unknown flag '%c'",buffer->buf[buffer->ridx-1]);
    }
  }
  
  *p = cc;
  return true;
}

/**************************************************************************/

static bool op_andcc(struct opcdata *opd)
{
  assert(opd           != NULL);
  assert(opd->op->page == 0);
  
  char c = skip_space(opd->buffer);
  if (c == '{')
  {
    unsigned char cc;
    
    if (!ccflags(&cc,opd->a09,opd->buffer))
      return false;
      
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = ~cc;
    return true;
  }
  else
  {
    opd->buffer->ridx--;
    return op_imm(opd);
  }
}

/**************************************************************************/

static bool op_orcc(struct opcdata *opd)
{
  assert(opd           != NULL);
  assert(opd->op->page == 0);
  
  char c = skip_space(opd->buffer);
  if (c == '{')
  {
    unsigned char cc;
    
    if (!ccflags(&cc,opd->a09,opd->buffer))
      return false;
      
    opd->bytes[opd->sz++] = opd->op->opcode;
    opd->bytes[opd->sz++] = cc;
    return true;
  }
  else
  {
    opd->buffer->ridx--;
    return op_imm(opd);
  }
}

/**************************************************************************/

static bool write_single(struct opcdata *opd,float val)
{
  assert(opd       != NULL);
  assert(opd->a09  != NULL);
  assert(opd->pass == 2);
  assert(!opd->a09->fdecb);
  
#if defined(__i386) || defined(__x86_64)
  char le[sizeof(float)];
  char be[sizeof(float)];
  
  memcpy(le,&val,sizeof(val));
  for (size_t i = sizeof(val) ; i > 0 ; i--)
    be[sizeof(val) - i] = le[i - 1];
  for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < sizeof(val)) ; i++ , opd->sz++)
    opd->bytes[opd->sz] = be[i];
  if (opd->a09->obj)
  {
    if (!opd->a09->format.data_write(&opd->a09->format,opd,be,sizeof(be)))
      return false;
  }
  return true;
#else
#  error You need to define the byte order
#endif
}

/**************************************************************************/

static bool write_double(struct opcdata *opd,double val,bool unpacked)
{
  assert(opd       != NULL);
  assert(opd->a09  != NULL);
  assert(opd->pass == 2);
  
  if (opd->a09->fdecb)
  {
    union float_u { double   f; uint64_t i;                   };
    union frac    { uint64_t i; char     c[sizeof(uint64_t)]; };
    
    if (!isnormal(val) && (val != 0.0))
      return message(opd->a09,MSG_ERROR,"E0090: floating point exceeds range of Color Computer");
      
    /*----------------------------------------------------------------------
    ; The IEEE-754 double (which pretty much all systems use these days) is
    ; formatted as:
    ;
    ;           [s:1] [exp:11 (biased by 1023)] [frac:52]
    ;
    ; The floating point format for the Color Computer is:
    ;
    ;           [exp:8 (biased by 129)] [s:1] [frac:31]
    ;
    ; Both assume the floating point fraction has a leading 1 and thus,
    ; it's not part of the actual storage format.  So all we have to do
    ; is kind of massage the bits around a bit.
    ;
    ; One wrinkle in this is the unpacked floating point format used on the
    ; Color Computer.  It's one byte longer:
    ;
    ;           [exp:8 (biased by 129)] [frac:32] [s:8]
    ;
    ; NOTE: The floating point system on the Color Computer doesn't have the
    ;       concepts of +-inf or NaN---those will generate an error.
    ;----------------------------------------------------------------------*/
    
    char          decbfloat[6];
    size_t        dfs  = 5; /* let's default to a packed DECB value */
    union float_u x    = { .f = val };
    union frac    frac = { .i = (x.i & 0x000FFFFFFFFFFFFFuLL) << 11 };
    bool          sign = (x.i >> 63) != 0;
    int           exp  = (int)((x.i >> 52) & 0x7ffuLL);
    
    assert(exp >= 0);
    assert(exp != 0x7FF);
    assert((exp > 0) || ((exp == 0) && (frac.i == 0)));
    
    if (exp > 0)
      exp = exp - 1023 + 129; /* unbias from IEEE-754 to DECB float */
      
    if (exp > 255)
      return message(opd->a09,MSG_ERROR,"E0090: floating point exceeds range of Color Computer");
      
    decbfloat[0] = exp;
#if defined(__i386) || defined(__x86_64)
    for (size_t i = sizeof(uint64_t) ; i > 4 ; i--)
      decbfloat[1 + sizeof(uint64_t) - i] = frac.c[i - 1];
#else
#  error You need to define the byte order
#endif

    if (unpacked)
    {
      dfs++;
      decbfloat[5]  = sign * 255;
      decbfloat[1] |= 0x80;
    }
    else
      decbfloat[1] |= sign ? 0x80 : 0x00;
      
    for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < dfs) ; i++,opd->sz++)
      opd->bytes[opd->sz] = decbfloat[i];
      
    if (opd->a09->obj)
    {
      if (!opd->a09->format.data_write(&opd->a09->format,opd,decbfloat,dfs))
        return false;
    }
  }
  else
  {
#if defined(__i386) || defined(__x86_64)
    char le[sizeof(val)];
    char be[sizeof(val)];
    
    memcpy(le,&val,sizeof(val));
    for (size_t i = sizeof(val) ; i > 0 ; i--)
      be[sizeof(val) - i] = le[i - 1];
    for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < sizeof(val)) ; i++ , opd->sz++)
      opd->bytes[opd->sz] = be[i];
    if (opd->a09->obj)
    {
      if (!opd->a09->format.data_write(&opd->a09->format,opd,be,sizeof(be)))
        return false;
    }
#else
#  error You need to define the byte order
#endif
  }
  return true;
}

/**************************************************************************/

static bool pseudo__float(struct opcdata *opd)
{
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  opd->data = true;
  
  while(true)
  {
    struct fvalue  fv;
    
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    
    if ((opd->op->opcode == 0) && !opd->a09->fdecb)
    {
      if (!rexpr(&fv,opd->a09,opd->buffer,opd->pass,false))
        return false;
      opd->datasz += sizeof(float);
      
      if (opd->pass == 2)
      {
        if (!write_single(opd,fv.value.f))
          return false;
      }
    }
    else
    {
      if (!rexpr(&fv,opd->a09,opd->buffer,opd->pass,true))
        return false;
      if (opd->a09->fdecb)
        opd->datasz += 5;
      else
        opd->datasz += sizeof(double);
      
      if (opd->pass == 2)
      {
        if (!write_double(opd,fv.value.d,opd->op->opcode == 1))
          return false;
      }
    }
    
    char c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return true;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"E0034: missing comma");
  }
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
    { ".ASSERT" , pseudo__assert , 0x00 , 0x00 , false } , // test
    { ".CODE"   , pseudo__code   , 0x00 , 0x00 , false } ,
    { ".DP"     , pseudo__dp     , 0x00 , 0x00 , false } ,
    { ".ENDTST" , pseudo__endtst , 0x00 , 0x00 , false } , // test
    { ".FLOAT"  , pseudo__float  , 0x00 , 0x00 , false } ,
    { ".FLOATD" , pseudo__float  , 0x01 , 0x00 , false } ,
    { ".NOTEST" , pseudo__notest , 0x00 , 0x00 , false } , // test
    { ".OPT"    , pseudo__opt    , 0x00 , 0x00 , false } ,
    { ".TEST"   , pseudo__test   , 0x00 , 0x00 , false } , // test
    { ".TROFF"  , pseudo__troff  , 0x00 , 0x00 , false } , // test
    { ".TRON"   , pseudo__tron   , 0x00 , 0x00 , false } , // test
    { "ABX"     , op_inh         , 0x3A , 0x00 , BYTE  } ,
    { "ADCA"    , op_idie        , 0x89 , 0x00 , BYTE  } ,
    { "ADCB"    , op_idie        , 0xC9 , 0x00 , BYTE  } ,
    { "ADDA"    , op_idie        , 0x8B , 0x00 , BYTE  } ,
    { "ADDB"    , op_idie        , 0xCB , 0x00 , BYTE  } ,
    { "ADDD"    , op_idie        , 0xC3 , 0x00 , WORD  } ,
    { "ALIGN"   , pseudo_align   , 0x00 , 0x00 , false } ,
    { "ANDA"    , op_idie        , 0x84 , 0x00 , BYTE  } ,
    { "ANDB"    , op_idie        , 0xC4 , 0x00 , BYTE  } ,
    { "ANDCC"   , op_andcc       , 0x1C , 0x00 , BYTE  } ,
    { "ASCII"   , pseudo_ascii   , 0x00 , 0x00 , false } ,
    { "ASL"     , op_die         , 0x08 , 0x00 , BYTE  } ,
    { "ASLA"    , op_inh         , 0x48 , 0x00 , BYTE  } ,
    { "ASLB"    , op_inh         , 0x58 , 0x00 , BYTE  } ,
    { "ASR"     , op_die         , 0x07 , 0x00 , BYTE  } ,
    { "ASRA"    , op_inh         , 0x47 , 0x00 , BYTE  } ,
    { "ASRB"    , op_inh         , 0x57 , 0x00 , BYTE  } ,
    { "BCC"     , op_br          , 0x24 , 0x00 , BYTE  } ,
    { "BCS"     , op_br          , 0x25 , 0x00 , BYTE  } ,
    { "BEQ"     , op_br          , 0x27 , 0x00 , BYTE  } ,
    { "BGE"     , op_br          , 0x2C , 0x00 , BYTE  } ,
    { "BGT"     , op_br          , 0x2E , 0x00 , BYTE  } ,
    { "BHI"     , op_br          , 0x22 , 0x00 , BYTE  } ,
    { "BHS"     , op_br          , 0x24 , 0x00 , BYTE  } ,
    { "BITA"    , op_idie        , 0x85 , 0x00 , BYTE  } ,
    { "BITB"    , op_idie        , 0xC5 , 0x00 , BYTE  } ,
    { "BLE"     , op_br          , 0x2F , 0x00 , BYTE  } ,
    { "BLO"     , op_br          , 0x25 , 0x00 , BYTE  } ,
    { "BLS"     , op_br          , 0x23 , 0x00 , BYTE  } ,
    { "BLT"     , op_br          , 0x2D , 0x00 , BYTE  } ,
    { "BMI"     , op_br          , 0x2B , 0x00 , BYTE  } ,
    { "BNE"     , op_br          , 0x26 , 0x00 , BYTE  } ,
    { "BPL"     , op_br          , 0x2A , 0x00 , BYTE  } ,
    { "BRA"     , op_br          , 0x20 , 0x00 , BYTE  } ,
    { "BRN"     , op_br          , 0x21 , 0x00 , BYTE  } ,
    { "BSR"     , op_br          , 0x8D , 0x00 , BYTE  } ,
    { "BVC"     , op_br          , 0x28 , 0x00 , BYTE  } ,
    { "BVS"     , op_br          , 0x29 , 0x00 , BYTE  } ,
    { "CLR"     , op_die         , 0x0F , 0x00 , BYTE  } ,
    { "CLRA"    , op_inh         , 0x4F , 0x00 , BYTE  } ,
    { "CLRB"    , op_inh         , 0x5F , 0x00 , BYTE  } ,
    { "CMPA"    , op_idie        , 0x81 , 0x00 , BYTE  } ,
    { "CMPB"    , op_idie        , 0xC1 , 0x00 , BYTE  } ,
    { "CMPD"    , op_idie        , 0x83 , 0x10 , WORD  } ,
    { "CMPS"    , op_idie        , 0x8C , 0x11 , WORD  } ,
    { "CMPU"    , op_idie        , 0x83 , 0x11 , WORD  } ,
    { "CMPX"    , op_idie        , 0x8C , 0x00 , WORD  } ,
    { "CMPY"    , op_idie        , 0x8C , 0x10 , WORD  } ,
    { "COM"     , op_die         , 0x03 , 0x00 , BYTE  } ,
    { "COMA"    , op_inh         , 0x43 , 0x00 , BYTE  } ,
    { "COMB"    , op_inh         , 0x53 , 0x00 , BYTE  } ,
    { "CWAI"    , op_andcc       , 0x3C , 0x00 , BYTE  } ,
    { "DAA"     , op_inh         , 0x19 , 0x00 , BYTE  } ,
    { "DEC"     , op_die         , 0x0A , 0x00 , BYTE  } ,
    { "DECA"    , op_inh         , 0x4A , 0x00 , BYTE  } ,
    { "DECB"    , op_inh         , 0x5A , 0x00 , BYTE  } ,
    { "END"     , pseudo_end     , 0x00 , 0x00 , false } ,
    { "EORA"    , op_idie        , 0x88 , 0x00 , BYTE  } ,
    { "EORB"    , op_idie        , 0xC8 , 0x00 , BYTE  } ,
    { "EQU"     , pseudo_equ     , 0x00 , 0x00 , false } ,
    { "EXG"     , op_exg         , 0x1E , 0x00 , BYTE  } ,
    { "EXTDP"   , pseudo_extdp   , 0x00 , 0x00 , false } ,
    { "EXTERN"  , pseudo_extern  , 0x00 , 0x00 , false } ,
    { "FCB"     , pseudo_fcb     , 0x00 , 0x00 , false } ,
    { "FCC"     , pseudo_fcc     , 0x00 , 0x00 , false } ,
    { "FCS"     , pseudo_fcs     , 0x00 , 0x00 , false } ,
    { "FDB"     , pseudo_fdb     , 0x00 , 0x00 , false } ,
    { "INC"     , op_die         , 0x0C , 0x00 , BYTE  } ,
    { "INCA"    , op_inh         , 0x4C , 0x00 , BYTE  } ,
    { "INCB"    , op_inh         , 0x5C , 0x00 , BYTE  } ,
    { "INCBIN"  , pseudo_incbin  , 0x00 , 0x00 , false } ,
    { "INCLUDE" , pseudo_include , 0x00 , 0x00 , false } ,
    { "JMP"     , op_die         , 0x0E , 0x00 , BYTE  } ,
    { "JSR"     , op_die         , 0x8D , 0x00 , BYTE  } , // see below
    { "LBCC"    , op_lbr         , 0x24 , 0x10 , WORD  } ,
    { "LBCS"    , op_lbr         , 0x25 , 0x10 , WORD  } ,
    { "LBEQ"    , op_lbr         , 0x27 , 0x10 , WORD  } ,
    { "LBGE"    , op_lbr         , 0x2C , 0x10 , WORD  } ,
    { "LBGT"    , op_lbr         , 0x2E , 0x10 , WORD  } ,
    { "LBHI"    , op_lbr         , 0x22 , 0x10 , WORD  } ,
    { "LBHS"    , op_lbr         , 0x24 , 0x10 , WORD  } ,
    { "LBLE"    , op_lbr         , 0x2F , 0x10 , WORD  } ,
    { "LBLO"    , op_lbr         , 0x25 , 0x10 , WORD  } ,
    { "LBLS"    , op_lbr         , 0x23 , 0x10 , WORD  } ,
    { "LBLT"    , op_lbr         , 0x2D , 0x10 , WORD  } ,
    { "LBMI"    , op_lbr         , 0x2B , 0x10 , WORD  } ,
    { "LBNE"    , op_lbr         , 0x26 , 0x10 , WORD  } ,
    { "LBPL"    , op_lbr         , 0x2A , 0x10 , WORD  } ,
    { "LBRA"    , op_lbr         , 0x16 , 0x00 , WORD  } ,
    { "LBRN"    , op_lbr         , 0x21 , 0x10 , WORD  } ,
    { "LBSR"    , op_lbr         , 0x17 , 0x00 , WORD  } ,
    { "LBVC"    , op_lbr         , 0x28 , 0x10 , WORD  } ,
    { "LBVS"    , op_lbr         , 0x29 , 0x10 , WORD  } ,
    { "LDA"     , op_idie        , 0x86 , 0x00 , BYTE  } ,
    { "LDB"     , op_idie        , 0xC6 , 0x00 , BYTE  } ,
    { "LDD"     , op_idie        , 0xCC , 0x00 , WORD  } ,
    { "LDS"     , op_idie        , 0xCE , 0x10 , WORD  } ,
    { "LDU"     , op_idie        , 0xCE , 0x00 , WORD  } ,
    { "LDX"     , op_idie        , 0x8E , 0x00 , WORD  } ,
    { "LDY"     , op_idie        , 0x8E , 0x10 , WORD  } ,
    { "LEAS"    , op_lea         , 0x32 , 0x00 , WORD  } ,
    { "LEAU"    , op_lea         , 0x33 , 0x00 , WORD  } ,
    { "LEAX"    , op_lea         , 0x30 , 0x00 , WORD  } ,
    { "LEAY"    , op_lea         , 0x31 , 0x00 , WORD  } ,
    { "LSL"     , op_die         , 0x08 , 0x00 , BYTE  } ,
    { "LSLA"    , op_inh         , 0x48 , 0x00 , BYTE  } ,
    { "LSLB"    , op_inh         , 0x58 , 0x00 , BYTE  } ,
    { "LSR"     , op_die         , 0x04 , 0x00 , BYTE  } ,
    { "LSRA"    , op_inh         , 0x44 , 0x00 , BYTE  } ,
    { "LSRB"    , op_inh         , 0x54 , 0x00 , BYTE  } ,
    { "MUL"     , op_inh         , 0x3D , 0x00 , BYTE  } ,
    { "NEG"     , op_die         , 0x00 , 0x00 , BYTE  } ,
    { "NEGA"    , op_inh         , 0x40 , 0x00 , BYTE  } ,
    { "NEGB"    , op_inh         , 0x50 , 0x00 , BYTE  } ,
    { "NOP"     , op_inh         , 0x12 , 0x00 , BYTE  } ,
    { "ORA"     , op_idie        , 0x8A , 0x00 , BYTE  } ,
    { "ORB"     , op_idie        , 0xCA , 0x00 , BYTE  } ,
    { "ORCC"    , op_orcc        , 0x1A , 0x00 , BYTE  } ,
    { "ORG"     , pseudo_org     , 0x00 , 0x00 , false } ,
    { "PSHS"    , op_pshpul      , 0x34 , 0x00 , BYTE  } ,
    { "PSHU"    , op_pshpul      , 0x36 , 0x00 , BYTE  } ,
    { "PUBLIC"  , pseudo_public  , 0x00 , 0x00 , false } ,
    { "PULS"    , op_pshpul      , 0x35 , 0x00 , BYTE  } ,
    { "PULU"    , op_pshpul      , 0x37 , 0x00 , BYTE  } ,
    { "RMB"     , pseudo_rmb     , 0x00 , 0x00 , false } ,
    { "ROL"     , op_die         , 0x09 , 0x00 , BYTE  } ,
    { "ROLA"    , op_inh         , 0x49 , 0x00 , BYTE  } ,
    { "ROLB"    , op_inh         , 0x59 , 0x00 , BYTE  } ,
    { "ROR"     , op_die         , 0x06 , 0x00 , BYTE  } ,
    { "RORA"    , op_inh         , 0x46 , 0x00 , BYTE  } ,
    { "RORB"    , op_inh         , 0x56 , 0x00 , BYTE  } ,
    { "RTI"     , op_inh         , 0x3B , 0x00 , BYTE  } ,
    { "RTS"     , op_inh         , 0x39 , 0x00 , BYTE  } ,
    { "SBCA"    , op_idie        , 0x82 , 0x00 , BYTE  } ,
    { "SBCB"    , op_idie        , 0xC2 , 0x00 , BYTE  } ,
    { "SET"     , pseudo_set     , 0x00 , 0x00 , false } ,
    { "SETDP"   , pseudo_setdp   , 0x00 , 0x00 , false } ,
    { "SEX"     , op_inh         , 0x1D , 0x00 , BYTE  } ,
    { "STA"     , op_die         , 0x87 , 0x00 , BYTE  } , // even though the
    { "STB"     , op_die         , 0xC7 , 0x00 , BYTE  } , // immediate mode
    { "STD"     , op_die         , 0xCD , 0x00 , WORD  } , // is invalid for
    { "STS"     , op_die         , 0xCF , 0x10 , WORD  } , // these instructions,
    { "STU"     , op_die         , 0xCF , 0x00 , WORD  } , // include the value
    { "STX"     , op_die         , 0x8F , 0x00 , WORD  } , // here because the
    { "STY"     , op_die         , 0x8F , 0x10 , WORD  } , // way the code works
    { "SUBA"    , op_idie        , 0x80 , 0x00 , BYTE  } ,
    { "SUBB"    , op_idie        , 0xC0 , 0x00 , BYTE  } ,
    { "SUBD"    , op_idie        , 0x83 , 0x00 , WORD  } ,
    { "SWI"     , op_inh         , 0x3F , 0x00 , BYTE  } ,
    { "SWI2"    , op_inh         , 0x3F , 0x10 , BYTE  } ,
    { "SWI3"    , op_inh         , 0x3F , 0x11 , BYTE  } ,
    { "SYNC"    , op_inh         , 0x13 , 0x00 , BYTE  } ,
    { "TFR"     , op_exg         , 0x1F , 0x00 , BYTE  } ,
    { "TST"     , op_die         , 0x0D , 0x00 , BYTE  } ,
    { "TSTA"    , op_inh         , 0x4D , 0x00 , BYTE  } ,
    { "TSTB"    , op_inh         , 0x5D , 0x00 , BYTE  } ,
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
