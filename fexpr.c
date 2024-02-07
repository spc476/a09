/****************************************************************************
*
*   Parse a floating point expression and return a value
*   Copyright (C) 2024 Sean Conner
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
* Note: Even though we don't have any right associative operators, we
* do have support for them for the time when (if) they are added.
*
****************************************************************************/

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

static bool feval(
                  struct a09    *         a09,
                  struct fvalue *restrict v1,
                  enum operator           op,
                  struct fvalue *restrict v2,
                  bool                    fdouble
)
{
  assert(a09 != NULL);
  assert(v1  != NULL);
  assert(v2  != NULL);
  
  if (v1->external || v2->external)
    return message(a09,MSG_ERROR,"E0007: EXTERN in expression not allowed");
    
  if (fdouble)
  {
    switch(op)
    {
      case OP_LOR:  v1->value.d = v1->value.d || v2->value.d; break;
      case OP_LAND: v1->value.d = v1->value.d && v2->value.d; break;
      case OP_GT:   v1->value.d = v1->value.d >  v2->value.d; break;
      case OP_GE:   v1->value.d = v1->value.d >= v2->value.d; break;
      case OP_EQ:   v1->value.d = v1->value.d == v2->value.d; break;
      case OP_LE:   v1->value.d = v1->value.d <= v2->value.d; break;
      case OP_LT:   v1->value.d = v1->value.d <  v2->value.d; break;
      case OP_NE:   v1->value.d = v1->value.d != v2->value.d; break;
      case OP_BOR:
      case OP_BEOR:
      case OP_BAND:
      case OP_SHR:
      case OP_SHL:  return message(a09,MSG_ERROR,"E0092: operator not supported for floats");
      case OP_SUB:  v1->value.d = v1->value.d -  v2->value.d; break;
      case OP_ADD:  v1->value.d = v1->value.d +  v2->value.d; break;
      case OP_MUL:  v1->value.d = v1->value.d *  v2->value.d; break;
      case OP_EXP:  v1->value.d = pow(v1->value.d,v2->value.d); break;
      case OP_DIV:
           if (v2->value.d == 0.0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value.d = v1->value.d / v2->value.d;
           break;
           
      case OP_MOD:
           if (v2->value.d == 0.0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value.d = fmod(v1->value.d,v2->value.d);
           break;
    }
  }
  else
  {
    switch(op)
    {
      case OP_LOR:  v1->value.f = v1->value.f || v2->value.f; break;
      case OP_LAND: v1->value.f = v1->value.f && v2->value.f; break;
      case OP_GT:   v1->value.f = v1->value.f >  v2->value.f; break;
      case OP_GE:   v1->value.f = v1->value.f >= v2->value.f; break;
      case OP_EQ:   v1->value.f = v1->value.f == v2->value.f; break;
      case OP_LE:   v1->value.f = v1->value.f <= v2->value.f; break;
      case OP_LT:   v1->value.f = v1->value.f <  v2->value.f; break;
      case OP_NE:   v1->value.f = v1->value.f != v2->value.f; break;
      case OP_BOR:
      case OP_BEOR:
      case OP_BAND:
      case OP_SHR:
      case OP_SHL:  return message(a09,MSG_ERROR,"E0092: operator not supported for floats");
      case OP_SUB:  v1->value.f = v1->value.f -  v2->value.f; break;
      case OP_ADD:  v1->value.f = v1->value.f +  v2->value.f; break;
      case OP_MUL:  v1->value.f = v1->value.f *  v2->value.f; break;
      case OP_EXP:  v1->value.f = powf(v1->value.f,v2->value.f); break;
      case OP_DIV:
           if (v2->value.f == 0.0f)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value.f = v1->value.f / v2->value.f;
           break;
           
      case OP_MOD:
           if (v2->value.f == 0.0f)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           v1->value.f = fmodf(v1->value.f,v2->value.f);
           break;
    }
  }
  v1->unknownpass1 = v1->unknownpass1 || v2->unknownpass1;
  v1->defined      = v1->defined      && v2->defined;
  return true;
}

/**************************************************************************/

static bool fvalue(struct fvalue *pv,struct a09 *a09,struct buffer *buffer,int pass,bool fdouble)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));

  char c   = skip_space(buffer);
  bool rc  = true;
  
  pv->defined = true; /* optimistic setting */
  
  if (c == '*')
  {
    if (fdouble)
      pv->value.d = a09->pc;
    else
      pv->value.f = a09->pc;
    return true;
  }
  
  if (c == '$')
  {
    uint16_t val;
    rc = s2num(a09,&val,buffer,16);
    if (rc && fdouble)
      pv->value.d = val;
    else
      pv->value.f = val;
  }
  else if (c == '&')
  {
    uint16_t val;
    rc = s2num(a09,&val,buffer,8);
    if (rc && fdouble)
      pv->value.d = val;
    else
      pv->value.f = val;
  }
  else if (c == '%')
  {
    uint16_t val;
    rc = s2num(a09,&val,buffer,2);
    if (rc && fdouble)
      pv->value.d = val;
    else
      pv->value.f = val;
  }
  else if ((c == '+') || (c == '-') || isdigit(c))
  {
    char *p;
    
    buffer->ridx--;
    errno = 0;
    if (fdouble)
      pv->value.d = strtod(&buffer->buf[buffer->ridx],&p);
    else
      pv->value.f = strtof(&buffer->buf[buffer->ridx],&p);
      
    rc            = !((p == &buffer->buf[buffer->ridx]) || (errno != 0));
    buffer->ridx += p - &buffer->buf[buffer->ridx];
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
      if (fdouble)
        pv->value.d = 0.0;
      else
        pv->value.f = 0.0f;
    }
    else
    {
      pv->unknownpass1 = (sym->filename == a09->infile) && (sym->ldef > a09->lnum);
      pv->external     = sym->type == SYM_EXTERN;
      if (pass == 2)
        sym->refs++;
      if (fdouble)
        pv->value.d = sym->value;
      else
        pv->value.f = sym->value;
    }
  }
  else
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  if (!rc)
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  /*---------------------------------------------------------------------
  ; Check for an exclamation mark, which we're using to denote factorial. 
  ; 0! is 1 (by definition).  A negative number is an error.
  ;----------------------------------------------------------------------*/
  
  c = skip_space(buffer);
  if (c == '!')
  {
    if (fdouble)
    {
      if (pv->value.d < 0.0)
        return message(a09,MSG_ERROR,"E0093: factorial not defined for negative numbers");
      else if (pv->value.d == 0.0)
        pv->value.d = 1.0;
      else 
      {
        double v = 1.0;
        while(pv->value.d > 0.0)
        {
          v = v * pv->value.d;
          pv->value.d = pv->value.d - 1.0;
        }
        pv->value.d = v;
      }
    }
    else
    {
      if (pv->value.f < 0.0f)
        return message(a09,MSG_ERROR,"E0093: factorial not defined for negative numbers");
      else if (pv->value.f == 0.0f)
        pv->value.f = 1.0f;
      else
      {
        float v = 1.0f;
        while(pv->value.f > 0.0)
        {
          v = v * pv->value.f;
          pv->value.f = pv->value.f - 1.0f;
        }
        pv->value.f = v;
      }
    }
  }
  else if (c != '\0')
    buffer->ridx--;
    
  return rc;
}

/**************************************************************************/

static bool ffactor(struct fvalue *pv,struct a09 *a09,struct buffer *buffer,int pass,bool fdouble)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  bool neg = false;
  
  memset(pv,0,sizeof(struct fvalue));
  
  char c = skip_space(buffer);
  if (c == '\0')
    return message(a09,MSG_ERROR,"E0010: unexpected end of input");
  
  if (c == '-')
  {
    neg = true;
    c   = skip_space(buffer);
  }
  
  if (c == '(')
  {
    c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    buffer->ridx--;
    if (!fexpr(pv,a09,buffer,pass,fdouble))
      return false;
    c = skip_space(buffer);
    if (c != ')')
      return message(a09,MSG_ERROR,"E0011: missing right parenthesis");
  }
  else
  {
    buffer->ridx--;
    if (!fvalue(pv,a09,buffer,pass,fdouble))
      return false;
  }
  
  if (neg)
  {
    if (fdouble)
      pv->value.d = -pv->value.d;
    else
      pv->value.f = -pv->value.f;
  }
  
  return true;
}

/**************************************************************************/

bool fexpr(struct fvalue *pv,struct a09 *a09,struct buffer *buffer,int pass,bool fdouble)
{
  struct optable const *op;
  struct fvalue         vstack[15];
  struct optable const *ostack[15];
  size_t                vsp  = sizeof(vstack) / sizeof(vstack[0]);
  size_t                osp  = sizeof(ostack) / sizeof(ostack[0]);
  
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (!ffactor(&vstack[--vsp],a09,buffer,pass,fdouble))
    return false;
    
  while(true)
  {
    if ((op = get_op(buffer)) == NULL)
      break;
      
    while(osp < sizeof(ostack) / sizeof(ostack[0]))
    {
      if (
               (ostack[osp]->pri >  op->pri)
           || ((ostack[osp]->pri == op->pri) && (op->pri == AS_LEFT))
         )
      {
        if (vsp >= (sizeof(vstack) / sizeof(vstack[0])) - 1)
          return message(a09,MSG_ERROR,"E0065: Internal error---expression parser mismatch");
        if (!feval(a09,&vstack[vsp + 1],ostack[osp]->op,&vstack[vsp],fdouble))
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
    if (!ffactor(&vstack[--vsp],a09,buffer,pass,fdouble))
      return false;
  }
  
  while(osp < sizeof(ostack) / sizeof(ostack[0]))
  {
    if (vsp >= (sizeof(vstack) / sizeof(vstack[0])) - 1)
      return message(a09,MSG_ERROR,"E0065: Internal error---expression parser mismatch");
    if (!feval(a09,&vstack[vsp + 1],ostack[osp]->op,&vstack[vsp],fdouble))
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
