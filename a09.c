
/* GPL3+ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

bool read_label(struct a09 *a09,char **plabel,size_t *plabelsize,int c)
{
  assert(a09         != NULL);
  assert(a09->in     != NULL);
  assert(plabel      != NULL);
  assert(plabelsize  != NULL);
  assert(*plabelsize == 0);
  
  char *nline = NULL;
  char *line  = NULL;
  size_t i    = 0;
  size_t max  = 0;
  
  while((c == '.') || (c == '_') || (c == '$') || isalpha(c) || isdigit(c))
  {
    if (i == max)
    {
      max   += 64;
      nline  = realloc(line,max);
      if (nline == NULL)
      {
        perror(a09->filename);
        free(line);
        return false;
      }
      line = nline;
    }
    
    line[i++] = toupper(c);
    c = fgetc(a09->in);
  }
  
  line[i++] = '\0';
  nline     = realloc(line,i);
  
  if (nline != NULL)
    line = nline;
  
  if (c != EOF)
    ungetc(c,a09->in);
  
  *plabel     = line;
  *plabelsize = i - 1;
  return true;
}

/**************************************************************************/

static bool parse_label(struct a09 *a09,char **plabel)
{
  assert(a09     != NULL);
  assert(a09->in != NULL);
  assert(plabel  != NULL);
  
  int c = fgetc(a09->in);
  if ((c == '.') || (c == '_') || isalpha(c))
  {
    char   *label = NULL;
    size_t  labelsize = 0;
    char   *name = NULL;
    
    if (!read_label(a09,&label,&labelsize,c))
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
    if (c != EOF)
      ungetc(c,a09->in);
    *plabel = NULL;
    return true;
  }
}

/**************************************************************************/

static bool parse_op(struct a09 *a09,struct opcode const **pop)
{
  assert(a09 != NULL);
  assert(pop != NULL);
  
  char top[10];
  int  c = 0;
  
  for (size_t i = 0 ; i < 10 ; i++)
  {
    c = fgetc(a09->in);
    if (c == EOF) return false;
    if (isspace(c))
    {
      ungetc(c,a09->in);
      top[i] = '\0';
      *pop   = op_find(top);
      return *pop != NULL;
    }
    else if (!isalpha(c) && !isdigit(c))
      break;

    top[i] = toupper(c);
  }
  
  assert(c != 0);
  ungetc(c,a09->in);
  return false;
}

/**************************************************************************/

int skip_space(FILE *in)
{
  int c;
  
  do
  {
    c = fgetc(in);
    if ((c == EOF) || (c == '\n'))
      return c;
  } while (isspace(c));
  
  return c;
}

/**************************************************************************/

bool skip_to_eoln(FILE *in)
{
  int c;
  
  do
    c = fgetc(in);
  while ((c != EOF) && (c != '\n'));
  return true;
}

/**************************************************************************/

static bool parse_line(struct a09 *a09)
{
  assert(a09 != NULL);
  
  char                *label = NULL;
  struct opcode const *op;
  int                  c;
  
  a09->lnum++;
  if (!parse_label(a09,&label))
  {
    assert(label == NULL);
    return false;
  }
  
  if (a09->debug)
    if (label != NULL)
      fprintf(stderr,"DEBUG: label='%s' line=%zu\n",label,a09->lnum);

  // XXX - do something with label
  
  c = skip_space(a09->in);
  
  if (c == EOF)
  {
    // XXX - add label?
    free(label);
    return true;
  }
  
  if (c == '\n')
  {
    // XXX - add label?
    free(label);
    return true;
  }
  
  if (c == ';')
  {
    // XXX - add label?
    free(label);
    return skip_to_eoln(a09->in);
  }
  
  ungetc(c,a09->in);
  
  if (!parse_op(a09,&op))
  {
    fprintf(stderr,"%s(%zu): unknown opcode\n",a09->filename,a09->lnum);
    return false;
  }
  
  if (a09->debug)
    fprintf(stderr,"DEBUG: opcode='%s'\n",op->name);
  
  op->func(op,label,a09);
  free(label);
  return skip_to_eoln(a09->in);
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  struct a09 a09;
  int        rc = 0;
  
  (void)argc;
  (void)argv;
  
  a09.debug     = false;
  a09.filename  = "(stdin)";
  a09.in        = stdin;
  a09.out       = fopen("a09.out","wb");
  a09.lnum      = 0;
  a09.list      = NULL;
  a09.symtab    = NULL;
  a09.label     = strdup("");
  a09.labelsize = 0;
  
  while(!feof(a09.in))
  {
    if (!parse_line(&a09))
    {
      rc = 1;
      break;
    }
  }
  
  free(a09.label);
  fclose(a09.out);
  return rc;
}
