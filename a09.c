/****************************************************************************
*
*   Main program for as09
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************
* Checking assumptions at compile time.  Technique from
* https://jorenar.com/blog/less-known-c#compile-time-assumption-checking-using-enums
***************************************************************************/

enum
{
  BYTE_ADDRESSABILITY = 1 / (CHAR_BIT         == 8),
  TWOS_COMPLEMENT     = 1 / (SCHAR_MIN        == -128),
  IEEE_754_FLOAT      = 1 / (sizeof(uint32_t) == sizeof(float)),
  IEEE_754_DOUBLE     = 1 / (sizeof(uint64_t) == sizeof(double)),
};

/**************************************************************************/

char const MSG_DEBUG[]   = "debug";
char const MSG_WARNING[] = "warning";
char const MSG_ERROR[]   = "error";

/**************************************************************************/

static bool check_warning_tag(struct a09 *a09,char const *tag,div_t *pres)
{
  assert(a09  != NULL);
  assert(tag  != NULL);
  assert(pres != NULL);
  
  if ((tag[0] == 'W') && isdigit(tag[1]) && isdigit(tag[2]) && isdigit(tag[3]) && isdigit(tag[4]) && !isdigit(tag[5]))
  {
    assert(sizeof(a09->nowarn) < INT_MAX);
    int num = (tag[1] - '0') * 1000
            + (tag[2] - '0') *  100
            + (tag[3] - '0') *   10
            + (tag[4] - '0');
    *pres   = div(num,CHAR_BIT);
    
    assert(pres->quot >= 0);
    assert(pres->quot <  (int)(sizeof(a09->nowarn)));
    assert(pres->rem  >= 0);
    assert(pres->rem  <  CHAR_BIT);
    
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0012: invalid warning '%s'",tag);
}

/**************************************************************************/

bool enable_warning(struct a09 *a09,char const *tag)
{
  div_t res;
  
  assert(a09 != NULL);
  assert(tag != NULL);
  
  if (!check_warning_tag(a09,tag,&res))
    return false;
  a09->nowarn[res.quot] &= (~(1 << res.rem));
  return true;
}

/**************************************************************************/

bool disable_warning(struct a09 *a09,char const *tag)
{
  div_t res;
  
  assert(a09 != NULL);
  assert(tag != NULL);
  
  if (!check_warning_tag(a09,tag,&res))
    return false;
  a09->nowarn[res.quot] |= 1 << res.rem;
  return true;
}

/**************************************************************************/

bool message(struct a09 *a09,char const *restrict tag,char const *restrict fmt,...)
{
  assert(a09 != NULL);
  assert(tag != NULL);
  assert(fmt != NULL);
  assert(
             ((tag == MSG_WARNING) && (fmt[0] == 'W'))
          || ((tag == MSG_ERROR)   && (fmt[0] == 'E'))
          || ((tag == MSG_DEBUG))
        );
        
  va_list ap;
  
  if ((tag == MSG_DEBUG) && !a09->debug)
    return true;
    
  if (fmt[0] == 'W')
  {
    div_t res;
    bool  rc = check_warning_tag(a09,fmt,&res);
    assert(rc);
    (void)rc;
    if ((a09->nowarn[res.quot] & (1 << res.rem)))
      return true;
  }
  
  if (a09->lnum > 0)
    fprintf(stderr,"%s:%zu: %s: ",a09->infile,a09->lnum,tag);
  else
    fprintf(stderr,"%s: %s: ",a09->infile,tag);
  va_start(ap,fmt);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wformat-nonliteral"
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
  vfprintf(stderr,fmt,ap);
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wformat-nonliteral"
#endif
  va_end(ap);
  fprintf(stderr,"\n");
  return tag != MSG_ERROR;
}

/**************************************************************************/

char *add_file_dep(struct a09 *a09,char const *filename)
{
  char   **deps;
  char    *name;
  size_t   len;
  
  assert(a09      != NULL);
  assert(filename != NULL);
  
  for (size_t i = 0 ; i < a09->ndeps ; i++)
  {
    if (strcmp(a09->deps[i],filename) == 0)
      return a09->deps[i];
  }
  
  deps = realloc(a09->deps,(a09->ndeps + 1) * sizeof(char *));
  if (deps == NULL)
  {
    message(a09,MSG_ERROR,"E0046: out of memory");
    return NULL;
  }
  
  len  = strlen(filename) + 1;
  name = malloc(len);
  if (name == NULL)
  {
    message(a09,MSG_ERROR,"E0046: out of memory");
    return NULL;
  }
  
  memcpy(name,filename,len);
  name[len - 1]          = '\0';
  a09->deps               = deps;
  a09->deps[a09->ndeps++] = name;
  
  return name;
}

/**************************************************************************/

static bool add_include_file(struct a09 *a09,char const *filename)
{
  char   **includes;
  char    *name;
  size_t   len;
  
  assert(a09      != NULL);
  assert(filename != NULL);
  
  includes = realloc(a09->includes,(a09->nincs + 1) * sizeof(char *));
  if (includes == NULL)
    return message(a09,MSG_ERROR,"E0046: out of memory");
  
  len  = strlen(filename) + 1;
  name = malloc(len);
  if (name == NULL)
    return message(a09,MSG_ERROR,"E0046: out of memory");
    
  memcpy(name,filename,len);
  name[len - 1]               = '\0';
  a09->includes               = includes;
  a09->includes[a09->nincs++] = name;
  
  return true;
}

/**************************************************************************/

bool read_line(FILE *in,struct buffer *buffer)
{
  assert(in     != NULL);
  assert(buffer != NULL);
  
  buffer->widx = 0;
  buffer->ridx = 0;
  
  while(!feof(in))
  {
    int c = fgetc(in);
    if (c == EOF)  return false;
    if (c == '\n') break;
    if (c == '\t')
    {
      for (size_t num = 8 - (buffer->widx & 7) , j = 0 ; j < num ; j++)
      {
        if (buffer->widx == sizeof(buffer->buf)-1)
          return false;
        buffer->buf[buffer->widx++] = ' ';
      }
    }
    else if (isprint(c))
    {
      if (buffer->widx == sizeof(buffer->buf)-1)
        return false;
      buffer->buf[buffer->widx++] = c;
    }
    else
      return false;
  }
  
  assert(buffer->widx < sizeof(buffer->buf));
  buffer->buf[buffer->widx] = '\0';
  return true;
}

/**************************************************************************/

bool read_label(struct buffer *buffer,label *label,char c)
{
  bool toolong = false;
  
  assert(buffer != NULL);
  assert(label  != NULL);
  assert(isgraph(c));
  
  size_t i = 0;
  
  while((c == '.') || (c == '_') || (c == '$') || isalnum(c))
  {
    if (i < sizeof(label->text))
      label->text[i++] = c;
    else
      toolong = true;
    c = buffer->buf[buffer->ridx++];
  }
  
  label->s = i;
  assert(buffer->ridx > 0);
  buffer->ridx--;
  return toolong;
}

/**************************************************************************/

bool parse_label(label *res,struct buffer *buffer,struct a09 *a09,int pass)
{
  assert(res    != NULL);
  assert(buffer != NULL);
  assert(a09    != NULL);
  assert((pass == 1) || (pass == 2));
  
  label tmp;
  char  c = buffer->buf[buffer->ridx];
  bool  toolong;
  
  if ((c == '.') || (c == '_') || isalpha(c))
  {
    buffer->ridx++;
    toolong = read_label(buffer,&tmp,c);
    if (tmp.text[0] == '.')
    {
      memcpy(res->text,a09->label.text,a09->label.s);
      size_t s = min(tmp.s,sizeof(tmp.text) - a09->label.s);
      assert(s <= sizeof(res->text));
      memcpy(&res->text[a09->label.s],tmp.text,s);
      res->s = a09->label.s + s;
      assert(res->s <= sizeof(res->text));
      if ((pass == 1) && (a09->label.s + tmp.s > sizeof(res->text)))
        message(a09,MSG_WARNING,"W0001: label '%.*s' exceeds %zu characters",res->s,res->text,sizeof(res->text));
    }
    else
    {
      if (toolong && (pass == 1))
        message(a09,MSG_WARNING,"W0001: label '%.*s' exceeds %zu characters",tmp.s,tmp.text,sizeof(tmp.text));
      *res = tmp;
    }
    return true;
  }
  else
    return false;
}

/**************************************************************************/

void upper_label(label *res)
{
  assert(res != NULL);
  for (size_t i = 0 ; i < res->s ; i++)
    res->text[i] = toupper(res->text[i]);
}

/**************************************************************************/

bool parse_op(struct buffer *buffer,struct opcode const **pop)
{
  assert(buffer != NULL);
  assert(pop    != NULL);
  
  char top[10];
  char c = 0;
  
  for (size_t i = 0 ; i < 10 ; i++)
  {
    c = buffer->buf[buffer->ridx];
    if (isspace(c) || (c == '\0'))
    {
      top[i] = '\0';
      *pop   = op_find(top);
      return *pop != NULL;
    }
    else if (!isalpha(c) && !isdigit(c) && (c != '.'))
      break;
      
    top[i] = toupper(c);
    buffer->ridx++;
  }
  
  assert(c != 0);
  return false;
}

/**************************************************************************/

char skip_space(struct buffer *buffer)
{
  assert(buffer != NULL);
  
  char c;
  
  do
  {
    c = buffer->buf[buffer->ridx];
    if (c == '\0')
      return c;
    buffer->ridx++;
  } while (isspace(c));
  
  return c;
}

/**************************************************************************/

bool print_list(struct a09 *a09,struct opcdata *opd,bool labelonly)
{
  assert(a09 != NULL);
  assert(opd != NULL);
  
  if ((a09->list != NULL) && (!opd->includehack))
  {
    if ((opd->sz == 0) && (opd->datasz == 0))
    {
      if (labelonly && opd->label.s > 0)
        fprintf(a09->list,"%04X:             ",a09->pc);
      else
        fprintf(a09->list,"                  ");
    }
    else
    {
      if (opd->data)
      {
        size_t c = 0; // # byte columns printed
        fprintf(a09->list,"%04X: ",a09->pc);
        for (size_t i = 0 ; i < opd->sz ; i++,c++)
          fprintf(a09->list,"%02X",opd->bytes[i]);
        for ( ; c < sizeof(opd->bytes) ; c++)
          fprintf(a09->list,"  ");
      }
      else
      {
        size_t i = 0; // index into opd->bytes[]
        size_t c = 0; // # byte columns printed
        
        fprintf(a09->list,"%04X: %02X",a09->pc,opd->bytes[i++]);
        c++;
        
        if ((opd->bytes[0] == 0x10) || (opd->bytes[0] == 0x11))
        {
          fprintf(a09->list,"%02X  ",opd->bytes[i++]);
          c++;
        }
        else
        {
          fprintf(a09->list,"    ");
          c++;
        }
        
        for ( ; i < opd->sz  ; i++)
        {
          fprintf(a09->list,"%02X",opd->bytes[i]);
          c++;
        }
        
        for ( ; c < 5 ; c++)
          fprintf(a09->list,"  ");
      }
    }
    
    fprintf(a09->list," %5zu | %s\n",a09->lnum,a09->inbuf.buf);
  }
  opd->includehack = false;
  return true;
}

/**************************************************************************/

static bool parse_line(struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  int            c;
  bool           rc;
  struct opcdata opd =
  {
    .a09    = a09,
    .op     = NULL,
    .buffer = buffer,
    .label  = { .s = 0 },
    .pass   = pass,
    .sz     = 0,
    .data   = false,
    .datasz = 0,
    .mode   = AM_INHERENT,
    .value  =
    {
      .value        = 0,
      .bits         = 0,
      .unknownpass1 = false,
      .defined      = false,
      .external     = false,
    },
    .bits        = 16,
    .pcrel       = false,
    .includehack = false,
  };
  
  if (parse_label(&opd.label,&a09->inbuf,a09,pass))
  {
    if (pass == 1)
    {
      if (symbol_add(a09,&opd.label,a09->pc) == NULL)
        return false;
    }
    else if (pass == 2)
    {
      /*--------------------------------------------------------------
      ; On pass 2, all ADDRESS labels should be the same.  If they're not,
      ; there's an internal error somewhere, so abort the assembly to avoid
      ; troubleshooting an assembler error in the program we're writing.
      ;---------------------------------------------------------------*/
      struct symbol *sym = symbol_find(a09,&opd.label);
      if (sym == NULL)
        return message(a09,MSG_ERROR,"E0001: Internal error---'%.*s' should exist, but doesn't",a09->label.s,a09->label.text);
      if ((sym->type == SYM_ADDRESS) && (sym->value != a09->pc))
        return message(a09,MSG_ERROR,"E0002: Internal error---out of phase;\n\t'%.*s' = %04X pass 1, %04X pass 2",a09->label.s,a09->label.text,sym->value,a09->pc);
    }
    
    /*-------------------------------------------------------------------
    ; Check to see if we have a global label in case we have a local label
    ;--------------------------------------------------------------------*/
    
    if ((pass == 1) && (opd.label.text[0] == '.') && (a09->label.s == 0))
      message(a09,MSG_WARNING,"W0010: missing initial label");
      
    /*-----------------------------------
    ; store the current global label
    ;------------------------------------*/
    
    a09->label = opd.label;
    char *p    = memchr(a09->label.text,'.',a09->label.s);
    if (p != NULL)
      a09->label.s = (unsigned char)(p - a09->label.text);
  }
  
  c = skip_space(&a09->inbuf);
  
  if ((c == ';') || (c == '\0'))
    return print_list(a09,&opd,true);
    
  a09->inbuf.ridx--; // ungetc()
  
  if (!parse_op(&a09->inbuf,&opd.op))
    return message(a09,MSG_ERROR,"E0003: unknown opcode");
    
  rc = opd.op->func(&opd);
  
  if (rc)
  {
    print_list(a09,&opd,false);
    
    if (opd.data)
      a09->pc += opd.datasz;
    else
      a09->pc += opd.sz;
      
    if ((pass == 2) && (opd.sz > 0) && (opd.datasz == 0) && a09->obj)
      return a09->format.write(&a09->format,&opd,opd.bytes,opd.sz,INSTRUCTION);
  }
  
  return rc;
}

/**************************************************************************/

bool assemble_pass(struct a09 *a09,int pass)
{
  assert(a09     != NULL);
  assert(a09->in != NULL);
  assert(pass    >= 1);
  assert(pass    <= 2);
  
  label saved = a09->label;
  
  rewind(a09->in);
  a09->lnum  = 0;
  a09->label = (label){ .s = 0 , .text = { '\0' } };
  
  message(a09,MSG_DEBUG,"Pass %d",pass);
  
  if (a09->runtests)
    if (!test_pass_start(a09,pass))
      return false;
      
  if (!a09->format.pass_start(&a09->format,a09,pass))
    return false;
    
  while(!feof(a09->in))
  {
    if (!read_line(a09->in,&a09->inbuf))
      break;
    a09->lnum++;
    if (!parse_line(a09,&a09->inbuf,pass))
      return false;
  }
  
  if (!a09->format.pass_end(&a09->format,a09,pass))
    return false;
    
  if (a09->runtests)
    if (!test_pass_end(a09,pass))
      return false;
      
  a09->label = saved;
  return true;
}

/**************************************************************************/

static bool nowarnlist(struct a09 *a09,char const *warnings)
{
  assert(a09 != NULL);
  
  if (warnings == NULL)
    return message(a09,MSG_ERROR,"E0068: missing option argument");
    
  while(true)
  {
    if (!disable_warning(a09,warnings))
      return message(a09,MSG_ERROR,"E0058: improper warning tag '%.4s'",warnings);
    warnings += 5;
    if (*warnings == '\0')
      break;
    if (*warnings++ != ',')
      return message(a09,MSG_ERROR,"E0023: missing expected comma");
  }
  
  return true;
}

/**************************************************************************/

static int usage(char const *prog)
{
  fprintf(
           stderr,
           "usage: %s [options] [files...]\n"
           "\t-I dir\t\tadd directory for include files\n"
           "\t-M\t\tgenerate Makefile dependencies on stdout\n"
           "\t-T\t\trun tests with TAP output\n"
           "\t-c file\t\tcore file (of 6809 VM) name (only if -t specified)\n"
           "\t-d\t\tdebug output\n"
           "\t-f format\toutput format (bin)\n"
           "\t-h\t\thelp (this text)\n"
           "\t-l listfile\tlist filename\n"
           "\t-n Wxxxx\tsupress the given warnings\n"
           "\t-o filename\toutput filename\n"
           "\t-r\t\trandomize the testing order (only if -t specified)\n"
           "\t-t\t\trun tests\n"
           "\n"
           "\tformats: bin rsdos srec basic\n"
           "%s"
           "%s"
           "%s"
           "%s"
           "",
           prog,
           format_bin_usage,
           format_rsdos_usage,
           format_srec_usage,
           format_basic_usage
         );
  return -1;
}

/**************************************************************************/

static int parse_command(int argc,char *argv[],struct a09 *a09)
{
  struct arg arg;
  char       c;
  
  assert(argc >= 1);
  assert(argv != NULL);
  assert(a09  != NULL);
  
  arg_init(&arg,argv,argc);
  
  while((c = arg_next(&arg)) != '\0')
  {
    char const *file;
    char const *format;
    
    switch(c)
    {
      case 'I':
           if ((file = arg_arg(&arg)) == NULL)
           {
             message(a09,MSG_ERROR,"E0068: missing option argument");
             return -1;
           }
           if (!add_include_file(a09,file))
             return -1;
           break;
           
      case 'M':
           a09->mkdeps = true;
           break;
           
      case 'T':
           a09->runtests = true;
           a09->tapout   = true;
           break;
           
      case 'c':
           if ((a09->corefile = arg_arg(&arg)) == NULL)
           {
             message(a09,MSG_ERROR,"E0068: missing option argument");
             return -1;
           }
           break;
           
      case 'd':
           a09->debug = true;
           break;
           
      case 'f':
           format = arg_arg(&arg);
           
           if (format == NULL)
           {
             message(a09,MSG_ERROR,"E0068: missing option argument");
             return -1;
           }
           else if (strcmp(format,"bin") == 0)
           {
             if (!format_bin_init(a09))
               return -1;
           }
           else if (strcmp(format,"rsdos") == 0)
           {
             if (!format_rsdos_init(a09))
               return -1;
           }
           else if (strcmp(format,"srec") == 0)
           {
             if (!format_srec_init(a09))
               return -1;
           }
           else if (strcmp(format,"basic") == 0)
           {
             if (!format_basic_init(a09))
               return -1;
           }
           else
           {
             message(a09,MSG_ERROR,"E0053: format '%s' not supported",format);
             return -1;
           }
           break;
           
      case 'h':
           return usage(argv[0]);
           
      case 'l':
           if ((a09->listfile = arg_arg(&arg)) == NULL)
           {
             message(a09,MSG_ERROR,"E0068: missing option argument");
             return -1;
           }
           break;
           
      case 'n':
          if (!nowarnlist(a09,arg_arg(&arg)))
            return -1;
           break;
           
      case 'o':
           if ((a09->outfile = arg_arg(&arg)) == NULL)
           {
             message(a09,MSG_ERROR,"E0068: missing option argument");
             return -1;
           }
           break;
           
      case 'r':
           a09->rndtests = true;
           break;
           
      case 't':
           a09->runtests = true;
           break;
           
      default:
           if (!a09->format.cmdline(&a09->format,a09,&arg,c))
           {
             fprintf(stderr,"unsupported option '%c'\n",c);
             return usage(argv[0]);
           }
           break;
    }
  }
  
  return arg_done(&arg);
}

/**************************************************************************/

static void warning_unused_symbols(struct a09 *a09,tree__s *tree)
{
  assert(a09 != NULL);
  
  if (tree != NULL)
  {
    warning_unused_symbols(a09,tree->left);
    struct symbol *sym = tree2sym(tree);
    if ((sym->refs == 0) && (sym->type == SYM_ADDRESS))
    {
      a09->lnum = sym->ldef;
      message(a09,MSG_WARNING,"W0002: symbol '%.*s' defined but not used",sym->name.s,sym->name.text);
    }
    warning_unused_symbols(a09,tree->right);
  }
}

/**************************************************************************/

static void dump_symbols(FILE *out,tree__s *tree)
{
  assert(out != NULL);
  
  if (tree != NULL)
  {
    static char const *const symtypes[] =
    {
      [SYM_UNDEF]   = "undefined",
      [SYM_ADDRESS] = "address",
      [SYM_EQU]     = "equate",
      [SYM_SET]     = "set",
      [SYM_PUBLIC]  = "public",
      [SYM_EXTERN]  = "extern",
    };
    
    dump_symbols(out,tree->left);
    struct symbol *sym = tree2sym(tree);
    if ((sym->type != SYM_SET) && (sym->refs > 0))
    {
      fprintf(
             out,
             "%5zu | %-11.11s %04X %5zu %.*s\n",
             sym->ldef,
             symtypes[sym->type],
             sym->value,
             sym->refs,
             sym->name.s,sym->name.text
          );
    }
    dump_symbols(out,tree->right);
  }
}

/**************************************************************************/

static int cleanup(struct a09 *a09,bool success)
{
  assert(a09 != NULL);
  
  if (a09->runtests)    test_fini(a09);
  if (a09->out != NULL) fclose(a09->out);
  if (a09->out != NULL) fclose(a09->in);
  
  if (!success)
  {
    if (a09->listfile) remove(a09->listfile);
    remove(a09->outfile);
  }
  
  a09->format.fini(&a09->format,a09);
  
  symbol_freetable(a09->symtab);
  for (size_t i = 0 ; i < a09->ndeps ; i++)
    free(a09->deps[i]);
  free(a09->deps);
  for (size_t i = 0 ; i < a09->nincs ; i++)
    free(a09->includes[i]);
  free(a09->includes);
  return success ? 0 : 1;
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  int        fi;
  bool       rc;
  struct a09 a09 =
  {
    .infile    = argv[0],
    .outfile   = "a09.obj",
    .listfile  = NULL,
    .corefile  = NULL,
    .tests     = NULL,
    .deps      = NULL,
    .includes  = NULL,
    .ndeps     = 0,
    .nincs     = 0,
    .in        = NULL,
    .out       = NULL,
    .list      = NULL,
    .lnum      = 0,
    .symtab    = NULL,
    .nowarn    = {0},
    .label     = { .s = 0, .text = { '\0' } },
    .pc        = 0,
    .dp        = 0,
    .debug     = false,
    .mkdeps    = false,
    .obj       = true,
    .runtests  = false,
    .rndtests  = false,
    .tapout    = false,
    .inbuf     = { .buf = {0}, .widx = 0, .ridx = 0 },
  };
  
  format_bin_init(&a09);
  fi = parse_command(argc,argv,&a09);
  
  if (fi == -1)
    return cleanup(&a09,false);
    
  if (fi == argc)
  {
    message(&a09,MSG_ERROR,"E0083: no input file specified");
    return cleanup(&a09,false);
  }
  
  if (a09.runtests)
    if (!test_init(&a09))
      return cleanup(&a09,false);
      
  a09.infile = add_file_dep(&a09,argv[fi]);
  a09.in     = fopen(a09.infile,"r");
  if (a09.in == NULL)
  {
    perror(a09.infile);
    return cleanup(&a09,false);
  }
  
  if (!assemble_pass(&a09,1))
    return cleanup(&a09,false);
    
  if (a09.mkdeps)
  {
    int len = printf("%s:",a09.outfile);
    
    for (size_t i = 0 ; i < a09.ndeps ; i++)
    {
      size_t fnlen = strlen(a09.deps[i]);
      if (fnlen + (unsigned)len > 77u)
      {
        printf(" \\\n ");
        len = 1;
      }
      len += printf(" %s",a09.deps[i]);
    }
    
    putchar('\n');
    return cleanup(&a09,true);
  }
  
  a09.out  = fopen(a09.outfile,"wb");
  
  if (a09.out == NULL)
  {
    perror(a09.outfile);
    return cleanup(&a09,false);
  }
  
  if (a09.listfile != NULL)
  {
    a09.list = fopen(a09.listfile,"w");
    if (a09.list == NULL)
    {
      perror(a09.listfile);
      return cleanup(&a09,false);
    }
    
    fprintf(a09.list,"                         | FILE %s\n",a09.infile);
  }
  
  /*-----------------------------------------------------------------------
  ; This initialization phase belongs here and not in assemble_pass().  We
  ; don't want to reset the PC or DP or global label when using INCLUDE.
  ;------------------------------------------------------------------------*/
  
  a09.pc    = 0;
  a09.dp    = 0;
  a09.label = (label){ .s = 0 , .text = { '\0' } };
  rc        = assemble_pass(&a09,2);
  
  message(&a09,MSG_DEBUG,"Post assembly phases");
  
  if (a09.runtests)
    test_run(&a09);
    
  if (rc)
    warning_unused_symbols(&a09,a09.symtab);
    
  if (a09.list != NULL)
  {
    fprintf(a09.list,"\n");
    dump_symbols(a09.list,a09.symtab);
    fclose(a09.list);
  }
  
  return cleanup(&a09,rc);
}
