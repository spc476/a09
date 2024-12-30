/****************************************************************************
*
*   Code to support the Motorola SREC format
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "a09.h"

struct format_srec
{
  char const    *S0file;
  uint16_t       addr;
  uint16_t       exec;
  size_t         recsize;
  size_t         idx;
  bool           endf;
  bool           execf;
  bool           override;
  unsigned char  buffer[252];
};

/**************************************************************************/

char const format_srec_usage[] =
        "\n"
        "SREC format options:\n"
        "\t-0 file\t\tcreate S0 record from file\n"
        "\t-E addr\t\texecution address\n"
        "\t-L addr\t\tinitial load address\n"
        "\t-O\t\tforce override of load and exec address\n"
        "\t-R size\t\tset #bytes per record (min=1, max=252, default=34)\n"
        "\n"
        "NOTE:\tS0 record will be truncated to max record size\n";
        
/**************************************************************************/

static void write_record(
        FILE                *out,
        int                  type,
        unsigned short       addr,
        unsigned char const  data[252],
        size_t               max
)
{
  unsigned char chksum;
  
  assert(out != NULL);
  assert(max <= 252);
  assert((type == '0') || (type == '1') || (type == '9'));
  
  fprintf(out,"S%c%02zX%04X",type,max+3,addr);
  chksum  = (unsigned char)max + 3 + (addr >> 8) + (addr & 0xFF);
  
  for (size_t i = 0 ; i < max ; i++)
  {
    fprintf(out,"%02X",data[i]);
    chksum += data[i];
  }
  
  fprintf(out,"%02X\n",~chksum & 0xFF);
}

/**************************************************************************/

static bool fsrec_cmdline(struct format *fmt,struct a09 *a09,struct arg *arg,char c)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(a09          != NULL);
  assert(arg          != NULL);
  assert(c            != '\0');
  
  struct format_srec *format = fmt->data;
  
  switch(c)
  {
    case 'R':
         if (!arg_size_t(&format->recsize,arg,1,252))
           return message(a09,MSG_ERROR,"E0067: record size must be between 1 and 252");
         break;
         
    case 'L':
         if (!arg_uint16_t(&format->addr,arg,0,65535u))
           return message(a09,MSG_ERROR,"E0069: address exceeds address space");
         break;
         
    case 'E':
         if (!arg_uint16_t(&format->exec,arg,0,65535u))
           return message(a09,MSG_ERROR,"E0069: address exceeds address space");
         format->execf = true;
         break;
         
    case 'O':
         format->override = true;
         break;
         
    case '0':
         if ((format->S0file = arg_arg(arg)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    default:
         return false;
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_pass_start(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    if (format->S0file != NULL)
    {
      FILE *fp = fopen(format->S0file,"rb");
      if (fp != NULL)
      {
        size_t bytes = fread(format->buffer,1,format->recsize,fp);
        size_t max   = bytes < format->recsize ? bytes : format->recsize;
        write_record(a09->out,'0',0,format->buffer,max);
        fclose(fp);
      }
      else
        return message(a09,MSG_ERROR,"E0070: %s: %s",format->S0file,strerror(errno));
    }
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_pass_end(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    if (!format->endf)
    {
      if (format->idx > 0)
        write_record(a09->out,'1',format->addr,format->buffer,format->idx);
      if (format->execf)
        write_record(a09->out,'9',format->exec,NULL,0);
    }
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_end(
        struct format       *fmt,
        struct opcdata      *opd,
        struct symbol const *sym
)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    if (format->endf)
      return message(opd->a09,MSG_ERROR,"E0056: END section already written");
      
    if (format->idx > 0)
      write_record(opd->a09->out,'1',format->addr,format->buffer,format->idx);
    if (!format->override && (sym != NULL))
      write_record(opd->a09->out,'9',sym->value,NULL,0);
    else if (sym != NULL)
      write_record(opd->a09->out,'9',format->exec,NULL,0);
    format->endf  = true;
    format->execf = true;
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_org(
        struct format  *fmt,
        struct opcdata *opd
)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    if (format->idx > 0)
    {
      write_record(opd->a09->out,'1',format->addr,format->buffer,format->idx);
      format->idx = 0;
    }
    
    if (!format->override)
      format->addr = opd->value.value;
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool write_data(
        struct format *format,
        FILE          *out,
        void const    *ptr,
        size_t         len
)
{
  assert(format          != NULL);
  assert(format->data    != NULL);
  assert(format->backend == BACKEND_SREC);
  assert(out             != NULL);
  assert(ptr             != NULL);
  
  struct format_srec *data   = format->data;
  char const         *buffer = ptr;
  
  for (size_t i = 0 ; i < len ; i++)
  {
    if (data->idx == data->recsize)
    {
      write_record(out,'1',data->addr,data->buffer,data->recsize);
      data->addr += data->recsize;
      data->idx   = 0;
    }
    data->buffer[data->idx++] = buffer[i];
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_write(struct format *fmt,struct opcdata *opd,void const *buffer,size_t len,bool instruction)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(opd          != NULL);
  assert(opd->pass    == 2);
  assert(buffer       != NULL);
  (void)instruction;
  
  return write_data(fmt,opd->a09->out,buffer,len);
}

/**************************************************************************/

static bool fsrec_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    for (size_t i = 0 ; i < opd->datasz ; i++)
    {
      if (format->idx == format->recsize)
      {
        write_record(opd->a09->out,'1',format->addr,format->buffer,format->recsize);
        format->addr += format->recsize;
        format->idx = 0;
      }
      format->buffer[format->idx++] = 0;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_rmb(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_SREC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_srec *format = fmt->data;
    
    if (opd->value.value == 0)
      return message(opd->a09,MSG_ERROR,"E0099: Can't reserve 0 bytes of memory");
      
    for (size_t i = 0 ; i < opd->value.value ; i++)
    {
      if (format->idx == format->recsize)
      {
        write_record(opd->a09->out,'1',format->addr,format->buffer,format->recsize);
        format->addr += format->recsize;
        format->idx = 0;
      }
      format->buffer[format->idx++] = 0;
    }
  }
  
  return true;
}

/**************************************************************************/

bool format_srec_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_SREC,
    .cmdline    = fsrec_cmdline,
    .pass_start = fsrec_pass_start,
    .pass_end   = fsrec_pass_end,
    .write      = fsrec_write,
    .opt        = fdefault__opt,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = fsrec_align,
    .end        = fsrec_end,
    .org        = fsrec_org,
    .rmb        = fsrec_rmb,
    .setdp      = fdefault,
    .test       = fdefault__test,
    .tron       = fdefault,
    .troff      = fdefault,
    .Assert     = fdefault,
    .endtst     = fdefault,
    .Float      = freal__ieee,
    .fini       = fdefault_fini,
    .data       = NULL,
  };
  
  assert(a09 != NULL);
  
  struct format_srec *data = malloc(sizeof(struct format_srec));
  if (data != NULL)
  {
    data->S0file     = NULL;
    data->addr       = 0;
    data->exec       = 0;
    data->recsize    = 34;
    data->idx        = 0;
    data->endf       = false;
    data->execf      = false;
    data->override   = false;
    a09->format      = callbacks;
    a09->format.data = data;
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
