
/* GPL3+ */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "a09.h"

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
  return block_zero_write(&fmt->rsdos,opd,opd->datasz);
}

/**************************************************************************/

static bool frsdos_end(union format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
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
  return block_zero_write(&fmt->rsdos,opd,opd->value.value);
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
  (void)a09;
  
  fmt->dp            = frsdos_dp;
  fmt->code          = frsdos_code;
  fmt->align         = frsdos_align;
  fmt->end           = frsdos_end;
  fmt->org           = frsdos_org;
  fmt->rmb           = frsdos_rmb;
  fmt->setdp         = frsdos_setdp;
  fmt->section_hdr   = 0;
  fmt->section_start = 0;
  fmt->entry         = 0;
  fmt->endf          = false;
  return true;
}

/**************************************************************************/
