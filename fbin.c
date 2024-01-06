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

static bool fbin_align(union format *fmt,struct opcdata *opd)
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

static bool fbin_org(union format *fmt,struct opcdata *opd,uint16_t start,uint16_t last)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_BIN);
  
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
  assert(fmt->backend == BACKEND_BIN);
  
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

bool format_bin_init(struct format_bin *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->backend    = BACKEND_BIN;
  fmt->cmdline    = fdefault_cmdline;
  fmt->pass_start = fdefault_pass;
  fmt->pass_end   = fdefault_pass;
  fmt->inst_write = fdefault_inst_write;
  fmt->data_write = fdefault_data_write;
  fmt->opt        = fdefault;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = fbin_align;
  fmt->end        = fdefault_end;
  fmt->org        = fbin_org;
  fmt->rmb        = fbin_rmb;
  fmt->setdp      = fdefault;
  fmt->test       = fdefault_test;
  fmt->tron       = fdefault;
  fmt->troff      = fdefault;
  fmt->Assert     = fdefault;
  fmt->endtst     = fdefault;
  fmt->fini       = fdefault_fini;
  fmt->first      = false;
  return true;
}

/**************************************************************************/
