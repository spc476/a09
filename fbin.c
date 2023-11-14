
/* GPL3+ */

#include <stdbool.h>
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
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool fbin_end(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

static bool fbin_org(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
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
  
  fmt->dp    = fbin_dp;
  fmt->code  = fbin_code;
  fmt->align = fbin_align;
  fmt->end   = fbin_end;
  fmt->org   = fbin_org;
  fmt->setdp = fbin_setdp;
  return true;
}

/**************************************************************************/
