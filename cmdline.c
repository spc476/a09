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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

/**************************************************************************/

char *cmd_opt(int *pi,int argc,char *argv[])
{
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  if (argv[*pi][2] == '\0')
  {
    if (*pi + 1 == argc)
      return NULL;
    else
      return argv[++(*pi)];
  }
  else
    return &argv[*pi][2];
}

/**************************************************************************/

bool cmd_unsigned_long(
        unsigned long int *pv,
        int               *pi,
        int                argc,
        char              *argv[],
        unsigned long int  low,
        unsigned long int  high
)
{
  assert(pv   != NULL);
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(low  <  high);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  char *opt = cmd_opt(pi,argc,argv);
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

bool cmd_size_t(
        size_t        *pv,
        int           *pi,
        int            argc,
        char          *argv[],
        unsigned long  low,
        unsigned long  high
)
{
  assert(pv   != NULL);
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(low  <  high);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  unsigned long int value;
  
  if (!cmd_unsigned_long(&value,pi,argc,argv,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/

bool cmd_uint16_t(
        uint16_t          *pv,
        int               *pi,
        int                argc,
        char              *argv[],
        unsigned long int  low,
        unsigned long int  high
)
{
  assert(pv   != NULL);
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(low  <  high);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  unsigned long int value;
  
  if (!cmd_unsigned_long(&value,pi,argc,argv,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/

bool cmd_uint8_t(
      uint8_t           *pv,
      int               *pi,
      int                argc,
      char              *argv[],
      unsigned long int  low,
      unsigned long int  high
)
{
  assert(pv   != NULL);
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(low  <  high);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  unsigned long int value;
  
  if (!cmd_unsigned_long(&value,pi,argc,argv,low,high))
    return false;
  *pv = value;
  return true;
}

/**************************************************************************/
