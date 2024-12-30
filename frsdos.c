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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "a09.h"

struct format_rsdos
{
  long     section_hdr;
  long     section_start;
  uint16_t entry;
  bool     endf;
  bool     org;
};

/**************************************************************************/

char const format_rsdos_usage[] = "";

/**************************************************************************/

static bool update_section_size(struct format_rsdos *format,struct opcdata *opd)
{
  unsigned char datasize[2];
  long          pos;
  long          size;
  
  assert(format != NULL);
  assert(opd    != NULL);
  
  pos = ftell(opd->a09->out);
  if (pos == -1)
    return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  if (pos < format->section_start)
    return message(opd->a09,MSG_ERROR,"E0054: Internal error---no header written?");
  size = pos - format->section_start;
  if (size == 0)
  {
    if (fseek(opd->a09->out,format->section_hdr,SEEK_SET) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    return true;
  }
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
    if (!format->org)
      return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing\n");
      
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

static bool frsdos_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(opd          != NULL);
  
  return block_zero_write(fmt->data,opd,opd->datasz);
}

/**************************************************************************/

static bool frsdos_end(struct format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_rsdos *format = fmt->data;
    unsigned char        hdr[5];
    
    if (!format->org)
      return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing\n");
      
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

static bool frsdos_org(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_rsdos *format = fmt->data;
    long                 pos;
    unsigned char        hdr[5];
    
    format->org = true;
    
    if (!update_section_size(format,opd))
      return false;
      
    pos = ftell(opd->a09->out);
    if (pos == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
      
    hdr[0] = 0;
    hdr[1] = 0;
    hdr[2] = 0;
    hdr[3] = opd->value.value >> 8;
    hdr[4] = opd->value.value & 255;
    
    if (fwrite(hdr,1,sizeof(hdr),opd->a09->out) != sizeof(hdr))
      return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
      
    format->section_hdr   = pos;
    format->section_start = ftell(opd->a09->out);
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool frsdos_rmb(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(opd          != NULL);
  
  if (opd->value.value == 0)
    return message(opd->a09,MSG_ERROR,"E0099: Can't reserve 0 bytes of memory");
  return block_zero_write(fmt->data,opd,opd->value.value);
}

/**************************************************************************/

static bool frsdos_write(struct format *fmt,struct opcdata *opd,void const *buffer,size_t len,bool instruction)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(opd          != NULL);
  assert(opd->pass    == 2);
  assert(buffer       != NULL);
  
  struct format_rsdos *format = fmt->data;
  if (!format->org)
    return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing\n");
  return fdefault_write(fmt,opd,buffer,len,instruction);
}

/**************************************************************************/

bool format_rsdos_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_RSDOS,
    .cmdline    = fdefault_cmdline,
    .pass_start = fdefault_pass,
    .pass_end   = fdefault_pass,
    .write      = frsdos_write,
    .opt        = fdefault__opt,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = frsdos_align,
    .end        = frsdos_end,
    .org        = frsdos_org,
    .rmb        = frsdos_rmb,
    .setdp      = fdefault,
    .test       = fdefault__test,
    .tron       = fdefault,
    .troff      = fdefault,
    .Assert     = fdefault,
    .endtst     = fdefault,
    .Float      = freal__msfp,
    .fini       = fdefault_fini,
    .data       = NULL,
  };
  
  assert(a09 != NULL);
  
  struct format_rsdos *data = malloc(sizeof(struct format_rsdos));
  if (data != NULL)
  {
    data->section_hdr   = 0;
    data->section_start = 0;
    data->entry         = 0;
    data->endf          = false;
    data->org           = false;
    a09->format         = callbacks;
    a09->format.data    = data;
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
