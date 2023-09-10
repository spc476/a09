
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
    char *p;
    char  c = buffer->buf[buffer->ridx];
    
    if (c == '_')
    {
      buffer->ridx++;
      continue;
    }
    
    if (c == '\0')
      return cnt > 0;
    
    if (!isxdigit(c))
      return cnt > 0;
    
    c = toupper(c);
    p = strchr(trans,toupper(c));
    if (p == NULL)
      return cnt > 0;
    uint16_t v = (uint16_t)(p - trans);
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

static bool factor(uint16_t *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  
  bool neg  = false;
  bool not  = false;
  bool plus = false;
  char c    = skip_space(buffer);
  bool rc;
  
  if (c == '*')
  {
    *pv = a09->pc;
    return true;
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
    rc = s2num(pv,buffer,16);
  else if (c == '&')
    rc = s2num(pv,buffer,8);
  else if (c == '%')
    rc = s2num(pv,buffer,2);
  else if (isdigit(c))
  {
    buffer->ridx--;
    rc = s2num(pv,buffer,10);
  }
  else if ((c == '_') || (c == '.') || isalpha(c))
  {
    struct symbol *sym;
    char          *label = NULL;
    size_t         len   = 0;
    
    if (!read_label(buffer,&label,&len,c))
    {
      assert(label == NULL);
      return false;
    }
    sym = symbol_find(a09,label);
    if (sym == NULL)
    {
      if (pass == 2)
      {
        message(a09,MSG_ERROR,"unknown symbol '%s'",label);
        free(label);
        return false;
      }
      *pv = 0;
    }
    else
      *pv = sym->value;
    free(label);
  }
  else if (c == '\'')
  {
    c = buffer->buf[buffer->ridx++];
    if (isgraph(c))
    {
      *pv = c;
      return true;
    }
    else
      return false;
  }
  else
    return false;
  
  if (neg)
  {
    if (*pv > INT16_MAX)
      return false;
    *pv = -*pv;
  }
  else if (not)
    *pv = ~*pv;
    
  return true;
}

/**************************************************************************/

bool expr(uint16_t *pv,struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(pv     != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  return factor(pv,a09,buffer,pass);
}

/**************************************************************************/
