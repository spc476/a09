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
#include <assert.h>

#include "a09.h"

/**************************************************************************/

char const format_srec_usage[] =
        "\n"
        "SREC format options:\n"
        "\t-R size\tset record size (max=252)\n"
        "\t-0 file\tcreate S0 record from file\n"
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
  
  assert(out  != NULL);
  assert(data != NULL);
  assert(max  >= 3);
  assert(max  <= 255);
  assert((type == '0') || (type == '1') || (type == '9'));
  
  fprintf(out,"S%c%02zX%04X",type,max,addr);
  chksum  = (unsigned char)max + (addr >> 8) + (addr & 0xFF);
  max    -= 3;
  
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
         if (value > 252)
         {
           fprintf(stderr,"%s: E9999: bad record size\n",MSG_ERROR);
           exit(1);
         }
         format->recsize = value;
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
  
  if (pass == 2)
  {
    struct format_srec *format = &fmt->srec;
    FILE               *fp     = fopen(format->S0file,"rb");
    if (fp != NULL)
    {
      size_t bytes = fread(format->buffer,1,format->recsize,fp);
      size_t max   = (bytes < format->recsize ? bytes : format->recsize) + 3;
      write_record(a09->out,'0',0,format->buffer,max);
      fclose(fp);
    }
    else
    {
      fprintf(stderr,"%s: E9999: %s: %s\n",MSG_ERROR,format->S0file,strerror(errno));
      return false;
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

  fmt->cmdline    = fsrec_cmdline;
  fmt->pass_start = fsrec_pass_start;
  fmt->pass_end   = fdefault_pass;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = fdefault;
  fmt->end        = fdefault_end;
  fmt->org        = fdefault_org;
  fmt->rmb        = fdefault;
  fmt->setdp      = fdefault;
  fmt->addr       = 0;
  fmt->recsize    = 37;
  fmt->idx        = 0;
  return true;
}

/**************************************************************************/
