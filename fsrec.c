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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "a09.h"

/**************************************************************************/

char const format_srec_usage[] =
        "\n"
        "SREC format options:\n"
        "\t-R size\t\tset #bytes per record (min=1, max=252, default=34)\n"
        "\t-0 file\t\tcreate S0 record from file\n"
        "\t-L addr\t\tinitial load address\n"
        "\t-E addr\t\texecution address\n"
        "\t-O\t\tforce override of load and exec address\n"
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

static bool fsrec_cmdline(union format *fmt,int *pi,char *argv[])
{
  assert(fmt  != NULL);
  assert(pi   != NULL);
  assert(*pi  >  0);
  assert(argv != NULL);
  assert(fmt->backend == BACKEND_SREC);
  
  struct format_srec *format = &fmt->srec;
  int                 i      = *pi;
  unsigned long int   value;
  
  switch(argv[i][1])
  {
    case 'R':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
           
         if (value == 0)
         {
           fprintf(stderr,"%s: E0067: minimum record size: 1\n",MSG_ERROR);
           exit(1);
         }
         else if (value > 252)
         {
           fprintf(stderr,"%s: E0068: maximum record size: 252\n",MSG_ERROR);
           exit(1);
         }
         format->recsize = value;
         break;
         
    case 'L':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
         if (value > USHRT_MAX)
         {
           fprintf(stderr,"%s: E0069: address exceeds address space\n",MSG_ERROR);
           exit(1);
         }
         format->addr = value;
         break;
         
    case 'E':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
         if (value > USHRT_MAX)
         {
           fprintf(stderr,"%s: E0069: address exceeds address space\n",MSG_ERROR);
           exit(1);
         }
         format->exec = value;
         format->execf = true;
         break;
         
    case 'O':
         format->override = true;
         break;
         
    case '0':
         if (argv[i][2] == '\0')
           format->S0file = argv[++i];
         else
           format->S0file = &argv[i][2];
         break;
         
    default:
         return false;
  }
  
  *pi = i;
  return true;
}

/**************************************************************************/

static bool fsrec_pass_start(union format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  
  if (pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
    if (format->S0file != NULL)
    {
      FILE               *fp     = fopen(format->S0file,"rb");
      if (fp != NULL)
      {
        size_t bytes = fread(format->buffer,1,format->recsize,fp);
        size_t max   = bytes < format->recsize ? bytes : format->recsize;
        write_record(a09->out,'0',0,format->buffer,max);
        fclose(fp);
      }
      else
      {
        fprintf(stderr,"%s: E0070: %s: %s\n",MSG_ERROR,format->S0file,strerror(errno));
        return false;
      }
    }
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_pass_end(union format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  assert((pass == 1) || (pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  
  if (pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
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
        union format        *fmt,
        struct opcdata      *opd,
        struct symbol const *sym
)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert(sym != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  
  if (opd->pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
    if (format->endf)
      return message(opd->a09,MSG_ERROR,"E0056: END section already written");
      
    if (format->idx > 0)
      write_record(opd->a09->out,'1',format->addr,format->buffer,format->idx);
    if (!format->override && (sym != NULL))
      write_record(opd->a09->out,'9',sym->value,NULL,0);
    else
      write_record(opd->a09->out,'9',format->exec,NULL,0);
    format->endf  = true;
    format->execf = true;
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_org(
        union format   *fmt,
        struct opcdata *opd,
        uint16_t        start,
        uint16_t        last
)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  (void)last;
  
  if (opd->pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
    if (format->idx > 0)
    {
      write_record(opd->a09->out,'1',format->addr,format->buffer,format->idx);
      format->idx = 0;
    }
    
    if (!format->override)
      format->addr = start;
  }
  
  return true;
}

/**************************************************************************/

static bool write_data(
        struct format_srec *format,
        FILE               *out,
        void const         *ptr,
        size_t              len
)
{
  assert(format != NULL);
  assert(out    != NULL);
  assert(ptr    != NULL);
  assert(format->backend == BACKEND_SREC);
  
  char const *buffer = ptr;
  
  for (size_t i = 0 ; i < len ; i++)
  {
    if (format->idx == format->recsize)
    {
      write_record(out,'1',format->addr,format->buffer,format->recsize);
      format->addr += format->recsize;
      format->idx   = 0;
    }
    format->buffer[format->idx++] = buffer[i];
  }
  
  return true;
}

/**************************************************************************/

static bool fsrec_inst_write(union format *fmt,struct opcdata *opd)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(opd->pass == 2);
  assert(!opd->data);
  assert(fmt->backend == BACKEND_SREC);
  
  return write_data(&fmt->srec,opd->a09->out,opd->bytes,opd->sz);
}

/**************************************************************************/

static bool fsrec_data_write(
        union format   *fmt,
        struct opcdata *opd,
        char const     *buffer,
        size_t          len
)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(buffer    != NULL);
  assert(opd->pass == 2);
  assert(opd->data);
  assert(fmt->backend == BACKEND_SREC);
  
  return write_data(&fmt->srec,opd->a09->out,buffer,len);
}

/**************************************************************************/

static bool fsrec_align(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  
  if (opd->pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
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

static bool fsrec_rmb(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_SREC);
  
  if (opd->pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    
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

bool format_srec_init(struct format_srec *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->backend    = BACKEND_SREC;
  fmt->cmdline    = fsrec_cmdline;
  fmt->pass_start = fsrec_pass_start;
  fmt->pass_end   = fsrec_pass_end;
  fmt->inst_write = fsrec_inst_write;
  fmt->data_write = fsrec_data_write;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = fsrec_align;
  fmt->end        = fsrec_end;
  fmt->org        = fsrec_org;
  fmt->rmb        = fsrec_rmb;
  fmt->setdp      = fdefault;
  fmt->test       = fdefault_test;
  fmt->tron       = fdefault;
  fmt->troff      = fdefault;
  fmt->Assert     = fdefault;
  fmt->endtst     = fdefault;
  fmt->fini       = fdefault_fini;
  fmt->addr       = 0;
  fmt->exec       = 0;
  fmt->recsize    = 34;
  fmt->idx        = 0;
  fmt->endf       = false;
  fmt->execf      = false;
  fmt->override   = false;
  return true;
}

/**************************************************************************/
