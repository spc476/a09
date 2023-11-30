/****************************************************************************
*
*   Code to provide default (nul) behavior for backends.
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

#include "a09.h"

/**************************************************************************/

bool fdefault_cmdline(union format *fmt,int *pi,char *argv[])
{
  (void)fmt;
  (void)pi;
  (void)argv;
  return false;
}

/**************************************************************************/

bool fdefault_pass(union format *fmt,struct a09 *a09,int pass)
{
  (void)fmt;
  (void)pass;
  (void)a09;
  return true;
}

/**************************************************************************/

bool fdefault(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
  return true;
}

/**************************************************************************/

bool fdefault_end(union format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  (void)fmt;
  (void)opd;
  (void)sym;
  return true;
}

/**************************************************************************/

bool fdefault_org(union format *fmt,struct opcdata *opd,uint16_t start,uint16_t last)
{
  (void)fmt;
  (void)opd;
  (void)start;
  (void)last;
  return true;
}

/**************************************************************************/

bool fdefault_inst_write(union format *fmt,struct opcdata *opd)
{
  (void)fmt;
  assert(opd       != NULL);
  assert(opd->pass == 2);
  
  if (fwrite(opd->bytes,1,opd->sz,opd->a09->out) != opd->sz)
  {
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
    else
      return message(opd->a09,MSG_ERROR,"E0041: truncated output to object file");
  }
  return true;
}

/**************************************************************************/

bool fdefault_data_write(
        union format   *fmt,
        struct opcdata *opd,
        char const     *buffer,
        size_t          len
)
{
  (void)fmt;
  assert(opd       != NULL);
  assert(opd->pass == 2);
  assert(buffer    != NULL);
  
  if (fwrite(buffer,1,len,opd->a09->out) != len)
  {
    if (ferror(opd->a09->out))
      return message(opd->a09,MSG_ERROR,"E0040: failed writing object file");
    else
      return message(opd->a09,MSG_ERROR,"E0041: truncated output to object file");
  }
  return true;
}

/**************************************************************************/
