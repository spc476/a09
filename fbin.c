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

char const format_bin_usage[] = "";

/**************************************************************************/

static bool fbin_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_BIN);
  (void)fmt;
  
  if (opd->pass == 2)
  {
    if (fseek(opd->a09->out,opd->datasz,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  
  return true;
}

/**************************************************************************/

static bool fbin_org(struct format *format,struct opcdata *opd)
{
  assert(format != NULL);
  assert(opd    != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(format->backend == BACKEND_BIN);
  
  if (opd->pass == 2)
  {
    /*----------------------------------------------------------------------
    ; format->data is used as a flag; if NULL, it's false, if not NULL, it's
    ; true.  It saves a memory allocation.
    ;-----------------------------------------------------------------------*/
    
    if (format->data)
    {
      uint16_t delta = opd->value.value - opd->a09->pc;
      if (fseek(opd->a09->out,delta,SEEK_CUR) == -1)
        return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    }
    
    format->data = format;
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool fbin_rmb(struct format *format,struct opcdata *opd)
{
  assert(format != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1 ) || (opd->pass == 2));
  assert(format->backend == BACKEND_BIN);
  (void)format;
  
  if (opd->pass == 2)
  {
    if (fseek(opd->a09->out,opd->value.value,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  return true;
}

/**************************************************************************/

bool format_bin_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_BIN,
    .cmdline    = fdefault_cmdline,
    .pass_start = fdefault_pass,
    .pass_end   = fdefault_pass,
    .write      = fdefault_write,
    .opt        = fdefault,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = fbin_align,
    .end        = fdefault_end,
    .org        = fbin_org,
    .rmb        = fbin_rmb,
    .setdp      = fdefault,
    .test       = fdefault_test,
    .tron       = fdefault,
    .troff      = fdefault,
    .Assert     = fdefault,
    .endtst     = fdefault,
    .Float      = freal_ieee,
    .fini       = fdefault_fini,
    .data       = NULL,
  };
  
  assert(a09 != NULL);
  a09->format      = callbacks;
  a09->format.data = NULL;
  return true;
}

/**************************************************************************/
