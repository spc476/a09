/****************************************************************************
*
*   Code to support the Dragon computer EXEC format
*   Copyright (C) 2026 Sean Conner
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
* --------------------------------------------------------------------
*
* http://dragon32.info/info/binformt.html
*
* BAS and BIN are essentially the same - both start with an 9 byte header of
* the following format:
*
*     Offset:  Type:   Value:
*        0       byte    $55     Constant
*        1       byte    FILETYPE
*        2:3     word    LOAD
*        4:5     word    LENGTH
*        6:7     word    EXEC
*        8       byte    $AA     Constant
*        9-xxx   byte[]  DATA
*
* FILETYPE is
*         $01     for BAS
*         $02     for BIN
*
* Remainder are undefined under standard Dragon DOS.  The type $03 may have
* been used in DosPlus from Phil Scott for a gapped m/c binary.  (??  check)
*
****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "a09.h"

struct format_dragon
{
  uint16_t load;
  uint16_t exec;
  bool     org;
};

/**************************************************************************/

char const format_dragon_usage[] = "";

/**************************************************************************/

static bool fdragon_pass_start(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  
  fmt->Float = freal__msfp; /* XXX okay? */
  
  if (pass == 2)
  {
    if (fseek(a09->out,9,SEEK_SET) == -1)
      return message(a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  return true;
}

/**************************************************************************/

static bool fdragon_pass_end(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (pass == 2)
  {
    struct format_dragon *dragon = fmt->data;
    long int              len    = ftell(a09->out);
    unsigned char         hdr[9];
    
    if (len == -1)
      return message(a09,MSG_ERROR,"E0038: %s",strerror(errno));
    if (fseek(a09->out,0,SEEK_SET) == -1)
      return message(a09,MSG_ERROR,"E0038: %s",strerror(errno));
      
    assert(len >= 9);
    len    -= 9; /* remove header length */
    if (len > 65527)
      return message(a09,MSG_ERROR,"E0112: length exceeds memory space");
      
    hdr[0] = 0x55;
    hdr[1] = 0x02;
    hdr[2] = dragon->load >>   8;
    hdr[3] = dragon->load &  255;
    hdr[4] = (len >> 8)   &  255;
    hdr[5] = len          &  255;
    hdr[6] = dragon->exec >>   8;
    hdr[7] = dragon->exec &  255;
    hdr[8] = 0xAA;
    
    if (fwrite(hdr,1,sizeof(hdr),a09->out) != sizeof(hdr))
      return message(a09,MSG_ERROR,"E0040: failed writing object file");
    if (fseek(a09->out,0,SEEK_END) == -1)
      return message(a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  
  return true;
}

/**************************************************************************/

static bool fdragon_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(opd          != NULL);
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

static bool fdragon_end(struct format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_dragon *dragon = fmt->data;
    
    if (sym == NULL)
      return message(opd->a09,MSG_ERROR,"E0111: missing label on END directive");
    dragon->exec = sym->value;
  }
  
  return true;
}

/**************************************************************************/

static bool fdragon_org(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_dragon *dragon = fmt->data;
    if (dragon->org)
    {
      long int delta = opd->value.value - opd->a09->pc;
      if (fseek(opd->a09->out,delta,SEEK_CUR) == -1)
        return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
    }
    else
    {
      dragon->load = opd->value.value;      
      dragon->org  = true;
    }
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool fdragon_rmb(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  (void)fmt;
  
  if (opd->pass == 2)
  {
    if (opd->value.value == 0)
      return message(opd->a09,MSG_ERROR,"E0099: Can't reserve 0 bytes of memory");
    if (fseek(opd->a09->out,opd->value.value,SEEK_CUR) == -1)
      return message(opd->a09,MSG_ERROR,"E0038: %s",strerror(errno));
  }
  return true;
}

/**************************************************************************/

static bool fdragon_write(struct format *fmt,struct opcdata *opd,void const *buffer,size_t len,bool instruction)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_DRAGON);
  assert(opd          != NULL);
  assert(opd->pass    == 2);
  assert(buffer       != NULL);
  
  struct format_dragon *format = fmt->data;
  if (!format->org)
    return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing");
  return fdefault_write(fmt,opd,buffer,len,instruction);
}

/**************************************************************************/

bool format_dragon_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_DRAGON,
    .cmdline    = fdefault_cmdline,
    .pass_start = fdragon_pass_start,
    .pass_end   = fdragon_pass_end,
    .write      = fdragon_write,
    .opt        = fdefault__opt,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = fdragon_align,
    .end        = fdragon_end,
    .org        = fdragon_org,
    .rmb        = fdragon_rmb,
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
  
  struct format_dragon *data = malloc(sizeof(struct format_dragon));
  if (data != NULL)
  {
    data->load       = 0;
    data->exec       = 0;
    data->org        = false;
    a09->format      = callbacks;
    a09->format.data = data;
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
