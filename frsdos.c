/****************************************************************************
*
*   Code to support the Coco BASIC format
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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

char const format_rsdos_usage[] = "";

/**************************************************************************/

static bool update_section_size(struct format_rsdos *format,struct opcdata *opd)
{
  unsigned char datasize[2];
  long          pos;
  long          size;
  
  pos = ftell(opd->a09->out);
  if (pos == -1)
    return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  if (pos < format->section_start)
    return message(opd->a09,MSG_ERROR,"E0054: Internal error---no header written?");
  size = pos - format->section_start;
  if (size == 0)
    return message(opd->a09,MSG_ERROR,"E0057: segment has no data");
  if (size > UINT16_MAX)
    return message(opd->a09,MSG_ERROR,"E0055: object size too large");
  if (fseek(opd->a09->out,format->section_hdr + 1,SEEK_SET) == -1)
    return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  datasize[0] = size >> 8;
  datasize[1] = size & 255;
  if (fwrite(datasize,1,2,opd->a09->out) != 2)
    return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
  if (fseek(opd->a09->out,pos,SEEK_SET) == -1)
    return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  return true;
}

/**************************************************************************/

static bool block_zero_write(
        struct format_rsdos *format,
        struct opcdata      *opd,
        uint16_t             bsize
)
{
  assert(format != NULL);
  assert(opd    != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    /*----------------------------------------------------------------------
    ; If the alignment is less than the RSDOS program header size, then just
    ; pad out the current code segment with 0.  Otherwise, end the current
    ; code segment and start a new one, to save space in the executable.
    ;-----------------------------------------------------------------------*/
    
    if (bsize < 6)
    {
      if (fseek(opd->a09->out,bsize,SEEK_CUR) == -1)
        return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    }
    else
    {
      unsigned char hdr[5];
      long          pos;
      uint16_t      addr;
      
      if (!update_section_size(format,opd))
        return false;
        
      pos    = ftell(opd->a09->out);
      addr   = opd->a09->pc + bsize;
      hdr[0] = 0;
      hdr[1] = 0;
      hdr[2] = 0;
      hdr[3] = addr >> 8;
      hdr[4] = addr & 255;
      
      if (fwrite(hdr,1,sizeof(hdr),opd->a09->out) != sizeof(hdr))
        return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
      format->section_hdr   = pos;
      format->section_start = ftell(opd->a09->out);
    }
  }
  
  return true;
}

/**************************************************************************/

static bool frsdos_align(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  return block_zero_write(&fmt->rsdos,opd,opd->datasz);
}

/**************************************************************************/

static bool frsdos_end(union format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_RSDOS);
  
  if (opd->pass == 2)
  {
    struct format_rsdos *format = &fmt->rsdos;
    unsigned char        hdr[5];
    
    if (format->endf)
      return message(opd->a09,MSG_ERROR,"E0056: END section already written");
      
    if (!update_section_size(format,opd))
      return false;
      
    hdr[0] = 0xFF;
    hdr[1] = 0;
    hdr[2] = 0;
    
    if (sym == NULL)
    {
      hdr[3] = 0;
      hdr[4] = 0;
    }
    else
    {
      hdr[3] = sym->value >> 8;
      hdr[4] = sym->value & 255;
    }
    
    if (fwrite(hdr,1,sizeof(hdr),opd->a09->out) != sizeof(hdr))
      return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
    format->endf = true;
  }
  
  return true;
}

/**************************************************************************/

static bool frsdos_org(union format *fmt,struct opcdata *opd,uint16_t start,uint16_t last)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_RSDOS);
  (void)last;
  
  if (opd->pass == 2)
  {
    struct format_rsdos *format = &fmt->rsdos;
    long                 pos    = ftell(opd->a09->out);
    unsigned char        hdr[5];
    
    if (pos == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
      
    hdr[0] = 0;
    hdr[1] = 0;
    hdr[2] = 0;
    hdr[3] = start >> 8;
    hdr[4] = start & 255;
    
    if (pos > 0)
      if (!update_section_size(format,opd))
        return false;
        
    if (fwrite(hdr,1,sizeof(hdr),opd->a09->out) != sizeof(hdr))
      return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
      
    format->section_hdr   = pos;
    format->section_start = ftell(opd->a09->out);
  }
  return true;
}

/**************************************************************************/

static bool frsdos_rmb(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  return block_zero_write(&fmt->rsdos,opd,opd->value.value);
}

/**************************************************************************/

bool format_rsdos_init(struct format_rsdos *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->backend       = BACKEND_RSDOS;
  fmt->cmdline       = fdefault_cmdline;
  fmt->pass_start    = fdefault_pass;
  fmt->pass_end      = fdefault_pass;
  fmt->inst_write    = fdefault_inst_write;
  fmt->data_write    = fdefault_data_write;
  fmt->dp            = fdefault;
  fmt->code          = fdefault;
  fmt->align         = frsdos_align;
  fmt->end           = frsdos_end;
  fmt->org           = frsdos_org;
  fmt->rmb           = frsdos_rmb;
  fmt->setdp         = fdefault;
  fmt->test          = fdefault_test;
  fmt->tron          = fdefault;
  fmt->troff         = fdefault;
  fmt->trigger       = fdefault;
  fmt->endtst        = fdefault;
  fmt->fini          = fdefault_fini;
  fmt->section_hdr   = 0;
  fmt->section_start = 0;
  fmt->entry         = 0;
  fmt->endf          = false;
  return true;
}

/**************************************************************************/
