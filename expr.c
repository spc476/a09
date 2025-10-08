/****************************************************************************
*
*   Parse an expression and return a value
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
* ---------------------------------------------------------------------
*
* Shunting Yard algorithm.  See
*
*           https://en.wikipedia.org/wiki/Shunting_yard_algorithm
*
* for more information on this algorithem.  This is a modified Shunting Yard
* algorithm, as we have no functions, and the parenthensis are handled by
* factor().  See the table cops[] in get_op() for the precedence levels.
*
****************************************************************************/

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

static bool eval(
                  struct a09   *         a09,
                  struct value *restrict v1,
                  enum operator          op,
                  struct value *restrict v2,
                  int                    pass
)
{
  assert(a09 != NULL);
  assert(v1  != NULL);
  assert(v2  != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (v1->external || v2->external)
    return message(a09,MSG_ERROR,"E0007: EXTERN in expression not allowed");
    
  switch(op)
  {
    case OP_LOR:  v1->value = v1->value || v2->value; break;
    case OP_LAND: v1->value = v1->value && v2->value; break;
    case OP_GT:   v1->value = v1->value >  v2->value; break;
    case OP_GE:   v1->value = v1->value >= v2->value; break;
    case OP_EQ:   v1->value = v1->value == v2->value; break;
    case OP_LE:   v1->value = v1->value <= v2->value; break;
    case OP_LT:   v1->value = v1->value <  v2->value; break;
    case OP_NE:   v1->value = v1->value != v2->value; break;
    case OP_BOR:  v1->value = v1->value |  v2->value; break;
    case OP_BEOR: v1->value = v1->value ^  v2->value; break;
    case OP_BAND: v1->value = v1->value &  v2->value; break;
    case OP_SHR:  v1->value = v1->value >> v2->value; break;
    case OP_SHL:  v1->value = v1->value << v2->value; break;
    case OP_SUB:  v1->value = v1->value -  v2->value; break;
    case OP_ADD:  v1->value = v1->value +  v2->value; break;
    case OP_MUL:  v1->value = v1->value *  v2->value; break;
    case OP_DIV:
         if (pass == 1)
           v1->value = 0;
         else
         {
           if (v2->value == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value = v1->value / v2->value;
         }
         break;
         
    case OP_MOD:
         if (pass == 1)
           v1->value = 0;
         else
         {
           if (v2->value == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value = v1->value % v2->value;
         }
         break;
         
    case OP_EXP:
         if (v2->value > 0x7FFF)
           return message(a09,MSG_ERROR,"E0091: negative exponents for integers not supported");
         else if (v2->value == 0)
           v1->value = 1;
         else
         {
           while(--v2->value)
             v1->value = v1->value * v1->value;
         }
         break;
         
    case OP_WORD:
         v1->value = value_lsb(a09,v1->value,pass) * 256
                   + value_lsb(a09,v2->value,pass);
         break;
  }
  
  v1->unknownpass1 = v1->unknownpass1 || v2->unknownpass1;
  v1->defined      = v1->defined      && v2->defined;
  return true;
}

/**************************************************************************/

bool s2num(struct a09 *a09,uint16_t *pv,struct buffer *buffer,uint16_t base)
{
  assert(a09    != NULL);
  assert(pv     != NULL);
  assert(buffer != NULL);
  assert((base == 2) || (base == 8) || (base == 10) || (base == 16));
  
  size_t cnt = 0;
  *pv        = 0;
  
  while(true)
  {
    uint16_t v;
    char     c = buffer->buf[buffer->ridx];
    
    if (c == '_')
    {
      buffer->ridx++;
      continue;
    }
    
    if (c == '\0')
      return cnt > 0;
    if (!isxdigit(c))
      return cnt > 0;
      
    v = toupper(c) - '0';
    if (v > 9)
      v -= 7;
      
    if (v >= base)
      return message(a09,MSG_ERROR,"E0061: '%c' not allowed in literal number",c);
      
    *pv *= base;
    
    if (*pv > UINT16_MAX - v)
      return message(a09,MSG_ERROR,"E0009: literal exceeds absolute allowable range of 0..65535");
      
    *pv += v;
    buffer->ridx++;
    cnt++;
  }
}

/**************************************************************************/

static bool value(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  char c      = skip_space(buffer);
  bool rc     = true;
  pv->defined = true; /* optimistic setting */
  
  if (c == '*')
  {
    pv->value = a09->pc;
    return true;
  }
  
  if (c == '$')
    rc = s2num(a09,&pv->value,buffer,16);
  else if (c == '&')
    rc = s2num(a09,&pv->value,buffer,8);
  else if (c == '%')
    rc = s2num(a09,&pv->value,buffer,2);
  else if (isdigit(c))
  {
    buffer->ridx--;
    rc = s2num(a09,&pv->value,buffer,10);
  }
  else if ((c == '_') || (c == '.') || isalpha(c))
  {
    struct symbol *sym;
    label          label;
    
    buffer->ridx--;
    if (!parse_label(&label,buffer,a09,pass))
      return false;
      
    sym = symbol_find(a09,&label);
    if (sym == NULL)
    {
      if (pass == 2)
        return message(a09,MSG_ERROR,"E0004: unknown symbol '%.*s'",label.s,label.text);
        
      pv->defined      = false;
      pv->external     = false;
      pv->unknownpass1 = true;
      pv->value        = 0;
    }
    else
    {
      pv->unknownpass1 = (sym->filename == a09->infile) && (sym->ldef > a09->lnum);
      pv->value        = sym->value;
      pv->external     = sym->type == SYM_EXTERN;
      if (pass == 2)
        sym->refs++;
    }
  }
  else if (c == '\'')
  {
    c = buffer->buf[buffer->ridx++];
    if (isprint(c))
    {
      pv->value = c;
      if (buffer->buf[buffer->ridx] == '\'')
        buffer->ridx++;
      return true;
    }
    else
      return message(a09,MSG_ERROR,"E0005: illegal escaped character in value");
  }
  else
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  if (!rc)
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  return rc;
}

/**************************************************************************/

static bool factor(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  bool neg = false;
  bool not = false;
  
  memset(pv,0,sizeof(struct value));
  char c = skip_space(buffer);
  if (c == '\0')
    return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    
  if (c == '>')
  {
    pv->bits = 16;
    c = skip_space(buffer);
  }
  else if (c == '<')
  {
    if (buffer->buf[buffer->ridx] == '<')
    {
      buffer->ridx++;
      pv->bits = 5;
    }
    else
      pv->bits = 8;
      
    c = skip_space(buffer);
  }
  
  if (c == '-')
  {
    neg = true;
    c   = buffer->buf[buffer->ridx++];
  }
  else if (c == '~')
  {
    not = true;
    c   = buffer->buf[buffer->ridx++];
  }
  else if (c == '+')
    c = buffer->buf[buffer->ridx++];
    
  if (c == '(')
  {
    c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    buffer->ridx--;
    if (!expr(pv,a09,buffer,pass))
      return false;
    c = skip_space(buffer);
    if (c != ')')
      return message(a09,MSG_ERROR,"E0011: missing right parenthesis");
  }
  else
  {
    buffer->ridx--;
    if (!value(pv,a09,buffer,pass))
      return false;
  }
  
  if (neg)
    pv->value = -pv->value;
  else if (not)
    pv->value = ~pv->value;
    
  return true;
}

/**************************************************************************/

struct optable const *get_op(struct buffer *buffer)
{
  assert(buffer != NULL);
  
  static struct optable const cops[] =
  {
    [OP_EXP]  = { OP_EXP  , AS_RIGHT , 1000 } ,
    [OP_MUL]  = { OP_MUL  , AS_LEFT  ,  900 } ,
    [OP_DIV]  = { OP_DIV  , AS_LEFT  ,  900 } ,
    [OP_MOD]  = { OP_MOD  , AS_LEFT  ,  900 } ,
    [OP_ADD]  = { OP_ADD  , AS_LEFT  ,  800 } ,
    [OP_SUB]  = { OP_SUB  , AS_LEFT  ,  800 } ,
    [OP_SHL]  = { OP_SHL  , AS_LEFT  ,  700 } ,
    [OP_SHR]  = { OP_SHR  , AS_LEFT  ,  700 } ,
    [OP_BAND] = { OP_BAND , AS_LEFT  ,  600 } ,
    [OP_BEOR] = { OP_BEOR , AS_LEFT  ,  500 } ,
    [OP_BOR]  = { OP_BOR  , AS_LEFT  ,  400 } ,
    [OP_WORD] = { OP_WORD , AS_LEFT  ,  350 } ,
    [OP_NE]   = { OP_NE   , AS_LEFT  ,  300 } ,
    [OP_LT]   = { OP_LT   , AS_LEFT  ,  300 } ,
    [OP_LE]   = { OP_LE   , AS_LEFT  ,  300 } ,
    [OP_EQ]   = { OP_EQ   , AS_LEFT  ,  300 } ,
    [OP_GE]   = { OP_GE   , AS_LEFT  ,  300 } ,
    [OP_GT]   = { OP_GT   , AS_LEFT  ,  300 } ,
    [OP_LAND] = { OP_LAND , AS_LEFT  ,  200 } ,
    [OP_LOR]  = { OP_LOR  , AS_LEFT  ,  100 } ,
  };
  
  char c = skip_space(buffer);
  
  switch(c)
  {
    case '/': return &cops[OP_DIV];
    case '%': return &cops[OP_MOD];
    case '+': return &cops[OP_ADD];
    case '-': return &cops[OP_SUB];
    case '^': return &cops[OP_BEOR];
    case '=': return &cops[OP_EQ];
    
    case '*':
         if (buffer->buf[buffer->ridx] == '*')
         {
           buffer->ridx++;
           return &cops[OP_EXP];
         }
         else
           return &cops[OP_MUL];
           
    case '&':
         if (buffer->buf[buffer->ridx] == '&')
         {
           buffer->ridx++;
           return &cops[OP_LAND];
         }
         else
           return &cops[OP_BAND];
           
    case '|':
         if (buffer->buf[buffer->ridx] == '|')
         {
           buffer->ridx++;
           return &cops[OP_LOR];
         }
         else
           return &cops[OP_BOR];
           
    case '<':
         if (buffer->buf[buffer->ridx] == '<')
         {
           buffer->ridx++;
           return &cops[OP_SHL];
         }
         else if (buffer->buf[buffer->ridx] == '=')
         {
           buffer->ridx++;
           return &cops[OP_LE];
         }
         else if (buffer->buf[buffer->ridx] == '>')
         {
           buffer->ridx++;
           return &cops[OP_NE];
         }
         else
           return &cops[OP_LT];
           
    case '>':
         if (buffer->buf[buffer->ridx] == '>')
         {
           buffer->ridx++;
           return &cops[OP_SHR];
         }
         else if (buffer->buf[buffer->ridx] == '=')
         {
           buffer->ridx++;
           return &cops[OP_GE];
         }
         else
           return &cops[OP_GT];
           
    case ':':
         if (buffer->buf[buffer->ridx] == ':')
         {
           buffer->ridx++;
           return &cops[OP_WORD];
         } /* fallthrough */
         
    default:
         if (c != '\0')
           buffer->ridx--;
         return NULL;
  }
}

/**************************************************************************/

bool expr(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  struct optable const *op;
  struct value          vstack[15];
  struct optable const *ostack[15];
  size_t                vsp  = sizeof(vstack) / sizeof(vstack[0]);
  size_t                osp  = sizeof(ostack) / sizeof(ostack[0]);
  
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (!factor(&vstack[--vsp],a09,buffer,pass))
    return false;
    
  while(true)
  {
    if ((op = get_op(buffer)) == NULL)
      break;
      
    while(osp < sizeof(ostack) / sizeof(ostack[0]))
    {
      if (
               (ostack[osp]->pri >  op->pri)
           || ((ostack[osp]->pri == op->pri) && (op->as == AS_LEFT))
         )
      {
        if (vsp >= (sizeof(vstack) / sizeof(vstack[0])) - 1)
          return message(a09,MSG_ERROR,"E0065: Internal error---expression parser mismatch");
        if (!eval(a09,&vstack[vsp + 1],ostack[osp]->op,&vstack[vsp],pass))
          return false;
        vsp++;
        osp++;
      }
      else
        break;
    }
    
    if (osp == 0)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    ostack[--osp] = op;
    
    if (vsp == 0)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    if (!factor(&vstack[--vsp],a09,buffer,pass))
      return false;
  }
  
  while(osp < sizeof(ostack) / sizeof(ostack[0]))
  {
    if (vsp >= (sizeof(vstack) / sizeof(vstack[0])) - 1)
      return message(a09,MSG_ERROR,"E0065: Internal error---expression parser mismatch");
    if (!eval(a09,&vstack[vsp + 1],ostack[osp]->op,&vstack[vsp],pass))
      return false;
    vsp++;
    osp++;
  }
  
  assert(osp ==  sizeof(ostack) / sizeof(ostack[0]));
  assert(vsp == (sizeof(vstack) / sizeof(vstack[0]) - 1));
  
  *pv = vstack[vsp];
  return true;
}

/**************************************************************************/
