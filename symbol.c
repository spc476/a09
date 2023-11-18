/****************************************************************************
*
*   Symbol table management
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

#include <string.h>
#include <stdlib.h>

#include "a09.h"

/**************************************************************************/

static int symstrcmp(void const *restrict needle,void const *restrict haystack)
{
  label         const *key   = needle;
  struct symbol const *value = haystack;
  int                  rc    = memcmp(key->text,value->name.text,min(key->s,value->name.s));
  
  if (rc == 0)
  {
    if (key->s < value->name.s)
      rc = -1;
    else if (key->s > value->name.s)
      rc = 1;
  }
  return rc;
}

/**************************************************************************/

static int symtreecmp(void const *restrict needle,void const *restrict haystack)
{
  struct symbol const *key   = needle;
  struct symbol const *value = haystack;
  int                  rc    = memcmp(key->name.text,value->name.text,min(key->name.s,value->name.s));
  
  if (rc == 0)
  {
    if (key->name.s < value->name.s)
      rc = -1;
    else if (key->name.s > value->name.s)
      rc = 1;
  }
  return rc;
}

/**************************************************************************/

struct symbol *symbol_find(struct a09 *a09,label const *name)
{
  tree__s *tree = tree_find(a09->symtab,name,symstrcmp);
  
  if (tree)
    return tree2sym(tree);
  else
    return NULL;
}

/**************************************************************************/

struct symbol *symbol_add(struct a09 *a09,label const *name,uint16_t value)
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
      sym->name         = *name;
      sym->type         = SYM_ADDRESS;
      sym->value        = value;
      sym->filename     = a09->infile;
      sym->ldef         = a09->lnum;
      sym->bits         = a09->dp == value >> 8 ? 8 : 16;
      sym->refs         = 0;
      a09->symtab       = tree_insert(a09->symtab,&sym->tree,symtreecmp);
      ListAddTail(&a09->symbols,&sym->node);
    }
  }
  else if (sym->type == SYM_SET)
  {
    sym->value    = value;
    sym->filename = a09->infile;
    sym->ldef     = a09->lnum;
    sym->bits     = value < 256 ? 8 : 16;
  }
  else
  {
    message(a09,MSG_ERROR,"E0049: '%.*s' already defined on line %zu",name->s,name->text,sym->ldef);
    return NULL;
  }
  
  return sym;
}

/**************************************************************************/
