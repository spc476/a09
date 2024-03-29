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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "a09.h"

/**************************************************************************/

int symstrcmp(void const *restrict needle,void const *restrict haystack)
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

struct symbol *symbol_add(struct a09 *a09,label const *name,uint16_t value)
{
  assert(a09  != NULL);
  assert(name != NULL);
  
  if (
          ((name->s == 1) && (toupper(name->text[0]) == 'A'))
       || ((name->s == 1) && (toupper(name->text[0]) == 'B'))
       || ((name->s == 1) && (toupper(name->text[0]) == 'D'))
     )
    message(a09,MSG_WARNING,"W0013: label '%.*s' could be mistaken for register in index",name->s,name->text);
    
  struct symbol *sym = symbol_find(a09,name);
  
  if (sym == NULL)
  {
    sym = malloc(sizeof(struct symbol));
    if (sym != NULL)
    {
      sym->tree.left   = NULL;
      sym->tree.right  = NULL;
      sym->tree.height = 0;
      sym->name        = *name;
      sym->type        = SYM_ADDRESS;
      sym->value       = value;
      sym->filename    = a09->infile;
      sym->ldef        = a09->lnum;
      sym->bits        = a09->dp == value >> 8 ? 8 : 16;
      sym->refs        = 0;
      a09->symtab      = tree_insert(a09->symtab,&sym->tree,symtreecmp);
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

void symbol_freetable(tree__s *tree)
{
  if (tree != NULL)
  {
    symbol_freetable(tree->left);
    symbol_freetable(tree->right);
    free(tree);
  }
}

/**************************************************************************/
