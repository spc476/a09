/****************************************************************************
*
*   Useful routines for parsing the command line
*   Copyright (C) 2024 Sean Conner
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
#include <assert.h>

#include "a09.h"

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#  pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif

/**************************************************************************/

#ifndef NDEBUG
  int check_arg(struct arg *arg)
  {
    assert(arg                      != NULL);
    assert(arg->argv                != NULL);
    assert(arg->argv[0]             != NULL);
    assert(arg->argv[arg->argc - 1] != NULL);
    assert(arg->argc                >  0);
    assert(arg->ci                  >  0);
    assert(arg->ci                  <= arg->argc);
    return 1;
  }
#endif

/**************************************************************************/

void arg_init(struct arg *arg,char **argv,int argc)
{
  assert(arg  != NULL);
  assert(argv != NULL);
  assert(argc >  0);
  
  arg->argv = argv;
  arg->argc = argc;
  arg->ci   = 1; /* skip program name */
  arg->si   = 0;
}

/**************************************************************************/

char arg_next(struct arg *arg)
{
  assert(check_arg(arg));
  
  if (arg->ci == arg->argc)
    return '\0';
    
  if (arg->si == 0)
  {
    assert(arg->argv[arg->ci][0] != '\0');
    if (arg->argv[arg->ci][0] != '-')
      return '\0';
    arg->si++;
    if (arg->argv[arg->ci][1] == '\0')
      return '\0';
  }
  
  if (arg->argv[arg->ci][arg->si] == '\0')
  {
    arg->ci++;
    arg->si = 0;
    if ((arg->ci == arg->argc) || (arg->argv[arg->ci][0] != '-'))
      return '\0';
  }
  
  if (arg->argv[arg->ci][arg->si] == '-')
    arg->si++;
    
  if (arg->argv[arg->ci][arg->si] == '\0')
    return '\0';
  else
    return arg->argv[arg->ci][arg->si++];
}

/**************************************************************************/

char *arg_arg(struct arg *arg)
{
  char *p;
  
  assert(check_arg(arg));
  
  if (arg->ci == arg->argc)
    return NULL;
    
  if (arg->argv[arg->ci][arg->si] == '\0')
  {
    if (arg->ci == arg->argc - 1)
      return NULL;
    else
      p = arg->argv[++arg->ci];
  }
  else
    p = &arg->argv[arg->ci][arg->si];
    
  arg->ci++;
  arg->si = 0;
  assert(arg->ci <= arg->argc);
  return p;
}

/**************************************************************************/

int arg_done(struct arg *arg)
{
  assert(check_arg(arg));
  if (arg->si == 0)
    return arg->ci;
  else
    return arg->ci + 1;
}

/**************************************************************************/

bool arg_unsigned_long(
        unsigned long int *pv,
        struct arg        *arg,
        unsigned long int  low,
        unsigned long int  high
)
{
  assert(pv  != NULL);
  assert(low <  high);
  assert(check_arg(arg));
  
  char *opt = arg_arg(arg);
  if (opt == NULL)
    return false;
  *pv = strtoul(opt,NULL,0);
  
  if (*pv < low)
    return false;
  if (*pv > high)
    return false;
  return true;
}

/**************************************************************************/

bool arg_size_t(
        size_t        *pv,
        struct arg    *arg,
        unsigned long  low,
        unsigned long  high
)
{
  assert(pv  != NULL);
  assert(low <  high);
  assert(check_arg(arg));
  
  unsigned long int value;
  
  if (!arg_unsigned_long(&value,arg,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/

bool arg_uint16_t(
        uint16_t          *pv,
        struct arg        *arg,
        unsigned long int  low,
        unsigned long int  high
)
{
  assert(pv   != NULL);
  assert(low  <  high);
  assert(check_arg(arg));
  
  unsigned long int value;
  
  if (!arg_unsigned_long(&value,arg,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/

bool arg_uint8_t(
      uint8_t           *pv,
      struct arg        *arg,
      unsigned long int  low,
      unsigned long int  high
)
{
  assert(pv   != NULL);
  assert(low  <  high);
  assert(check_arg(arg));
  
  unsigned long int value;
  
  if (!arg_unsigned_long(&value,arg,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/
