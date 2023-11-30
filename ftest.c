/****************************************************************************
*
*   The Unit Test backend
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

bool format_test_init(struct format_test *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->cmdline    = fdefault_cmdline;
  fmt->pass_start = fdefault_pass;
  fmt->pass_end   = fdefault_pass;
  fmt->inst_write = fdefault_inst_write;
  fmt->data_write = fdefault_data_write;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = fdefault;
  fmt->end        = fdefault_end;
  fmt->org        = fdefault_org;
  fmt->setdp      = fdefault;
  return true;
}

/**************************************************************************/
