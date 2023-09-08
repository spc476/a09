
/* GPL3+ */

#include <string.h>
#include <stdlib.h>

#include "a09.h"

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
  else
    return NULL;    
}

/**************************************************************************/

struct symbol *symbol_add(struct a09 *a09,char const *name,uint16_t value)
{
  assert(a09  != NULL);
  assert(name != NULL);
  
  struct symbol *sym = symbol_find(a09,name);
  
  if (sym == NULL)
  {
    sym = malloc(sizeof(struct symbol));
    if (sym != NULL)
    {
      sym->tree.left    = NULL;
      sym->tree.right   = NULL;
      sym->tree.height  = 0;
      sym->node.ln_Succ = NULL;
      sym->node.ln_Pred = NULL;
      sym->name         = strdup(name);
      sym->value        = value;
      sym->filename     = a09->infile;
      sym->ldef         = a09->lnum;
      sym->external     = false;
      sym->equ          = false;
      sym->set          = false;
      ListAddTail(&a09->symbols,&sym->node);
      a09->symtab = tree_insert(a09->symtab,&sym->tree,symtreecmp);
    }
  }
  else if (sym->set)
  {
    sym->value    = value;
    sym->filename = a09->infile;
    sym->ldef     = a09->lnum;
  }    
  else
  {
    message(a09,MSG_ERROR,"'%s' already defined on line %zu",name,sym->ldef);
    return NULL;
  }
  
  return sym;
}

/**************************************************************************/
