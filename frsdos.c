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
* --------------------------------------------------------------------
*
*	Offset:	Type:	Value:
*	0	byte	0	; data block - multiple allowed
*	1:2	word	LENGTH
*	3:4	word	LOAD
*	5-xxx	byte[]	DATA LENGTH in size
*
*	0	byte	255	; exec block - one at end of file
*	1:2	word	0
*	3:4	word	EXEC address
*
****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "a09.h"

struct format_rsdos
{
  char     *basicf;
  char     *name;
  long      section_hdr;
  long      section_start;
  uint16_t  entry;
  uint16_t  line;
  uint16_t  strspace;
  uint16_t  staddr;
  uint16_t  usr;
  uint16_t  defusr[10];
  bool      endf;
  bool      org;
  bool      exec;
};

/**************************************************************************/

char const format_rsdos_usage[] =
        "\n"
        "RSDOS format options:\n"
        "\t-B filename\tfilename for BASIC code output\n"
        "\t-E\t\tinclude EXEC call\n"
        "\t-L line\t\tstarting line # (default 10)\n"
        "\t-N filename\tfilename for RSDOS\n"
        "\t-P size\t\tsize of string pool (default 200)\n";
        
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
      return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing");
      
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

static bool frsdos_pass_start(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  (void)a09;
  (void)pass;
  
  fmt->Float = freal__msfp;
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
      return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing");
      
    if (format->endf)
      return message(opd->a09,MSG_ERROR,"E0056: END section already written");
      
    if (!update_section_size(format,opd))
      return false;
      
    hdr[0] = 0xFF;
    hdr[1] = 0;
    hdr[2] = 0;
    
    if (sym == NULL)
    {
      if (format->exec)
        return message(opd->a09,MSG_ERROR,"E0111: missing label on END directive");
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
    
    if (format->name != NULL)
    {
      bool    defusr = false;
      char    buffer[249];
      char    rsdfn[16];
      size_t  idx;
      FILE   *basic;
      
      if (format->basicf == NULL)
      {
        char const *p = strrchr(opd->a09->outfile,'/');
        if (p == NULL)
          p = opd->a09->outfile;
        format->basicf = rsdfn;
        
        for ( idx = 0 ; (p[idx] != '\0') && (idx < sizeof(rsdfn)-1) ; idx++)
        {
          if (p[idx] == '.')
            format->basicf[idx] = '/';
          else
            format->basicf[idx] = p[idx];
        }
        format->basicf[idx] = '\0';
      }
      
      for (idx = 0 ; format->basicf[idx] != '\0'; idx++)
        format->basicf[idx] = toupper(format->basicf[idx]);
        
      idx = snprintf(
          buffer,
          sizeof(buffer),
          "%u CLEAR%u,%u:LOADM\"%s\"",
          format->line,
          format->strspace,
          format->entry - 1,
          format->basicf
      );
      assert(idx < sizeof(buffer));
      
      if (format->usr != 0)
      {
        idx += snprintf(&buffer[idx],sizeof(buffer) - idx,":POKE275,%u:POKE276,%u",format->usr >> 8,format->usr & 255);
        assert(idx < sizeof(buffer));
      }
      
      for (size_t i = 0 ; i < 10 ; i++)
      {
        if (format->defusr[i] != 0)
        {
          idx += snprintf(&buffer[idx],sizeof(buffer) - idx,":DEFUSR%zu=%u",i,format->defusr[i]);
          assert(idx < sizeof(buffer));
          defusr = true;
        }
      }
      
      if (defusr && (format->usr != 0))
        return message(opd->a09,MSG_ERROR,"E0072: can't use USR and DEFUSRn at the same time");
        
      if (format->exec)
      {
        idx += snprintf(&buffer[idx],sizeof(buffer) - idx,":EXEC%u",sym->value);
        assert(idx < sizeof(buffer));
      }
      
      basic = fopen(format->name,"w");
      if (basic == NULL)
        return message(opd->a09,MSG_ERROR,"E0070: %s: %s",format->basicf,strerror(errno));
      fwrite(buffer,1,idx,basic);
      fputc('\n',basic);
      fclose(basic);
    }
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
    
    if (opd->value.value < format->entry)
      format->entry = opd->value.value;
      
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
    return message(opd->a09,MSG_ERROR,"E0057: ORG directive missing");
  return fdefault_write(fmt,opd,buffer,len,instruction);
}

/**************************************************************************/

static bool frsdos_cmdline(struct format *fmt,struct a09 *a09,struct arg *arg,char c)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_RSDOS);
  assert(a09          != NULL);
  assert(arg          != NULL);
  assert(c            != '\0');
  
  struct format_rsdos *format = fmt->data;
  
  switch(c)
  {
    case 'B':
         if ((format->name = arg_arg(arg)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    case 'E':
         format->exec = true;
         break;
         
    case 'L':
         if (!arg_uint16_t(&format->line,arg,0,63999u))
           return message(a09,MSG_ERROR,"E0102: line number must be between 1 and 63999");
         break;
         
    case 'N':
         if ((format->basicf = arg_arg(arg)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    case 'P':
         if (!arg_uint16_t(&format->strspace,arg,0,22*1024))
           return message(a09,MSG_ERROR,"E0103: string space must be between 0 and 22528");
         break;
         
    default:
         return false;
  }
  
  return true;
}

/**************************************************************************/

static bool frsdos__opt(struct format *fmt,struct opcdata *opd,label *be)
{
  assert(fmt       != NULL);
  assert(fmt->data != NULL);
  assert(opd       != NULL);
  assert(opd->a09  != NULL);
  assert(be        != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct format_rsdos *format = fmt->data;
  struct value         val;
  
  if ((be->s != 5) && (memcmp(be->text,"BASIC",5) != 0))
    return true;
    
  if (opd->pass == 2)
  {
    label tmp;
    char  c = skip_space(opd->buffer);
    read_label(opd->buffer,&tmp,c);
    upper_label(&tmp);
    
    if ((tmp.s == 3) && (memcmp(tmp.text,"USR",3) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      format->usr = val.value;
    }
    
    else if ((tmp.s == 7) && (memcmp(tmp.text,"DEFUSR",6) == 0))
    {
      if (!isdigit(tmp.text[6]))
        return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      format->defusr[(size_t)(tmp.text[6] - '0')] = val.value;
    }
    
    else if ((tmp.s == 8) && (memcmp(tmp.text,"STRSPACE",8) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      format->strspace = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"LINE",4) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      format->line = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"INCR",4) == 0))
      return true;
      
    else if ((tmp.s == 4) && (memcmp(tmp.text,"CODE",4) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      format->line = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"EXEC",4) == 0))
      format->exec = true;
      
    else
      return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
  }
  
  return true;
}

/**************************************************************************/

bool format_rsdos_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_RSDOS,
    .cmdline    = frsdos_cmdline,
    .pass_start = frsdos_pass_start,
    .pass_end   = fdefault_pass,
    .write      = frsdos_write,
    .opt        = frsdos__opt,
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
    data->basicf        = NULL;
    data->name          = NULL;
    data->section_hdr   = 0;
    data->section_start = 0;
    data->entry         = 65535u;
    data->line          = 10;
    data->strspace      = 200;
    data->staddr        = 0;
    data->usr           = 0;
    data->endf          = false;
    data->org           = false;
    data->exec          = false;
    a09->format         = callbacks;
    a09->format.data    = data;
    memset(data->defusr,0,sizeof(data->defusr));
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
