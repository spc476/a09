/****************************************************************************
*
*   Code to output Color Basic code
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
* CB: USR
*       entry:  FP0    - argument (no registers are defined)
*		VALTYP - type of argument
*       exit:   FP0    - result (no registers defined)
*
* ECB/DECB: USRn
*       entry:  X      - #FP0 ($004F) if passed number
*                        string descriptor if passed string
*               A      - 00 if number, 0xFF if string
*               B      - string length if A == 0xFF
*		VALTYP - same as A
*		FP0    - argument to USRn
*       exit:   FP0    - result (no registers defined)
*
****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************

line length as text: 249
line # 0 .. 63999

10 DATA 123...
20 CLEAR200,8192:FORA=8192TO9001:READB:POKEA,B:NEXT
30 POKE275,hi:POKE276,lo
40 DEFUSR0=addr:DEFUSR1=addr...

options         starting line #
                string space
                increment
                add (via 'A09)
                
                .opt    basic usr label         ; CB    @ $0113
                .opt    basic defusr0 label     ; ECB   DEFUSRn=addr
                .opt    basic strspace 200
                .opt    basic line 10
                .opt    basic incr 10
                .opt    basic code 20

**************************************************************************/

struct format_basic
{
  int      idx;
  uint16_t cline;
  uint16_t dline;
  uint16_t incr;
  uint16_t strspace;
  uint16_t staddr;
  uint16_t usr;
  uint16_t defusr[10];
  bool     org;
  bool     init;
  bool     exec;
  char     buffer[249];
};

/**************************************************************************/

char const format_basic_usage[] =
        "\n"
        "BASIC format options:\n"
        "\t-C line\t\tstarting line # for code (automatic after DATA)\n"
        "\t-E\t\tinclude EXEC call\n"
        "\t-L line\t\tstarting line # for DATA (default 10)\n"
        "\t-N incr\t\tline increment (default 10)\n"
        "\t-P size\t\tsize of string pool (default 200)\n"
        "\n";
        
/**************************************************************************/

static void write_byte(FILE *out,struct format_basic *basic,unsigned char byte)
{
  assert(out   != NULL);
  assert(basic != NULL);
  
  int len = snprintf(&basic->buffer[basic->idx],sizeof(basic->buffer) - basic->idx,"%u,",byte);
  if ((unsigned)len > sizeof(basic->buffer) - basic->idx)
  {
    assert(basic->idx > 1);
    assert((unsigned)basic->idx <= sizeof(basic->buffer));
    fwrite(basic->buffer,1,basic->idx - 1,out);
    fputc('\n',out);
    basic->dline += basic->incr;
    basic->idx    = snprintf(basic->buffer,sizeof(basic->buffer),"%u DATA",basic->dline);
    assert((unsigned)basic->idx < sizeof(basic->buffer));
    len          = snprintf(&basic->buffer[basic->idx],basic->idx,"%u,",byte);
    assert((unsigned)(basic->idx + len) < sizeof(basic->buffer));
  }
  
  basic->idx += len;
}

/**************************************************************************/

static bool fbasic_cmdline(struct format *fmt,struct a09 *a09,struct arg *arg,char c)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(a09          != NULL);
  assert(arg          != NULL);
  assert(c            != '\0');
  
  struct format_basic *basic = fmt->data;
  
  switch(c)
  {
    case 'C':
         if (!arg_uint16_t(&basic->cline,arg,0,63999u))
           return message(a09,MSG_ERROR,"E0102: line number must be between 1 and 63999");
         break;
         
    case 'E':
         basic->exec = true;
         break;
         
    case 'L':
         if (!arg_uint16_t(&basic->dline,arg,0,63999u))
           return message(a09,MSG_ERROR,"E0102: line number must be between 1 and 63999");
         break;
         
    case 'N':
         if (!arg_uint16_t(&basic->incr,arg,0,65535u))
           return message(a09,MSG_ERROR,"E0101: line increment must be between 1 and 65535");
         break;
         
    case 'P':
         if (!arg_uint16_t(&basic->strspace,arg,0,22*1024))
           return message(a09,MSG_ERROR,"E0103: string space must be between 0 and 22528");
         break;
         
    default:
         return false;
  }
  
  return true;
}

/**************************************************************************/

static bool fbasic_pass_start(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  (void)a09;
  assert((pass == 1) || (pass == 2));
  
  fmt->Float = freal__msfp;
  
  if (pass == 2)
  {
    struct format_basic *basic = fmt->data;
    if (!basic->init)
    {
      basic->idx = snprintf(basic->buffer,sizeof(basic->buffer),"%u DATA",basic->dline);
      basic->init = true;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool fbasic_write(struct format *fmt,struct opcdata *opd,void const *buffer,size_t len,bool instruction)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(opd          != NULL);
  assert(opd->pass    == 2);
  assert(buffer       != NULL);
  (void)instruction;
  
  struct format_basic *basic = fmt->data;
  unsigned char const *buf   = buffer;
  
  for (size_t i = 0 ; i < len ; i++)
    write_byte(opd->a09->out,basic,buf[i]);
    
  return true;
}

/**************************************************************************/

static bool fbasic__opt(struct format *fmt,struct opcdata *opd,label *be)
{
  assert(fmt       != NULL);
  assert(fmt->data != NULL);
  assert(opd       != NULL);
  assert(opd->a09  != NULL);
  assert(be        != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct format_basic *basic = fmt->data;
  struct value         val;
  
  if ((be->s != 5) && (memcmp(be->text,"BASIC",5) != 0))
    return true;
    
  if (opd->pass == 2)
  {
    label tmp;
    char c = skip_space(opd->buffer);
    read_label(opd->buffer,&tmp,c);
    upper_label(&tmp);
    
    if ((tmp.s == 3) && (memcmp(tmp.text,"USR",3) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->usr = val.value;
    }
    
    else if ((tmp.s == 7) && (memcmp(tmp.text,"DEFUSR",6) == 0))
    {
      if (!isdigit(tmp.text[6]))
        return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->defusr[(size_t)(tmp.text[6] - '0')] = val.value;
    }
    
    else if ((tmp.s == 8) && (memcmp(tmp.text,"STRSPACE",8) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->strspace = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"LINE",4) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->dline = val.value;
      basic->idx   = snprintf(basic->buffer,sizeof(basic->buffer),"%u DATA",basic->dline);
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"INCR",4) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->incr = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"CODE",4) == 0))
    {
      if (!expr(&val,opd->a09,opd->buffer,opd->pass))
        return false;
      basic->cline = val.value;
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"EXEC",4) == 0))
      basic->exec = true;
      
    else
      return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
  }
  
  return true;
}

/**************************************************************************/

static bool fbasic_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(opd          != NULL);
  assert(opd->a09     != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_basic *basic = fmt->data;
    
    for (size_t i = 0 ; i < opd->datasz ; i++)
      write_byte(opd->a09->out,basic,0);
  }
  return true;
}

/**************************************************************************/

static bool fbasic_end(struct format *fmt,struct opcdata *opd,struct symbol const *sym)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  (void)sym;
  
  if (opd->pass == 2)
  {
    struct format_basic *basic  = fmt->data;
    bool                 defusr = false;
    
    if (!basic->org)
      return message(opd->a09,MSG_ERROR,"E0039: missing value for ORG");
      
    fwrite(basic->buffer,1,basic->idx - 1,opd->a09->out);
    fputc('\n',opd->a09->out);
    
    if (basic->cline == 64000)
      basic->cline = basic->dline + basic->incr;
      
    fprintf(
        opd->a09->out,
        "%u CLEAR%u,%u:FORA=%uTO%u:READB:POKEA,B:NEXT",
        basic->cline,
        basic->strspace,
        basic->staddr - 1,
        basic->staddr,
        opd->a09->pc - 1
    );
    
    if (basic->usr != 0)
      fprintf(opd->a09->out,":POKE275,%u:POKE276,%u",basic->usr >> 8,basic->usr & 255);
      
    basic->idx = 0;
    
    for (size_t i = 0 ; i < 10 ; i++)
    {
      if (basic->defusr[i] != 0)
      {
        int len = snprintf(&basic->buffer[basic->idx],sizeof(basic->buffer) - basic->idx,":DEFUSR%zu=%u",i,basic->defusr[i]);
        assert((unsigned)(basic->idx + len) < sizeof(basic->buffer));
        basic->idx += len;
        defusr      = true;
      }
    }
    
    if (defusr)
    {
      if (basic->usr != 0)
        return message(opd->a09,MSG_ERROR,"E0072: can't use USR and DEFUSRn at the same time");
        
      fwrite(basic->buffer,1,basic->idx - 1,opd->a09->out);
    }
    
    if (basic->exec)
    {
      if (sym == NULL)
        return message(opd->a09,MSG_ERROR,"E0111: missing label on END directive");
      fprintf(opd->a09->out,":EXEC%u",sym->value);
    }
    
    fputc('\n',opd->a09->out);
  }
  
  return true;
}

/**************************************************************************/

static bool fbasic_org(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct format_basic *basic = fmt->data;
  
  if (basic->org)
    return message(opd->a09,MSG_ERROR,"E0104: BASIC format does not support multiple ORG directives");
    
  if (opd->pass == 2)
  {
    if (!basic->org)
      basic->staddr = opd->value.value;
    basic->org = true;
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool fbasic_rmb(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(opd          != NULL);
  assert(opd->a09     != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->value.value == 0)
    return message(opd->a09,MSG_ERROR,"E0099: Can't reserve 0 bytes of memory");
    
  if (opd->pass == 2)
  {
    struct format_basic *basic = fmt->data;
    
    for (size_t i = 0 ; i < opd->value.value ; i++)
      write_byte(opd->a09->out,basic,0);
  }
  return true;
}

/**************************************************************************/

bool format_basic_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_BASIC,
    .cmdline    = fbasic_cmdline,
    .pass_start = fbasic_pass_start,
    .pass_end   = fdefault_pass,
    .write      = fbasic_write,
    .opt        = fbasic__opt,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = fbasic_align,
    .end        = fbasic_end,
    .org        = fbasic_org,
    .rmb        = fbasic_rmb,
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
  
  struct format_basic *basic = malloc(sizeof(struct format_basic));
  if (basic != NULL)
  {
    basic->idx       = 0;
    basic->cline     = 64000; /* no defined line number */
    basic->dline     = 10;
    basic->incr      = 10;
    basic->strspace  = 200;
    basic->staddr    = 0;
    basic->usr       = 0;
    basic->org       = false;
    basic->init      = false;
    basic->exec      = false;
    basic->buffer[0] = '\0';
    a09->format      = callbacks;
    a09->format.data = basic;
    memset(basic->defusr,0,sizeof(basic->defusr));
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
