
/* GPL3+ */

#ifndef I_CBED9C6E_E5CA_55A4_A099_890E6F362B3C
#define I_CBED9C6E_E5CA_55A4_A099_890E6F362B3C

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
  AM_OPERAND  = 0,
  AM_IMMED    = 0,
  AM_DIRECT,
  AM_INDEX,
  AM_EXTENDED,
  AM_INHERENT,
};

enum symtype
{
  SYM_UNDEF,
  SYM_ADDRESS,
  SYM_EQU,
  SYM_SET,
};

typedef struct label
{
  unsigned char s;
  char          text[63];
} label;

struct buffer
{
  char    buf[133];
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
  label          label;
  uint16_t       pc;
  bool           debug;
};

struct symbol
{
  tree__s       tree;
  Node          node;
  label         name;
  enum symtype  type;
  uint16_t      value;
  char const   *filename;
  size_t        ldef;
  size_t        bits;
  bool          external;
  bool          public;
};

struct value
{
  uint16_t      value;
  size_t        bits;
  unsigned char postbyte;
  bool          unknownpass1;
  bool          defined;
};

struct opcdata
{
  struct a09          *a09;
  struct opcode const *op;
  struct buffer       *buffer;
  label                label;
  int                  pass;
  uint16_t             sz;
  unsigned char        bytes[6];
  bool                 data;
  size_t               datasz;
  enum admode          mode;
  struct value         value;
  size_t               bits;
  bool                 pcrel;
  
  /*--------------------------------------------------------------------------
  ; This is solely for the use of the INCLUDE pseudo-op.  We want to include
  ; the line in the listing file before the included file appears in the
  ; listing file, but that's it.  This is here to prevent the INCLUDE line
  ; being listed twice.  If it's set, then print_list() won't print the
  ; line.
  ;--------------------------------------------------------------------------*/
  
  bool                 includehack;
};

struct opcode
{
  char            name[8];
  bool          (*func)(struct opcdata *);
  unsigned char   opcode[4];
  unsigned char   page;
  bool            bit16;
};

struct indexregs
{
  char          reg[3];
  unsigned char pushpull;
  unsigned char tehi;
  unsigned char telo;
  bool          b16;
};

/**************************************************************************/

extern char const MSG_DEBUG[];
extern char const MSG_WARNING[];
extern char const MSG_ERROR[];

extern bool                 message       (struct a09 *,char const *restrict,char const *restrict,...) __attribute__((format(printf,3,4)));
extern bool                 parse_label   (label *,struct buffer *,struct a09 *);
extern char                 skip_space    (struct buffer *);
extern bool                 print_list    (struct a09 *,struct opcdata *,bool);
extern bool                 assemble_pass (struct a09 *,int);
extern struct opcode const *op_find       (char const *);
extern bool                 expr          (struct value *,struct a09 *,struct buffer *,int);
extern struct symbol       *symbol_find   (struct a09 *,label const *);
extern struct symbol       *symbol_add    (struct a09 *,label const *,uint16_t);

/**************************************************************************/

static inline size_t min(size_t a,size_t b)
{
  return a < b ? a : b;
}

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
