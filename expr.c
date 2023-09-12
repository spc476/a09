
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

static bool factor(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  bool neg  = false;
  bool not  = false;
  bool plus = false;
  char c    = skip_space(buffer);
  bool rc;
  
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
      pv->unknownpass1 = pass == 1;
      pv->value       = 0;
    }
    else
      pv->value = sym->value;
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
    
  return true;
}

/**************************************************************************/

bool expr(struct value *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  pv->name         = NULL;
  pv->value        = 0;
  pv->bits         = 0;
  pv->unknownpass1 = false;
  pv->defined      = false;
  return factor(pv,a09,buffer,pass);
}

/**************************************************************************/
