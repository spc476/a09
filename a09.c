
/* GPL3+ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

static bool set_namespace(struct a09 *a09,char const *namespace)
{
  assert(a09       != NULL);
  assert(namespace != NULL);
  
  size_t len = strlen(namespace) + 1;
  a09->namespace = malloc(len);
  if (a09->namespace == NULL)
    return false;
  memcpy(a09->namespace,namespace,len);
  return true;
}

/**************************************************************************/

static bool read_label(struct a09 *a09,char **plabel,size_t *plabelsize,int c)
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
  assert(a09            != NULL);
  assert(a09->in        != NULL);
  assert(a09->namespace != NULL);
  assert(plabel         != NULL);
  
  int c = fgetc(a09->in);
  if ((c == '.') || (c == '_') || (c == '$') || isalpha(c))
  {
    char   *label = NULL;
    size_t  labelsize = 0;
    char   *name;
    size_t  len;
    
    if (!read_label(a09,&label,&labelsize,c))
      return false;
    
    if (*label == '.')
    {
      len  = a09->namespacelen + a09->labelsize + labelsize + 1;
      name = malloc(len);
      snprintf(name,len,"%s%s%s",a09->namespace,a09->label,label);
    }
    else
    {
      len  = a09->namespacelen + labelsize + 1;
      name = malloc(len);
      snprintf(name,len,"%s%s",a09->namespace,label);
      free(a09->label);
      a09->label     = name;
      a09->labelsize = len - 1;
    }
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

static bool parse_line(struct a09 *a09)
{
  char *label;
  int   c;
  
  if (!parse_label(a09,&label))
    return false;
  
  if (a09->debug)
    if (label != NULL)
      fprintf(stderr,"DEBUG: label='%s'\n",label);
      
  do
  {
    c = fgetc(a09->in);
  } while ((c != EOF) && (c != '\n'));
  
  return true;
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  struct a09           a09;
  //struct opcode const *op;
  int                  rc = 0;
  
  (void)argc;
  (void)argv;
  
  a09.debug     = true;
  a09.filename  = "";
  a09.in        = stdin;
  a09.out       = fopen("a09.out","wb");
  a09.list      = NULL;
  a09.symtab    = NULL;
  a09.label     = strdup("");
  a09.labelsize = 0;
  set_namespace(&a09,"");
  
  while(!feof(a09.in))
  {
    if (!parse_line(&a09))
    {
      rc = 1;
      break;
    }
  }
  
  free(a09.label);
  free(a09.namespace);
  fclose(a09.out);
  return rc;
}
