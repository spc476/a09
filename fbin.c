/****************************************************************************
*
*   Code to support the bin (binary) output format
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
      if (fseek(opd->a09->out,delta,SEEK_CUR) == -1)
        return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    }
    
    format->first = true;
  }
  return true;
}

/**************************************************************************/

static bool fbin_rmb(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  assert(opd != NULL);
  assert((opd->pass == 1 ) || (opd->pass == 2));
  
  /*---------------------
  ; XXX - maybe if the file offset is 0, skip this, to become
  ; compatible with the Lua assembler I have?
  ;--------------------*/
  
  if (opd->pass == 2)
  {
    if (fseek(opd->a09->out,opd->value.value,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
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
  fmt->rmb   = fbin_rmb;
  fmt->setdp = fbin_setdp;
  fmt->first = false;
  return true;
}

/**************************************************************************/
