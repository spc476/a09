
/* GPL3+ */

#ifndef FOO
#define FOO

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wconversion"
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
  tree__s    *symtab;
  char       *namespace;
  size_t      namespacelen;
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

struct opcode const *op_find(char const *);

#endif
