
/* GPL3+ */

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

static bool s2num(uint16_t *pv,struct buffer *buffer,uint16_t base)
{
  assert(pv     != NULL);
  assert(buffer != NULL);
  assert((base == 2) || (base == 8) || (base == 10) || (base == 16));
  
  size_t cnt = 0;
  *pv        = 0;
  
  while(true)
  {
    static char const trans[16] = "0123456789ABCDEF";
    char              c         = buffer->buf[buffer->ridx];
    
    if (c == '_')
    {
      buffer->ridx++;
      continue;
    }
    
    if (c == '\0')
      return cnt > 0;
    if (!isxdigit(c))
      return cnt > 0;
      
    uint16_t v = (uint16_t)((char const *)memchr(trans,toupper(c),sizeof(trans)) - trans);
    
    if (v >= base)
      return false;
    if (*pv > UINT16_MAX - v)
      return false;
      
    *pv *= base;
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
  
  bool neg  = false;
  bool not  = false;
  bool plus = false;
  char c    = skip_space(buffer);
  bool rc   = true;
  
  pv->defined = true; /* optimistic setting */
  
  if (c == '*')
  {
    pv->value = a09->pc;
    return true;
  }
  
  if (c == '>')
  {
    pv->bits = 16;
    c = buffer->buf[buffer->ridx++];
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
    c = buffer->buf[buffer->ridx++];
  }
  
  do
  {
    if (c == '-')
    {
      if (not || plus || neg) return false;
      neg = true;
      c   = buffer->buf[buffer->ridx++];
      continue;
    }
    else if (c == '~')
    {
      if (neg || plus || not) return false;
      not = true;
      c   = buffer->buf[buffer->ridx++];
      continue;
    }
    else if (c == '+')
    {
      if (neg || plus || not) return false;
      plus = true;
      c    = buffer->buf[buffer->ridx++];
      continue;
    }
  } while(false);
  
  if (c == '$')
    rc = s2num(&pv->value,buffer,16);
  else if (c == '&')
    rc = s2num(&pv->value,buffer,8);
  else if (c == '%')
    rc = s2num(&pv->value,buffer,2);
  else if (isdigit(c))
  {
    buffer->ridx--;
    rc = s2num(&pv->value,buffer,10);
  }
  else if ((c == '_') || (c == '.') || isalpha(c))
  {
    struct symbol *sym;
    label          label;
    
    buffer->ridx--;
    if (!parse_label(&label,buffer,a09))
      return false;
      
    sym = symbol_find(a09,&label);
    if (sym == NULL)
    {
      if (pass == 2)
      {
        message(a09,MSG_ERROR,"unknown symbol '%.*s'",label.s,label.text);
        return false;
      }
      pv->defined      = false;
      pv->unknownpass1 = true;
      pv->value        = 0;
    }
    else
    {
      pv->unknownpass1 = sym->ldef > a09->lnum;
      pv->value        = sym->value;
    }
  }
  else if (c == '\'')
  {
    c = buffer->buf[buffer->ridx++];
    if (isgraph(c))
    {
      pv->value = c;
      return true;
    }
    else
      return false;
  }
  else
    return false;
    
  if (neg)
  {
    if (pv->value > INT16_MAX)
      return false;
    pv->value = -pv->value;
  }
  else if (not)
    pv->value = ~pv->value;
    
  return rc;
}

/**************************************************************************/

static bool eval(struct a09 *a09,struct value *v1,char op,struct value *v2)
{
  assert(v1 != NULL);
  assert(v2 != NULL);
  
  switch(op)
  {
    case '+': v1->value += v2->value; break;
    case '-': v1->value -= v2->value; break;
    case '*': v1->value *= v2->value; break;
    case '/':
         if (v2->value == 0)
           return message(a09,MSG_ERROR,"divide by 0 error");
         v1->value /= v2->value;
         break;
         
    default:
         return message(a09,MSG_ERROR,"internal error");
  }
  
  v1->unknownpass1 = v1->unknownpass1 || v2->unknownpass1;
  v1->defined      = v1->defined      && v2->defined;
  return true;
}

/**************************************************************************/

static bool factor(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  char c = skip_space(buffer);
  if (c == '\0')
    return message(a09,MSG_ERROR,"unexpected end of input");
    
  if (c == '(')
  {
    c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"unexpected end of input");
    buffer->ridx--;
    if (!expr(pv,a09,buffer,pass))
      return false;
    c = skip_space(buffer);
    if (c != ')')
      return message(a09,MSG_ERROR,"missing right parenthesis");
    return true;
  }
  else
  {
    buffer->ridx--;
    return value(pv,a09,buffer,pass);
  }
}

/**************************************************************************/

static bool term(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));

  if (!factor(pv,a09,buffer,pass))
    return false;
  
  while(true)
  {
    struct value val;
    
    char op = skip_space(buffer);
    if (op == '\0')
      return true;
    if ((op != '*') && (op != '/'))
    {
      buffer->ridx--;
      return true;
    }
    
    char c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"unexpected end of expression");
    
    memset(&val,0,sizeof(val));
    buffer->ridx--;
    if (!factor(&val,a09,buffer,pass))
      return message(a09,MSG_ERROR,"missing factor in expression");
    if (!eval(a09,pv,op,&val))
      return false;
  }
}

/**************************************************************************/

bool expr(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  memset(pv,0,sizeof(struct value));
  if (!term(pv,a09,buffer,pass))
    return false;
  
  while(true)
  {
    struct value val;
    char op = skip_space(buffer);
    if (op == '\0')
      return true;
    
    if ((op != '+') && (op != '-'))
    {
      buffer->ridx--;
      return true;
    }
    
    char c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"unexpected end of expression");
    
    memset(&val,0,sizeof(val));
    buffer->ridx--;
    if (!term(&val,a09,buffer,pass))
      return message(a09,MSG_ERROR,"missing term in expression");
    
    if (!eval(a09,pv,op,&val))
      return false;
  }
}

/**************************************************************************/
