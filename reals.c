/****************************************************************************
*
*   Functions to parse the various formats of floating point values.
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
* ---------------------------------------------------------------------
*
* The IEEE-754 double (which pretty much all systems use these days) is
* formatted as:
*
*           [s:1] [exp:11 (biased by 1023)] [frac:52]
*
* There is the MC6839 ROM, a position independent from Motorola, which
* supports IEEE-754 floats.  It's 8K, so it's fairly large as 6809 libraries
* go, but it does a good, if slow, job of supporting IEEE-754.
*
* The floating point format for the Color Computer (written by Microsoft)
* is:
*
*           [exp:8 (biased by 129)] [s:1] [frac:31]
*
* Both assume the floating point fraction has a leading 1 and thus, it's not
* part of the actual storage format.  So all we have to do is kind of
* massage the bits around a bit.
*
* There is a third floating point library I found for the 6809, found here:
*
*	https://lennartb.home.xs4all.nl/m6809.html
*
* The only difference between this and the Microsoft floating point format,
* is the exponent bias---it uses $80 instead of $81.
*
* NOTE: The floating point system on the Color Computer doesn't have the
*       concepts of +-inf or NaN---those will generate an error.
*
****************************************************************************/

#include <math.h>
#include <string.h>

#include "a09.h"

/**************************************************************************/

bool freal__ieee(struct format *fmt,struct opcdata *opd)
{
  (void)fmt;
  assert(opd         != NULL);
  assert(opd->a09    != NULL);
  assert(opd->buffer != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  opd->data = true;
  
  while(true)
  {
    struct fvalue fv;
    
    skip_space(opd->buffer);
    opd->buffer->ridx--;
    
    if (opd->op->opcode == 0)
    {
      if (!rexpr(&fv,opd->a09,opd->buffer,opd->pass,false))
        return false;
      opd->datasz += sizeof(float);
      if (opd->pass == 2)
      {
        uint32_t x;
        char     buf[sizeof(float)];
        
        memcpy(&x,&fv.value.f,sizeof(float));
        buf[0] = (x >> 24) & 255;
        buf[1] = (x >> 16) & 255;
        buf[2] = (x >>  8) & 255;
        buf[3] = (x      ) & 255;
        for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < sizeof(buf)) ; i++ , opd->sz++)
          opd->bytes[opd->sz] = buf[i];
        if (opd->a09->obj)
          if (!opd->a09->format.write(&opd->a09->format,opd,buf,sizeof(buf),DATA))
            return false;
      }
    }
    else
    {
      if (!rexpr(&fv,opd->a09,opd->buffer,opd->pass,true))
        return false;
      opd->datasz += sizeof(double);
      if (opd->pass == 2)
      {
        uint64_t x;
        char     buf[sizeof(double)];
        
        memcpy(&x,&fv.value.d,sizeof(double));
        buf[0] = (x >> 56) & 255;
        buf[1] = (x >> 48) & 255;
        buf[2] = (x >> 40) & 255;
        buf[3] = (x >> 32) & 255;
        buf[4] = (x >> 24) & 255;
        buf[5] = (x >> 16) & 255;
        buf[6] = (x >>  8) & 255;
        buf[7] = (x      ) & 255;
        for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < sizeof(buf)) ; i++ , opd->sz++)
          opd->bytes[opd->sz] = buf[i];
        if (opd->a09->obj)
          if (!opd->a09->format.write(&opd->a09->format,opd,buf,sizeof(buf),DATA))
            return false;
      }
    }
       
    char c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return true;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"E0034: missing comma");
  }
}

/**************************************************************************/

static bool freal_40b(struct format *fmt,struct opcdata *opd,int bias)
{
  (void)fmt;
  assert(opd          != NULL);
  assert(opd->a09     != NULL);
  assert(opd->buffer  != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert((bias == 128) || (bias == 129));
  
  opd->data = true;
  
  while(true)
  {
    struct fvalue fv;

    skip_space(opd->buffer);
    opd->buffer->ridx--;
    
    if (!rexpr(&fv,opd->a09,opd->buffer,opd->pass,true))
      return false;
    opd->datasz += 5;
    
    if (opd->pass == 2)
    {
      if (!isnormal(fv.value.d) && (fv.value.d != 0.0))
        return message(opd->a09,MSG_ERROR,"E0090: floating point exceeds range of Color Computer");
        
      char     decbfloat[6];
      uint64_t x;
      
      memcpy(&x,&fv.value.d,sizeof(double));
      uint64_t frac = (x & 0x000FFFFFFFFFFFFFuLL);
      bool     sign = (int64_t)x < 0;
      int      exp  = (int)((x >> 52) & 0x7FFuLL);
      
      assert(exp < 0x7FF);
      assert((exp > 0) || ((exp == 0) && (frac == 0)));
      if (exp > 0)
        exp = exp - 1023 + bias; /* unbias from IEEE-754 to DECB float */
      if (exp > 255)
        return message(opd->a09,MSG_ERROR,"E0090: floating point exceeds range of Color Computer");
        
      decbfloat[0] = exp;
      decbfloat[1] = (frac >> 45) & 255;
      decbfloat[2] = (frac >> 37) & 255;
      decbfloat[3] = (frac >> 29) & 255;
      decbfloat[4] = (frac >> 21) & 255;
      
      if (opd->op->opcode == 1)
        message(opd->a09,MSG_WARNING,"W0019: double floats not supported, using single float");

      decbfloat[1] |= sign ? 0x80 : 0x00;
      
      for (size_t i = 0 ; (opd->sz < sizeof(opd->bytes)) && (i < 5) ; i++,opd->sz++)
        opd->bytes[opd->sz] = decbfloat[i];
      if (opd->a09->obj)
        if (!opd->a09->format.write(&opd->a09->format,opd,decbfloat,5,DATA))
          return false;
    }
    
    char c = skip_space(opd->buffer);
    if ((c == ';') || (c == '\0'))
      return true;
    if (c != ',')
      return message(opd->a09,MSG_ERROR,"E0034: missing comma");
  }
}

/**************************************************************************/

bool freal__msfp(struct format *fmt,struct opcdata *opd)
{
  (void)fmt;
  assert(opd          != NULL);
  assert(opd->a09     != NULL);
  assert(opd->buffer  != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  return freal_40b(fmt,opd,129);
}

/**************************************************************************/

bool freal__lbfp(struct format *fmt,struct opcdata *opd)
{
  (void)fmt;
  assert(opd          != NULL);
  assert(opd->a09     != NULL);
  assert(opd->buffer  != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));

  return freal_40b(fmt,opd,128);
}

/**************************************************************************/
