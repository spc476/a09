
/* GPL3+ */

#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

static bool fbin_dp(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool fbin_code(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool fbin_align(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  (void)fmt;
  
  if (opd->pass == 2)
  {
    if (fseek(opd->a09->out,opd->datasz,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  
  return true;
}

/**************************************************************************/

static bool fbin_end(union format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  (void)fmt;
  (void)opd;
  (void)sym;
  return true;
}

/**************************************************************************/

static bool fbin_org(union format *fmt,struct opcdata *opd,uint16_t start,uint16_t last)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct format_bin *format = &fmt->bin;
  
  if (opd->pass == 2)
  {
    if (format->first)
    {
      uint16_t delta = start - last;
      fprintf(stderr,"pc=%04X start=%04X delta=%d\n",last,start,delta);
      if (fseek(opd->a09->out,delta,SEEK_CUR) == -1)
        return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    }
    
    format->first = true;
  }
  return true;
}

/**************************************************************************/

static bool fbin_setdp(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

bool format_bin_init(struct format_bin *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->dp    = fbin_dp;
  fmt->code  = fbin_code;
  fmt->align = fbin_align;
  fmt->end   = fbin_end;
  fmt->org   = fbin_org;
  fmt->setdp = fbin_setdp;
  fmt->first = false;
  return true;
}

/**************************************************************************/
