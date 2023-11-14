
/* GPL3+ */

#include <stdbool.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

static bool frsdos_dp(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool frsdos_code(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool frsdos_align(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool frsdos_end(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool frsdos_org(union format *fmt,struct opcdata *opd)
{
  
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool frsdos_setdp(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

bool format_rsdos_init(struct format_rsdos *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  
  fmt->dp    = frsdos_dp;
  fmt->code  = frsdos_code;
  fmt->align = frsdos_align;
  fmt->end   = frsdos_end;
  fmt->org   = frsdos_org;
  fmt->setdp = frsdos_setdp;
  return true;
}

/**************************************************************************/
