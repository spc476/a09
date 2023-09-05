
/* GPL3+ */

#ifndef FOO
#define FOO

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <cgilib6/nodelist.h>
#include <cgilib6/tree.h>

enum admode
{
  AM_IMMED,
  AM_DIRECT,
  AM_INDEX,
  AM_EXTENDED,
};

struct a09
{
  char const *infile;
  char const *outfile;
  char const *listfile;
  FILE       *in;
  FILE       *out;
  FILE       *list;
  size_t      lnum;
  tree__s    *symtab;
  List        symbols;
  char       *label;
  size_t      labelsize;
  bool        debug;
};

struct symbol
{
  tree__s     tree;
  Node        node;
  char       *name;
  uint16_t    value;
  char const *filename;
  size_t      ldef;
  long       *o16;
  size_t      o16m;
  size_t      o16i;
  size_t      o16d;
  long       *o8;
  size_t      o8m;
  size_t      o8i;
  size_t      o8d;
  bool        defined;
  bool        external;
  bool        equ;
  bool        set;
};

struct opcode
{
  char            name[6];
  bool          (*func)(struct opcode const *,char const *,struct a09 *);
  unsigned char   opcode[4];
  unsigned char   page;
};

extern bool                 set_namespace(struct a09 *,char const *);
extern bool                 read_label   (struct a09 *,char **,size_t *,int);
extern int                  skip_space   (FILE *);
extern bool                 skip_to_eoln (FILE *);
extern struct opcode const *op_find      (char const *);

extern struct symbol *symbol_new     (void);
extern struct symbol *symbol_find    (struct a09 *,char const *);
extern struct symbol *symbol_add     (struct a09 *,char const *,uint16_t);
extern bool           symbol_defer16 (struct symbol *,long);
extern bool           symbol_defer8  (struct symbol *,long);

/**************************************************************************/

static inline struct symbol *tree2sym(tree__s *tree)
{
  assert(tree != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diagnostic ignored "-Wcast-align"
#endif
  return (struct symbol *)((char *)tree - offsetof(struct symbol,tree));
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wcast-align"
#endif
}

/**************************************************************************/

static inline struct symbol *node2sym(Node *node)
{
  assert(node != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diagnostic ignored "-Wcast-align"
#endif
  return (struct symbol *)((char *)node - offsetof(struct symbol,node));
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wcast-align"
#endif
}

#endif
