
/* GPL3+ */

#include <cgilib6/nodelist.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

static bool append_char(struct buffer *buffer,char c)
{
  if (buffer->widx == buffer->size)
  {
    buffer->size += 1024;
    char *n       = realloc(buffer->buf,buffer->size);
    if (n == NULL)
      return false;
    buffer->buf = n;
  }
  
  buffer->buf[buffer->widx] = c;
  if (c)
    buffer->widx++;
  return true;
}

/**************************************************************************/

static bool read_line(FILE *in,struct buffer *buffer)
{
  assert(in     != NULL);
  assert(buffer != NULL);
  
  buffer->widx = 0;
  buffer->ridx = 0;
  
  while(!feof(in))
  {
    int c = fgetc(in);
    if (c == EOF)  break;
    if (c == '\n') break;
    if (c == '\t')
    {
      for (size_t num = 8 - (buffer->widx & 7) , j = 0 ; j < num ; j++)
        if (!append_char(buffer,' '))
          return false;
    }
    else
    {
      if (!append_char(buffer,c))
        return false;
    }
  }
  
  return append_char(buffer,'\0');
}

/**************************************************************************/

bool read_label(struct buffer *buffer,char **plabel,size_t *plabelsize,char c)
{
  assert(buffer      != NULL);
  assert(plabel      != NULL);
  assert(plabelsize  != NULL);
  assert(*plabelsize == 0);
  
  char *nline = NULL;
  char *line  = NULL;
  size_t i     = 0;
  size_t max   = 0;
  
  while((c == '.') || (c == '_') || (c == '$') || isalpha(c) || isdigit(c))
  {
    if (i == max)
    {
      max += 64;
      nline = realloc(line,max);
      if (nline == NULL)
      {
        free(line);
        return false;
      }
      line = nline;
    }
    
    line[i++] = toupper(c);
    c = buffer->buf[buffer->ridx++];
  }
  
  line[i++] = '\0';
  nline     = realloc(line,i);
  
  if (nline != NULL)
    line = nline;
    
  if (buffer->ridx > 0)
    buffer->ridx--;
  
  *plabel     = line;
  *plabelsize = i - 1;
  return true;
}

/**************************************************************************/

static bool parse_label(char **plabel,struct buffer *buffer,struct a09 *a09)
{
  assert(plabel != NULL);
  assert(buffer != NULL);
  assert(a09    != NULL);
  
  char c = buffer->buf[buffer->ridx];

  if ((c == '.') || (c == '_') || isalpha(c))
  {
    char   *label     = NULL;
    char   *name      = NULL;
    size_t  labelsize = 0;
    
    buffer->ridx++;
    if (!read_label(buffer,&label,&labelsize,c))
      return false;
    
    if (*label == '.')
    {
      name = malloc(a09->labelsize + labelsize + 1);
      memcpy(name,a09->label,a09->labelsize);
      memcpy(&name[a09->labelsize],label,labelsize + 1);
    }
    else
    {
      name = malloc(labelsize + 1);
      memcpy(name,label,labelsize + 1);
      free(a09->label);
      a09->label = strdup(name);
    }
    
    free(label);
    *plabel = name;
    return true;
  }
  else
  {
    *plabel = NULL;
    return true;
  }
}

/**************************************************************************/

static bool parse_op(struct a09 *a09,struct buffer *buffer,struct opcode const **pop)
{
  assert(a09    != NULL);
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
    else if (!isalpha(c) && !isdigit(c))
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
  char c;
  
  do
  {
    c = buffer->buf[buffer->ridx++];
    if ((c == '\0') || (c == '\n'))
      return c;
  } while (isspace(c));
  
  return c;
}

/**************************************************************************/

static bool parse_line(struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  char                *label = NULL;
  struct opcode const *op;
  int                  c;
  
  if (!parse_label(&label,&a09->inbuf,a09))
  {
    assert(label == NULL);
    return false;
  }
  
  if (a09->debug)
    if (label != NULL)
      fprintf(stderr,"DEBUG: label='%s' line=%zu\n",label,a09->lnum);

  // XXX - do something with label
  
  c = skip_space(&a09->inbuf);
  
  if (c == '\0')
  {
    // XXX - add label?
    free(label);
    return true;
  }
  
  if (c == ';')
  {
    // XXX - add label?
    free(label);
    return true;
  }
  
  a09->inbuf.ridx--; // ungetc()
  
  if (!parse_op(a09,&a09->inbuf,&op))
  {
    fprintf(stderr,"%s(%zu): unknown opcode\n",a09->infile,a09->lnum);
    return false;
  }
  
  if (a09->debug)
    fprintf(stderr,"DEBUG: opcode='%s'\n",op->name);
  
  struct opcdata opd =
  {
    .a09 = a09,
    .op  = op,
    .buffer = &a09->inbuf,
    .label  = label,
    .pass   = pass,
    .sz     = 0,
  };
  bool rc = op->func(&opd);
  
  if (rc)
  {
    a09->pc += opd.sz;
    if ((pass == 2) && (opd.sz > 0))
    {
      size_t x = fwrite(opd.bytes,1,opd.sz,a09->out);
      if (x != opd.sz)
        rc = false;
    }
  }
  
  free(label);
  return rc;
}

/**************************************************************************/

static bool assemble_pass(struct a09 *a09,int pass)
{
  assert(a09     != NULL);
  assert(a09->in != NULL);
  assert(pass    >= 1);
  assert(pass    <= 2);
  
  if (a09->debug)
    fprintf(stderr,"Pass %d\n",pass);
    
  while(!feof(a09->in))
  {
    if (!read_line(a09->in,&a09->inbuf))
      return false;
    a09->lnum++;
    if (!parse_line(a09,&a09->inbuf,pass))
      return false;
  }
  
  return true;
}

/**************************************************************************/

static int parse_command(int argc,char *argv[],struct a09 *a09)
{
  int i;
  
  assert(argc >= 1);
  assert(argv != NULL);
  assert(a09  != NULL);
  
  for (i = 1 ; i < argc ; i++)
  {
    if (argv[i][0] == '-')
    {
      switch(argv[i][1])
      {
        case 'o':
             if (argv[i][2] == '\0')
               a09->outfile = argv[++i];
             else
               a09->outfile = &argv[i][2];
             break;
             
        case 'l':
             if (argv[i][2] == '\0')
               a09->listfile = argv[++i];
             else
               a09->listfile = &argv[i][2];
             break;
             
        case 'd':
             a09->debug = true;
             break;
             
        case 'h':
        default:
             fprintf(
                      stderr,
                      "usage: [options] [files...]\n"
                      "\t-o filename\toutput filename\n"
                      "\t-o listfile\tlist filename\n"
                      "\t-d\t\tdebug output\n"
                      "\t-h\t\thelp (this text)\n"
                    );
             exit(1);
      }
    }
    else
      break;    
  }
  
  return i;
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  int         fi;
  bool        rc;
  struct a09  a09 =
  {
    .infile    = NULL,
    .outfile   = "a09.obj",
    .listfile  = NULL,
    .in        = NULL,
    .out       = NULL,
    .list      = NULL,
    .lnum      = 0,
    .symtab    = NULL,
    .label     = NULL,
    .labelsize = 0,
    .pc        = 0,
    .debug     = false,
    .inbuf     =
    {
      .buf   = NULL,
      .size  = 0,
      .widx  = 0,
      .ridx  = 0,
    },
  };
  
  ListInit(&a09.symbols);
  fi = parse_command(argc,argv,&a09);
  a09.in = stdin;
  
  if (fi == argc)
  {
    fprintf(stderr,"no input file specified\n");
    exit(1);
  }
  
  a09.infile = argv[fi];
  a09.in = fopen(a09.infile,"r");
  if (a09.in == NULL)
  {
    perror(a09.infile);
    exit(1);
  }
  
  if (!assemble_pass(&a09,1))
    exit(1);
    
  rewind(a09.in);
  
  a09.pc  = 0;
  a09.out = fopen(a09.outfile,"wb");
  if (a09.out == NULL)
  {
    perror(a09.outfile);
    exit(1);
  }
  
  if (a09.listfile != NULL)
  {
    a09.list = fopen(a09.listfile,"w");
    if (a09.list == NULL)
    {
      perror(a09.listfile);
      exit(1);
    }
  }
  
  rc = assemble_pass(&a09,2);
  if (a09.list) fclose(a09.list);
  fclose(a09.out);
  fclose(a09.in);
  
  if (!rc)
  {
    if (a09.listfile) remove(a09.listfile);
    remove(a09.outfile);
  }
  
  for(
       Node *node = ListRemTail(&a09.symbols);
       NodeValid(node);
       node = ListRemTail(&a09.symbols)
     )
  {
    struct symbol *sym = node2sym(node);
    free(sym->name);
    free(sym);
  }
  
  free(a09.label);
  free(a09.inbuf.buf);
  return rc ? 0 : 1;
}
