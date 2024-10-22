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
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

/*

line length as text: 249

10 'A09 START
20 DATA 123...
20 CLEAR200,8192:FORA=8192TO9001:READB:POKEA,B:NEXT
30 POKE275,hi:POKE276,lo
40 DEFUSR0=addr:DEFUSR1=addr...
50 EXEC8192
60 CSAVEM"name",start,end,transfer
70 SAVEM"name",start,end,transfer
80 'A09 END

binary format:	$FF <len:16>
                <nextline:16><linenum:16> <data:8...>
                
                $26 $2C $00 $0A $95 '200,8192:'
                                $80 'A' $B3 '8192' $A5 '9001:'
                                $8D 'B:'
                                $92 'A,B:'
                                $8B ':'
                                $A2 '8291' $00
                $27 $24 $00 $14 $86 '18,57,123,123...' $00
                $00 $00
                
options		starting line #
                string space
                increment
                ascii vs. tokenized
                add (via ' ASM [exec] [incr#] or REM ASM [exec] [incr#])
                loadaddr ($2601 default)
                
                
                
                
.OPT

;		.opt	basic usr label		; CB	@ $0113
;		.opt	basic defusr0 label	; ECB	DEFUSRn=addr
;		.opt	basic defusr1 label	; ECB
;		.opt	basic strspace 200
;		.opt	basic exec label




*/

/**************************************************************************/

#define CLEAR	"\x95"
#define FOR	"\x80"
#define EQ	"\xB3"
#define TO	"\xA5"
#define READ	"\x8D"
#define POKE	"\x92"
#define NEXT	"\x8B"
#define EXEC	"\xA2"
#define DATA	"\x86"
#define REM	"\x82"
#define REMq	"\x83"
#define DEFUSR	"\xB9\xFF\x83"

struct format_basic
{
  char const *fadd;
  char const *disk;
  char const *cassette;
  int         idx;
  uint16_t    line;
  uint16_t    incr;
  uint16_t    strspace;
  uint16_t    staddr;
  uint16_t    usr;
  uint16_t    defusr[10];
  bool        org;
  char        buffer[249];
};

char const format_basic_usage[] =
        "\n"
        "BASIC format options:\n"
        "\t-A file\t\tadd code to specified BASIC program\n"
        "\t-C name\tadd code to save 'name' program to cassette\n"
        "\t-I incr\t\tline increment (default 10)\n"
        "\t-L line\t\tstarting line # (default 10)\n"
        "\t-P size\t\tsize of string pool (default 200)\n"
        "\t-S name\tadd code to save 'name' program to disk\n"
        "\n";
        
/**************************************************************************/

static bool fbasic_cmdline(struct format *fmt,struct a09 *a09,int argc,int *pi,char *argv[])
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_BASIC);
  assert(a09          != NULL);
  assert(argc         >  0);
  assert(pi           != NULL);
  assert(*pi          >  0);
  assert(argv         != NULL);
  
  struct format_basic *basic = fmt->data;
  
  switch(argv[*pi][1])
  {
    case 'A':
         if ((basic->fadd = cmd_opt(pi,argc,argv)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    case 'C':
         if ((basic->cassette = cmd_opt(pi,argc,argv)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
         
    case 'I':
         if (!cmd_uint16_t(&basic->incr,pi,argc,argv,0,65535u))
           return message(a09,MSG_ERROR,"E9999: line increment must be between 1 and 65535");
         break;
         
    case 'L':
         if (!cmd_uint16_t(&basic->line,pi,argc,argv,0,65535u))
           return message(a09,MSG_ERROR,"E9999: line number must be between 1 and 65535");
         break;
         
    case 'P':
         if (!cmd_uint16_t(&basic->strspace,pi,argc,argv,0,22*1024))
           return message(a09,MSG_ERROR,"E9999: string space must be between 0 and 22528");
         break;

    case 'S':
         if ((basic->disk = cmd_opt(pi,argc,argv)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    default:
         usage(argv[0]);
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
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  
  if (pass == 2)
  {
    struct format_basic *basic = fmt->data;
    basic->idx = snprintf(basic->buffer,sizeof(basic->buffer),"%u DATA",(unsigned)basic->line);
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
  char const          *buf   = buffer;
  
  for (size_t i = 0 ; i < len ; i++)
  {
    int len = snprintf(&basic->buffer[basic->idx],sizeof(basic->buffer) - basic->idx,"%u,",buf[i] & 255);
    if ((unsigned)len > sizeof(basic->buffer) - basic->idx)
    {
      assert(basic->idx > 1);
      fwrite(basic->buffer,1,basic->idx - 1,opd->a09->out);
      fputc('\n',opd->a09->out);
      basic->line += basic->incr;
      basic->idx   = snprintf(basic->buffer,sizeof(basic->buffer),"%u DATA",(unsigned)basic->line);
      len          = snprintf(&basic->buffer[basic->idx],basic->idx,"%u,",buf[i] & 255);
    }
    
    basic->idx += len;
  }
  
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
    
    else
      return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
  }
  
  return true;
}

/**************************************************************************/

static bool fbasic_align(struct format *fmt,struct opcdata *opd)
{
  (void)fmt;
  (void)opd;
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
  
  if (opd->pass == 2)
  {
    struct format_basic *basic  = fmt->data;
    bool                 defusr = false;
    
    if (!basic->org)
      return message(opd->a09,MSG_ERROR,"E9999: missing ORG for backend\n");
    
    fwrite(basic->buffer,1,basic->idx - 1,opd->a09->out);
    fputc('\n',opd->a09->out);
    
    fprintf(
        opd->a09->out,
        "%u CLEAR%u,%u:FORA=%uTO%u:READB:POKEA,B:NEXT\n",
        (unsigned)basic->line,
        (unsigned)basic->strspace,
        (unsigned)basic->staddr - 1,
        (unsigned)basic->staddr,
        (unsigned)opd->a09->pc - 1
    );
    
    if (basic->usr != 0)
    {
      basic->line += basic->incr;
      fprintf(
          opd->a09->out,
          "%u POKE275,%u:POKE276,%u\n",
          (unsigned)basic->line,
          (unsigned)basic->usr >> 8,
          (unsigned)basic->usr & 255
      );
    }
    
    basic->idx = snprintf(basic->buffer,sizeof(basic->buffer),"%u ",(unsigned)basic->line + basic->incr);
    
    for (size_t i = 0 ; i < 10 ; i++)
    {
      if (basic->defusr[i] != 0)
      {
        int len = snprintf(&basic->buffer[basic->idx],sizeof(basic->buffer) - basic->idx,"DEFUSR%zu=%u:",i,(unsigned)basic->defusr[i]);
        assert((unsigned)(basic->idx + len) < sizeof(basic->buffer));
        basic->idx += len;
        defusr      = true;
      }
    }
    
    if (defusr)
    {
      fwrite(basic->buffer,1,basic->idx -1 , opd->a09->out);
      fputc('\n',opd->a09->out);
      basic->line += basic->incr;
    }
    
    if ((sym != NULL) && (basic->cassette == NULL) && (basic->disk == NULL))
    {
      basic->line += basic->incr;
      fprintf(opd->a09->out,"%u EXEC%u\n",(unsigned)basic->line,(unsigned)sym->value);
    }
    
    if (basic->cassette != NULL)
    {
      if (sym == NULL)
        return message(opd->a09,MSG_ERROR,"E9999: missing entry point on END");
        
      basic->line += basic->incr;
      fprintf(
          opd->a09->out,
          "%u CSAVEM\"%s\",%u,%u,%u\n",
          (unsigned)basic->line,
          basic->cassette,
          (unsigned)basic->staddr,
          (unsigned)opd->a09->pc,
          (unsigned)sym->value
      );
    }
    
    if (basic->disk != NULL)
    {
      if (sym == NULL)
        return message(opd->a09,MSG_ERROR,"E9999: missing entry point on END");
      
      basic->line += basic->incr;
      fprintf(
          opd->a09->out,
          "%u SAVEM\"%s\",%u,%u,%u\n",
          (unsigned)basic->line,
          basic->disk,
          (unsigned)basic->staddr,
          (unsigned)opd->a09->pc,
          (unsigned)sym->value
      );
    }
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

  if (opd->pass == 2)
  {
    struct format_basic *basic = fmt->data;    
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
  (void)fmt;
  (void)opd;
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
    basic->fadd      = NULL;
    basic->disk      = NULL;
    basic->cassette  = NULL;
    basic->idx       = 0;
    basic->line      = 10;
    basic->incr      = 10;
    basic->strspace  = 200;
    basic->staddr    = 0;
    basic->usr       = 0;
    basic->org       = false;
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
