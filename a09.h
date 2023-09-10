
/* GPL3+ */

#ifndef FOO
#define FOO

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wconversion"
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
  AM_OPERAND  = 0,
  AM_IMMED    = 0,
  AM_DIRECT,
  AM_INDEX,
  AM_EXTENDED,
  AM_INHERENT,
};

struct buffer
{
  char   *buf;
  size_t  size;
  size_t  widx;
  size_t  ridx;
};

struct a09
{
  char const    *infile;
  char const    *outfile;
  char const    *listfile;
  FILE          *in;
  FILE          *out;
  FILE          *list;
  struct buffer  inbuf;
  size_t         lnum;
  tree__s       *symtab;
  List           symbols;
  char          *label;
  size_t         labelsize;
  uint16_t       pc;
  bool           debug;
};

struct symbol
{
  tree__s     tree;
  Node        node;
  char       *name;
  uint16_t    value;
  char const *filename;
  size_t      ldef;
  bool        external;
  bool        equ;
  bool        set;
};

struct opcdata
{
  struct a09          *a09;
  struct opcode const *op;
  struct buffer       *buffer;
  char                *label;
  int                  pass;
  uint16_t             sz;
  unsigned char        bytes[5];
  bool                 data;
  enum admode          mode;
  uint16_t             value;
  int                  ireg;
  size_t               bits;
};

struct opcode
{
  char            name[8];
  bool          (*func)(struct opcdata *);
  unsigned char   opcode[4];
  unsigned char   page;
  bool            bit16;
};


extern char const MSG_DEBUG[];
extern char const MSG_WARNING[];
extern char const MSG_ERROR[];

extern bool                 message      (struct a09 *,char const *restrict,char const *restrict,...);
extern bool                 read_label   (struct buffer *,char **,size_t *,char);
extern char                 skip_space   (struct buffer *);
extern struct opcode const *op_find      (char const *);

extern bool           expr        (uint16_t *,struct a09 *,struct buffer *,int);

extern struct symbol *symbol_find (struct a09 *,char const *);
extern struct symbol *symbol_add  (struct a09 *,char const *,uint16_t);

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

/**************************************************************************/

#endif
