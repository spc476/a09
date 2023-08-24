
/* GPL3+ */

#ifndef FOO
#define FOO

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

#include <stdbool.h>
#include <stdint.h>

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
  char const *filename;
  FILE       *in;
  FILE       *out;
  FILE       *list;
  size_t      lnum;
  tree__s    *symtab;
  char       *label;
  size_t      labelsize;
  bool        debug;
};

struct symbol
{
  tree__s     tree;
  char const *name;
  uint16_t    value;
  long       *o16;
  size_t      o16m;
  size_t      o16i;
  long       *o8;
  size_t      o8m;
  size_t      o8i;
  bool        defined;
  bool        external;
  bool        b8;
};

struct opcode
{
  char            name[6];
  bool          (*func)(struct opcode const *,struct a09 *);
  unsigned char   opcode[4];
  unsigned char   page;
};

extern bool                 set_namespace(struct a09 *,char const *);
extern bool                 read_label   (struct a09 *,char **,size_t *,int);
extern int                  skip_space   (FILE *);
extern bool                 skip_to_eoln (FILE *);
extern struct opcode const *op_find      (char const *);

#endif
