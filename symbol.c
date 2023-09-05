
/* GPL3+ */

#include <string.h>
#include <stdlib.h>

#include "a09.h"

/**************************************************************************/

struct symbol *symbol_new(void)
{
  struct symbol *sym = malloc(sizeof(struct symbol));
  if (sym != NULL)
  {
    sym->tree.left    = NULL;
    sym->tree.right   = NULL;
    sym->tree.height  = 0;
    sym->node.ln_Succ = NULL;
    sym->node.ln_Pred = NULL;
    sym->name         = NULL;
    sym->value        = 0;
    sym->filename     = NULL;
    sym->ldef         = 0;
    sym->o16          = NULL;
    sym->o16m         = 0;
    sym->o16i         = 0;
    sym->o16d         = 4;
    sym->o8           = NULL;
    sym->o8m          = 0;
    sym->o8i          = 0;
    sym->o8d          = 4;
    sym->defined      = false;
    sym->external     = false;
    sym->equ          = false;
    sym->set          = false;
  }
  return sym;
}

/**************************************************************************/

static int symstrcmp(void const *restrict needle,void const *restrict haystack)
{
  char          const *key   = needle;
  struct symbol const *value = haystack;
  return strcmp(key,value->name);
}

/**************************************************************************/

static int symtreecmp(void const *restrict needle,void const *restrict haystack)
{
  struct symbol const *key   = needle;
  struct symbol const *value = haystack;
  return strcmp(key->name,value->name);
}

/**************************************************************************/

struct symbol *symbol_find(struct a09 *a09,char const *name)
{
  tree__s *tree = tree_find(a09->symtab,name,symstrcmp);

  if (tree)
    return tree2sym(tree);

  struct symbol *sym = symbol_new();
  if (sym != NULL)
  {
    sym->name   = strdup(name);
    a09->symtab = tree_insert(a09->symtab,&sym->tree,symtreecmp);
  }
  return sym;
}

/**************************************************************************/

struct symbol *symbol_add(struct a09 *a09,char const *name,uint16_t value)
{
  assert(a09  != NULL);
  assert(name != NULL);
  
  struct symbol *sym = symbol_find(a09,name);
  
  if (sym == NULL)
    return NULL;
  else if (!sym->defined)
  {
    sym->value    = value;
    sym->defined  = true;
    sym->filename = a09->infile;
    sym->ldef     = a09->lnum;
    ListAddTail(&a09->symbols,&sym->node);
    
    for (size_t i = 0 ; i < sym->o16i ; i++)
    {
      unsigned char buffer[2];
      
      buffer[0] = value >> 8;
      buffer[1] = value & 255;
      
      if (fseek(a09->out,sym->o16[i],SEEK_SET) != 0)
      {
        perror(a09->infile);
        return NULL;
      }
      
      if (fwrite(buffer,1,2,a09->out) != 2)
      {
        perror(a09->infile);
        return NULL;
      }
    }
    
    free(sym->o16);
    sym->o16  = NULL;
    sym->o16i = 0;
    sym->o16m = 0;
    sym->o16d = 4;
    
    {
      if (((value >> 8) != 255) && ((value >> 8) != 0))
        fprintf(stderr,"%s(%zu): warning: truncating '%s' to 8-bit value\n",a09->infile,a09->lnum,name);
    }
    
    for (size_t i = 0 ; i < sym->o8i ; i++)
    {
      unsigned char buffer[1];
      
      buffer[0] = value & 255;
      if (fseek(a09->out,sym->o8[i],SEEK_SET) != 0)
      {
        perror(a09->infile);
        return NULL;
      }
      
      if (fwrite(buffer,1,1,a09->out) != 1)
      {
        perror(a09->infile);
        return NULL;
      }
    }
    
    free(sym->o8);
    sym->o8  = NULL;
    sym->o8i = 0;
    sym->o8m = 0;
    sym->o8d = 4;
  }
  else
  {
    if (sym->set)
      sym->value = value;
    else
    {
      fprintf(stderr,"%s(%zu): error: '%s' already defined on line %zu\n",a09->infile,a09->lnum,name,sym->ldef);
      return NULL;
    }
  }
  
  return sym;
}

/**************************************************************************/

bool symbol_defer16(struct symbol *sym,long offset)
{
  assert(sym != NULL);
  
  if (sym->o16i == sym->o16m)
  {
    sym->o16d *= 2;
    long *n = realloc(sym->o16,sizeof(long) * sym->o16d);
    if (n == NULL)
      return false;
    sym->o16  = n;
    sym->o16m = sym->o16d;
  }
  
  sym->o16[sym->o16i++] = offset;
  return true;
}

/**************************************************************************/

bool symbol_defer8(struct symbol *sym,long offset)
{
  assert(sym != NULL);
  
  if (sym->o8i == sym->o8m)
  {
    sym->o8d *= 2;
    long *n = realloc(sym->o8,sizeof(long) * sym->o8d);
    if (n == NULL)
      return false;
    sym->o8  = n;
    sym->o8m = sym->o8d;
  }
  
  sym->o8[sym->o8i++] = offset;
  return true;
}

/**************************************************************************/
